/* Detailed client-side network diagnostics. */
#ifndef EZQUAKE_NETLOG_H
#define EZQUAKE_NETLOG_H

#include "net.h"

void Netlog_Init(void);
void Netlog_Shutdown(void);
void Netlog_Frame(void);
void Netlog_ConnectionEvent(const char *event, const netadr_t *address);
void Netlog_RawPacket(const char *direction, const char *transport, const netadr_t *address, int bytes, int error_code);
void Netlog_DelayQueue(const char *direction, const netadr_t *address, int bytes, double delay_ms, qbool accepted);
void Netlog_NetchanTransmit(const netchan_t *chan, unsigned sequence, unsigned acknowledge,
	qbool reliable, int payload_bytes, int packet_bytes);
void Netlog_NetchanReceive(const netchan_t *chan, unsigned sequence, unsigned acknowledge,
	qbool reliable_message, qbool reliable_ack, int packet_bytes, qbool accepted,
	int expected_sequence, int gap);
void Netlog_ClientMove(int sequence, int loss_percent, int bytes, qbool suppressed,
	float pps_balance, int consecutive_suppressed, qbool protected_impulse);
void Netlog_ClientAck(int sequence, double sent_time, double received_time, int bytes);
void Netlog_ServerChoke(int acknowledge, int reported, int marked);
void Netlog_LossCalculated(int percent, int lost, int samples);

#endif
