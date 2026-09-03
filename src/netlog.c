/*
 * Detailed client-side network diagnostics.
 *
 * Packet payloads are deliberately not recorded: sequence information,
 * timing, sizes and local settings are enough to diagnose packet loss while
 * avoiding accidental capture of chat, passwords and userinfo.
 */
#include <time.h>

#include "quakedef.h"
#include "netlog.h"
#include "version.h"

#define NETLOG_BUFFER_SIZE (64 * 1024)

extern cvar_t rate, mtu;
extern cvar_t cl_maxfps, cl_independentPhysics;
extern cvar_t cl_delay_packet, cl_delay_packet_target, cl_delay_packet_dev;
extern cvar_t cl_earlypackets;
extern cvar_t cl_c2spps, cl_c2sImpulseBackup, cl_c2sdupe, cl_nodelta;

static void Netlog_OnChange(cvar_t *var, char *value, qbool *cancel);

cvar_t netlog = { "netlog", "0", 0, Netlog_OnChange };

static FILE *netlog_file;
static char netlog_path[MAX_OSPATH];
static double netlog_started;
static double netlog_last_flush;
static double netlog_last_summary;
static int netlog_last_loss = -1;
static unsigned long netlog_raw_rx_packets;
static unsigned long netlog_raw_tx_packets;
static unsigned long netlog_raw_rx_bytes;
static unsigned long netlog_raw_tx_bytes;
static unsigned long netlog_netchan_gaps;
static unsigned long netlog_out_of_order;
static unsigned long netlog_queue_drops;
static unsigned long netlog_local_suppressed;
static unsigned long netlog_server_choked;
static int netlog_socket_reported = INVALID_SOCKET;

static qbool Netlog_IsActive(void)
{
	return netlog_file != NULL;
}

static const char *Netlog_Address(const netadr_t *address)
{
	return address ? NET_AdrToString(*address) : "-";
}

static void Netlog_Write(const char *event, const char *format, ...)
{
	va_list args;

	if (!netlog_file)
		return;

	fprintf(netlog_file, "%.6f %-14s ", Sys_DoubleTime() - netlog_started, event);
	va_start(args, format);
	vfprintf(netlog_file, format, args);
	va_end(args);
	fputc('\n', netlog_file);
}

static void Netlog_WriteSettings(void)
{
	Netlog_Write("settings",
		"rate=%s mtu=%s cl_maxfps=%s cl_independentPhysics=%s cl_c2spps=%s cl_c2sdupe=%s cl_c2sImpulseBackup=%s cl_nodelta=%s",
		rate.string, mtu.string, cl_maxfps.string, cl_independentPhysics.string,
		cl_c2spps.string, cl_c2sdupe.string, cl_c2sImpulseBackup.string, cl_nodelta.string);
	Netlog_Write("settings",
		"cl_delay_packet=%s cl_delay_packet_target=%s cl_delay_packet_dev=%s cl_earlypackets=%s",
		cl_delay_packet.string, cl_delay_packet_target.string,
		cl_delay_packet_dev.string, cl_earlypackets.string);
}

static qbool Netlog_Open(void)
{
	time_t now = time(NULL);
	struct tm local_time;
	char directory[MAX_OSPATH];
	char filename_stem[48];
	char filename[64];
	FILE *existing;
	int suffix = 0;

#ifdef _WIN32
	localtime_s(&local_time, &now);
#else
	localtime_r(&now, &local_time);
#endif

	snprintf(directory, sizeof(directory), "%s/qw/netlogs", com_basedir);
	Sys_mkdir(directory);
	strftime(filename_stem, sizeof(filename_stem), "netlog-%Y%m%d-%H%M%S", &local_time);
	do {
		if (suffix)
			snprintf(filename, sizeof(filename), "%s-%02d.txt", filename_stem, suffix);
		else
			snprintf(filename, sizeof(filename), "%s.txt", filename_stem);
		snprintf(netlog_path, sizeof(netlog_path), "%s/%s", directory, filename);
		existing = fopen(netlog_path, "r");
		if (existing)
			fclose(existing);
		++suffix;
	} while (existing && suffix < 100);

	netlog_file = fopen(netlog_path, "w");
	if (!netlog_file) {
		Com_Printf_State(PRINT_FAIL, "Netlog: could not open %s (%s)\n", netlog_path, strerror(errno));
		netlog_path[0] = '\0';
		return false;
	}

	setvbuf(netlog_file, NULL, _IOFBF, NETLOG_BUFFER_SIZE);
	netlog_started = Sys_DoubleTime();
	netlog_last_flush = netlog_last_summary = netlog_started;
	netlog_last_loss = -1;
	netlog_raw_rx_packets = netlog_raw_tx_packets = 0;
	netlog_raw_rx_bytes = netlog_raw_tx_bytes = 0;
	netlog_netchan_gaps = netlog_out_of_order = 0;
	netlog_queue_drops = netlog_local_suppressed = netlog_server_choked = 0;
	netlog_socket_reported = INVALID_SOCKET;

	fprintf(netlog_file, "# ezquake-tf network diagnostic log\n");
	fprintf(netlog_file, "# version=%s build=%s commit=%s started=%04d-%02d-%02d %02d:%02d:%02d\n",
		EZQUAKE_TF_RELEASE_VERSION, VERSION, GIT_COMMIT_DATETIME,
		local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday,
		local_time.tm_hour, local_time.tm_min, local_time.tm_sec);
	fprintf(netlog_file, "# payloads are not recorded; timestamps are seconds since logging started\n");
	Netlog_WriteSettings();
	if (cls.state != ca_disconnected)
		Netlog_ConnectionEvent("enabled", &cls.netchan.remote_address);
	fflush(netlog_file);

	Com_Printf_State(PRINT_OK, "Netlog enabled: %s\n", netlog_path);
	Com_Printf("Use netlog 0 to stop it; the file can grow quickly.\n");
	return true;
}

static void Netlog_Close(const char *reason)
{
	if (!netlog_file)
		return;

	Netlog_Write("stop", "reason=%s rx_packets=%lu tx_packets=%lu rx_bytes=%lu tx_bytes=%lu gaps=%lu out_of_order=%lu queue_drops=%lu local_suppressed=%lu server_choked=%lu",
		reason, netlog_raw_rx_packets, netlog_raw_tx_packets,
		netlog_raw_rx_bytes, netlog_raw_tx_bytes, netlog_netchan_gaps,
		netlog_out_of_order, netlog_queue_drops, netlog_local_suppressed,
		netlog_server_choked);
	fflush(netlog_file);
	fclose(netlog_file);
	netlog_file = NULL;
	Com_Printf("Netlog stopped: %s\n", netlog_path);
}

static void Netlog_OnChange(cvar_t *var, char *value, qbool *cancel)
{
	qbool enable = atoi(value) != 0;
	(void)var;

	if (enable == Netlog_IsActive())
		return;

	if (enable) {
		if (!Netlog_Open())
			*cancel = true;
	}
	else {
		Netlog_Close("disabled");
	}
}

static void Netlog_Status_f(void)
{
	if (!Netlog_IsActive()) {
		Com_Printf("Netlog is disabled. Enable it with: netlog 1\n");
		return;
	}

	Com_Printf("Netlog: %s\n", netlog_path);
	Com_Printf("RX %lu packets/%lu bytes, TX %lu packets/%lu bytes, gaps %lu, out-of-order %lu\n",
		netlog_raw_rx_packets, netlog_raw_rx_bytes, netlog_raw_tx_packets,
		netlog_raw_tx_bytes, netlog_netchan_gaps, netlog_out_of_order);
}

void Netlog_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&netlog);
	Cvar_ResetCurrentGroup();
	Cmd_AddCommand("netlog_status", Netlog_Status_f);
}

void Netlog_Shutdown(void)
{
	Netlog_Close("client_shutdown");
}

void Netlog_Frame(void)
{
	double now;
	net_stat_result_t stats;
	int receive_buffer = 0;
	int send_buffer = 0;
#ifdef _WIN32
	int option_length;
#else
	socklen_t option_length;
#endif

	if (!netlog_file)
		return;

	now = Sys_DoubleTime();
	if (cls.socketip != INVALID_SOCKET && cls.socketip != netlog_socket_reported) {
		option_length = sizeof(receive_buffer);
		getsockopt(cls.socketip, SOL_SOCKET, SO_RCVBUF, (char *)&receive_buffer, &option_length);
		option_length = sizeof(send_buffer);
		getsockopt(cls.socketip, SOL_SOCKET, SO_SNDBUF, (char *)&send_buffer, &option_length);
		Netlog_Write("socket", "handle=%d local=%s receive_buffer=%d send_buffer=%d",
			cls.socketip, NET_AdrToString(net_local_cl_ipadr), receive_buffer, send_buffer);
		netlog_socket_reported = cls.socketip;
	}
	if (cls.frametime > 0.050)
		Netlog_Write("frame_stall", "frametime_ms=%.3f state=%d incoming=%d outgoing=%d",
			cls.frametime * 1000.0, cls.state, cls.netchan.incoming_sequence,
			cls.netchan.outgoing_sequence);

	if (now - netlog_last_summary >= 1.0) {
		memset(&stats, 0, sizeof(stats));
		if (cls.state >= ca_connected && CL_CalcNetStatistics(1.0f, network_stats, NETWORK_STATS_SIZE, &stats) > 0) {
			Netlog_Write("summary", "state=%d pl=%d ping_ms=%.2f/%.2f/%.2f jitter=%.2f loss=%.2f choke=%.2f delta=%.2f netlimit=%.2f bw_in=%.0f bw_out=%.0f samples=%d",
				cls.state, CL_CalcNet(), stats.ping_min, stats.ping_avg, stats.ping_max,
				stats.ping_dev, stats.lost_lost, stats.lost_rate, stats.lost_delta,
				stats.lost_netlimit, stats.bandwidth_in, stats.bandwidth_out, stats.samples);
		}
		else {
			Netlog_Write("summary", "state=%d server=%s incoming=%d outgoing=%d latency_ms=%.2f",
				cls.state, cls.servername[0] ? cls.servername : "-",
				cls.netchan.incoming_sequence, cls.netchan.outgoing_sequence,
				cls.latency * 1000.0);
		}
		netlog_last_summary = now;
	}

	if (now - netlog_last_flush >= 1.0) {
		fflush(netlog_file);
		netlog_last_flush = now;
	}
}

void Netlog_ConnectionEvent(const char *event, const netadr_t *address)
{
	Netlog_Write("connection", "event=%s address=%s state=%d server=%s",
		event, Netlog_Address(address), cls.state, cls.servername[0] ? cls.servername : "-");
	if (!strcmp(event, "connected"))
		Netlog_WriteSettings();
}

void Netlog_RawPacket(const char *direction, const char *transport, const netadr_t *address, int bytes, int error_code)
{
	if (!netlog_file)
		return;

	if (direction[0] == 'R') {
		++netlog_raw_rx_packets;
		if (bytes > 0)
			netlog_raw_rx_bytes += bytes;
	}
	else {
		++netlog_raw_tx_packets;
		if (bytes > 0)
			netlog_raw_tx_bytes += bytes;
	}

	Netlog_Write("raw_packet", "dir=%s transport=%s address=%s bytes=%d error=%d state=%d",
		direction, transport, Netlog_Address(address), bytes, error_code, cls.state);
	if (error_code)
		fflush(netlog_file);
}

void Netlog_DelayQueue(const char *direction, const netadr_t *address, int bytes, double delay_ms, qbool accepted)
{
	if (!accepted)
		++netlog_queue_drops;
	Netlog_Write("delay_queue", "dir=%s address=%s bytes=%d delay_ms=%.3f result=%s",
		direction, Netlog_Address(address), bytes, delay_ms, accepted ? "queued" : "full_drop");
}

void Netlog_NetchanTransmit(const netchan_t *chan, unsigned sequence, unsigned acknowledge,
	qbool reliable, int payload_bytes, int packet_bytes)
{
	Netlog_Write("netchan_tx", "address=%s seq=%u ack=%u reliable=%d payload=%d packet=%d dupe=%d reliable_pending=%d cleartime_delta_ms=%.3f",
		Netlog_Address(&chan->remote_address), sequence, acknowledge, reliable,
		payload_bytes, packet_bytes, chan->dupe, chan->reliable_length,
		(chan->cleartime - curtime) * 1000.0);
}

void Netlog_NetchanReceive(const netchan_t *chan, unsigned sequence, unsigned acknowledge,
	qbool reliable_message, qbool reliable_ack, int packet_bytes, qbool accepted,
	int expected_sequence, int gap)
{
	if (!accepted)
		++netlog_out_of_order;
	else if (gap > 0)
		netlog_netchan_gaps += gap;

	Netlog_Write("netchan_rx", "address=%s seq=%u ack=%u reliable=%d reliable_ack=%d packet=%d expected=%d gap=%d result=%s",
		Netlog_Address(&chan->remote_address), sequence, acknowledge, reliable_message,
		reliable_ack, packet_bytes, expected_sequence, gap,
		accepted ? "accepted" : "stale_or_duplicate");
	if (!accepted || gap > 0)
		fflush(netlog_file);
}

void Netlog_ClientMove(int sequence, int loss_percent, int bytes, qbool suppressed,
	float pps_balance, int consecutive_suppressed, qbool protected_impulse)
{
	if (suppressed)
		++netlog_local_suppressed;
	Netlog_Write("client_move", "seq=%d advertised_pl=%d bytes=%d result=%s cl_c2spps=%s balance=%.6f consecutive_suppressed=%d protected_impulse=%d",
		sequence, loss_percent, bytes, suppressed ? "local_suppressed" : "send",
		cl_c2spps.string, pps_balance, consecutive_suppressed, protected_impulse);
}

void Netlog_ClientAck(int sequence, double sent_time, double received_time, int bytes)
{
	Netlog_Write("client_ack", "seq=%d sent=%.6f received=%.6f rtt_ms=%.3f bytes=%d outgoing_now=%d",
		sequence, sent_time, received_time, (received_time - sent_time) * 1000.0,
		bytes, cls.netchan.outgoing_sequence);
}

void Netlog_ServerChoke(int acknowledge, int reported, int marked)
{
	netlog_server_choked += marked;
	Netlog_Write("server_choke", "ack=%d reported=%d marked=%d", acknowledge, reported, marked);
	fflush(netlog_file);
}

void Netlog_LossCalculated(int percent, int lost, int samples)
{
	if (!netlog_file)
		return;
	if (percent != netlog_last_loss || lost > 0) {
		Netlog_Write("pl_calculated", "percent=%d dropped=%d samples=%d incoming=%d outgoing=%d latency_ms=%.3f",
			percent, lost, samples, cls.netchan.incoming_sequence,
			cls.netchan.outgoing_sequence, cls.latency * 1000.0);
		netlog_last_loss = percent;
	}
}
