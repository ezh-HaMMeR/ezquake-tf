#include "quakedef.h"
#include "update_client.h"
#include "update_common.h"
#include "menu.h"
#include "version.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <direct.h>
#include <curl/curl.h>
#include <jansson.h>
#include <SDL.h>

#pragma comment(lib, "bcrypt.lib")

#define UPDATE_ASSET_NAME "ezquake-tf-windows-x64.zip"
#define UPDATE_API_URL "https://api.github.com/repos/ezh-HaMMeR/ezquake-tf/releases/latest"
#define UPDATE_PATH_CAPACITY 4096
#define UPDATE_MESSAGE_CAPACITY 512

typedef enum update_state_e {
	UPDATE_IDLE,
	UPDATE_CHECKING,
	UPDATE_CURRENT,
	UPDATE_AVAILABLE,
	UPDATE_DOWNLOADING,
	UPDATE_READY,
	UPDATE_INSTALLING,
	UPDATE_FAILED
} update_state_t;

typedef struct update_memory_s {
	char *data;
	size_t size;
} update_memory_t;

typedef struct update_context_s {
	SDL_mutex *mutex;
	SDL_Thread *thread;
	update_state_t state;
	int thread_done;
	int cancel_requested;
	int notification_pending;
	char version[128];
	char api_url[2048];
	char asset_url[2048];
	char release_url[2048];
	char sha256[65];
	char archive[UPDATE_PATH_CAPACITY];
	char message[UPDATE_MESSAGE_CAPACITY];
} update_context_t;

static update_context_t update_context;
static cvar_t autoupdate = {"autoupdate", "1"};

static size_t Update_MemoryWrite(void *contents, size_t size, size_t count, void *user)
{
	update_memory_t *memory = (update_memory_t *)user;
	size_t bytes = size * count;
	char *next = (char *)realloc(memory->data, memory->size + bytes + 1);
	if (!next)
		return 0;
	memory->data = next;
	memcpy(memory->data + memory->size, contents, bytes);
	memory->size += bytes;
	memory->data[memory->size] = '\0';
	return bytes;
}

static size_t Update_FileWrite(void *contents, size_t size, size_t count, void *user)
{
	return fwrite(contents, size, count, (FILE *)user);
}

static void Update_SetResult(update_state_t state, const char *message)
{
	SDL_LockMutex(update_context.mutex);
	update_context.state = state;
	update_context.thread_done = 1;
	update_context.notification_pending = 1;
	strlcpy(update_context.message, message ? message : "", sizeof(update_context.message));
	SDL_UnlockMutex(update_context.mutex);
}

static CURLcode Update_ConfigureCurl(CURL *curl, const char *url)
{
	CURLcode result = CURLE_OK;
	result |= curl_easy_setopt(curl, CURLOPT_URL, url);
	result |= curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	result |= curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	result |= curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	result |= curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
	result |= curl_easy_setopt(curl, CURLOPT_USERAGENT, "ezquake-tf-updater/1");
	return result;
}

static int Update_Progress(void *user, curl_off_t download_total, curl_off_t download_now,
	curl_off_t upload_total, curl_off_t upload_now)
{
	int cancel;
	(void)user; (void)download_total; (void)download_now; (void)upload_total; (void)upload_now;
	SDL_LockMutex(update_context.mutex);
	cancel = update_context.cancel_requested;
	SDL_UnlockMutex(update_context.mutex);
	return cancel;
}

static void Update_EnableCancellation(CURL *curl)
{
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Update_Progress);
}

static int Update_CheckThread(void *unused)
{
	CURL *curl = NULL;
	CURLcode result;
	update_memory_t memory;
	json_error_t error;
	json_t *root = NULL, *assets, *asset;
	const char *tag, *release_url, *asset_url = NULL, *digest = NULL;
	size_t i;
	char message[UPDATE_MESSAGE_CAPACITY];
	char api_url[2048];
	(void)unused;

	memset(&memory, 0, sizeof(memory));
	SDL_LockMutex(update_context.mutex);
	strlcpy(api_url, update_context.api_url, sizeof(api_url));
	SDL_UnlockMutex(update_context.mutex);
	curl = curl_easy_init();
	if (!curl || Update_ConfigureCurl(curl, api_url) != CURLE_OK)
		goto network_error;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Update_MemoryWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &memory);
	Update_EnableCancellation(curl);
	result = curl_easy_perform(curl);
	if (result != CURLE_OK)
		goto network_error;
	root = json_loadb(memory.data, memory.size, JSON_REJECT_DUPLICATES, &error);
	if (!root || !json_is_object(root))
		goto data_error;
	tag = json_string_value(json_object_get(root, "tag_name"));
	release_url = json_string_value(json_object_get(root, "html_url"));
	assets = json_object_get(root, "assets");
	if (!tag || !json_is_array(assets))
		goto data_error;
	json_array_foreach(assets, i, asset) {
		const char *name = json_string_value(json_object_get(asset, "name"));
		if (name && !strcmp(name, UPDATE_ASSET_NAME)) {
			asset_url = json_string_value(json_object_get(asset, "browser_download_url"));
			digest = json_string_value(json_object_get(asset, "digest"));
			break;
		}
	}
	if (!asset_url || !digest || strncmp(digest, "sha256:", 7) || strlen(digest + 7) != 64)
		goto data_error;
	if (Update_CompareVersions(EZQUAKE_TF_RELEASE_VERSION, tag) >= 0) {
		snprintf(message, sizeof(message), "Version %s is up to date.", EZQUAKE_TF_RELEASE_VERSION);
		Update_SetResult(UPDATE_CURRENT, message);
	}
	else {
		SDL_LockMutex(update_context.mutex);
		strlcpy(update_context.version, tag, sizeof(update_context.version));
		strlcpy(update_context.asset_url, asset_url, sizeof(update_context.asset_url));
		strlcpy(update_context.release_url, release_url ? release_url : "", sizeof(update_context.release_url));
		strlcpy(update_context.sha256, digest + 7, sizeof(update_context.sha256));
		SDL_UnlockMutex(update_context.mutex);
		snprintf(message, sizeof(message), "Version %s is available. Set autoupdate 1 or run /update to install it.", tag);
		Update_SetResult(UPDATE_AVAILABLE, message);
	}
	goto done;

network_error:
	Update_SetResult(UPDATE_FAILED, "Update check failed: could not connect to GitHub.");
	goto done;
data_error:
	Update_SetResult(UPDATE_FAILED, "Update check failed: invalid release metadata.");
done:
	if (root) json_decref(root);
	if (curl) curl_easy_cleanup(curl);
	free(memory.data);
	return 0;
}

static int Update_ParseExpectedHash(const char *text, unsigned char output[32])
{
	int i;
	for (i = 0; i < 32; ++i) {
		char hi = text[i * 2], lo = text[i * 2 + 1];
		int a = hi >= '0' && hi <= '9' ? hi - '0' : hi >= 'a' && hi <= 'f' ? hi - 'a' + 10 : hi >= 'A' && hi <= 'F' ? hi - 'A' + 10 : -1;
		int b = lo >= '0' && lo <= '9' ? lo - '0' : lo >= 'a' && lo <= 'f' ? lo - 'a' + 10 : lo >= 'A' && lo <= 'F' ? lo - 'A' + 10 : -1;
		if (a < 0 || b < 0) return 0;
		output[i] = (unsigned char)((a << 4) | b);
	}
	return text[64] == '\0';
}

static int Update_FileSha256(const char *path, unsigned char output[32])
{
	BCRYPT_ALG_HANDLE algorithm = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	PUCHAR object = NULL;
	DWORD object_size = 0, result_size = 0, read = 0;
	HANDLE file = INVALID_HANDLE_VALUE;
	unsigned char buffer[65536];
	BOOL read_ok;
	int ok = 0;
	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
	if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_size, sizeof(object_size), &result_size, 0) < 0) goto done;
	object = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, object_size);
	if (!object || BCryptCreateHash(algorithm, &hash, object, object_size, NULL, 0, 0) < 0) goto done;
	file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) goto done;
	while ((read_ok = ReadFile(file, buffer, sizeof(buffer), &read, NULL)) && read > 0)
		if (BCryptHashData(hash, buffer, read, 0) < 0) goto done;
	if (!read_ok || BCryptFinishHash(hash, output, 32, 0) < 0) goto done;
	ok = 1;
done:
	if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
	if (hash) BCryptDestroyHash(hash);
	if (object) HeapFree(GetProcessHeap(), 0, object);
	if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
	return ok;
}

static void Update_GetInstallRoot(char *root, size_t root_size)
{
	char *slash;
	Sys_GetFullExePath(root, (unsigned int)root_size, true);
	slash = strrchr(root, '\\');
	if (slash) *slash = '\0';
}

static void Update_CleanupRuntimeCopies(void)
{
	char root[UPDATE_PATH_CAPACITY], pattern[UPDATE_PATH_CAPACITY], path[UPDATE_PATH_CAPACITY];
	WIN32_FIND_DATAA data;
	HANDLE find;
	Update_GetInstallRoot(root, sizeof(root));
	snprintf(pattern, sizeof(pattern), "%s\\.update\\update-runtime-*.exe", root);
	find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do {
		snprintf(path, sizeof(path), "%s\\.update\\%s", root, data.cFileName);
		DeleteFileA(path);
	} while (FindNextFileA(find, &data));
	FindClose(find);
}

static int Update_AnsiToUtf8(const char *input, char *output, size_t output_size)
{
	wchar_t wide[UPDATE_PATH_CAPACITY];
	int wide_count, utf8_count;
	wide_count = MultiByteToWideChar(CP_ACP, 0, input, -1, wide, UPDATE_PATH_CAPACITY);
	if (!wide_count)
		return 0;
	utf8_count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, output, (int)output_size, NULL, NULL);
	return utf8_count > 0;
}

static int Update_DownloadThread(void *unused)
{
	CURL *curl = NULL;
	CURLcode result;
	FILE *file = NULL;
	char url[2048], version[128], expected_text[65], root[UPDATE_PATH_CAPACITY];
	char update_dir[UPDATE_PATH_CAPACITY], archive[UPDATE_PATH_CAPACITY], message[UPDATE_MESSAGE_CAPACITY];
	unsigned char expected[32], actual[32];
	(void)unused;

	SDL_LockMutex(update_context.mutex);
	strlcpy(url, update_context.asset_url, sizeof(url));
	strlcpy(version, update_context.version, sizeof(version));
	strlcpy(expected_text, update_context.sha256, sizeof(expected_text));
	SDL_UnlockMutex(update_context.mutex);
	Update_GetInstallRoot(root, sizeof(root));
	snprintf(update_dir, sizeof(update_dir), "%s\\.update", root);
	_mkdir(update_dir);
	snprintf(archive, sizeof(archive), "%s\\ezquake-tf-%s.zip", update_dir, version);
	file = fopen(archive, "wb");
	if (!file) goto file_error;
	curl = curl_easy_init();
	if (!curl || Update_ConfigureCurl(curl, url) != CURLE_OK) goto network_error;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Update_FileWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	Update_EnableCancellation(curl);
	result = curl_easy_perform(curl);
	fclose(file); file = NULL;
	if (result != CURLE_OK) goto network_error;
	if (!Update_ParseExpectedHash(expected_text, expected) || !Update_FileSha256(archive, actual) || memcmp(expected, actual, 32)) {
		DeleteFileA(archive);
		Update_SetResult(UPDATE_FAILED, "Download finished, but SHA-256 verification failed. The archive was deleted.");
		goto done;
	}
	SDL_LockMutex(update_context.mutex);
	strlcpy(update_context.archive, archive, sizeof(update_context.archive));
	SDL_UnlockMutex(update_context.mutex);
	snprintf(message, sizeof(message), "Version %s was downloaded and verified. Run /update again to install it.", version);
	Update_SetResult(UPDATE_READY, message);
	goto done;
file_error:
	Update_SetResult(UPDATE_FAILED, "Could not create the temporary update file.");
	goto done;
network_error:
	if (file) { fclose(file); file = NULL; }
	DeleteFileA(archive);
	Update_SetResult(UPDATE_FAILED, "Could not download the update archive from GitHub.");
done:
	if (curl) curl_easy_cleanup(curl);
	if (file) fclose(file);
	return 0;
}

static int Update_StartWorker(update_state_t state, int (*function)(void *), const char *name)
{
	if (update_context.thread)
		return 0;
	update_context.state = state;
	update_context.thread_done = 0;
	update_context.cancel_requested = 0;
	update_context.notification_pending = 0;
	if (state == UPDATE_CHECKING)
		strlcpy(update_context.api_url, UPDATE_API_URL, sizeof(update_context.api_url));
	update_context.thread = SDL_CreateThread(function, name, NULL);
	if (!update_context.thread) {
		update_context.state = UPDATE_FAILED;
		strlcpy(update_context.message, "Could not start the update worker.", sizeof(update_context.message));
		update_context.notification_pending = 1;
		return 0;
	}
	return 1;
}

static int Update_WritePlan(char *plan_path, size_t plan_path_size, char *runtime_path, size_t runtime_path_size)
{
	char root[UPDATE_PATH_CAPACITY], client[UPDATE_PATH_CAPACITY], updater[UPDATE_PATH_CAPACITY], update_dir[UPDATE_PATH_CAPACITY];
	char root_utf8[UPDATE_PATH_CAPACITY * 3], client_utf8[UPDATE_PATH_CAPACITY * 3], archive_utf8[UPDATE_PATH_CAPACITY * 3];
	json_t *root_json, *arguments;
	int i, result;
	Update_GetInstallRoot(root, sizeof(root));
	snprintf(client, sizeof(client), "%s\\ezquake.exe", root);
	snprintf(updater, sizeof(updater), "%s\\update.exe", root);
	snprintf(update_dir, sizeof(update_dir), "%s\\.update", root);
	snprintf(runtime_path, runtime_path_size, "%s\\update-runtime-%lu.exe", update_dir, (unsigned long)GetCurrentProcessId());
	snprintf(plan_path, plan_path_size, "%s\\plan-%lu.json", update_dir, (unsigned long)GetCurrentProcessId());
	if (GetFileAttributesA(updater) == INVALID_FILE_ATTRIBUTES) {
		Com_Printf_State(PRINT_FAIL, "update.exe was not found next to ezquake.exe.\n");
		return 0;
	}
	if (!CopyFileA(updater, runtime_path, FALSE)) {
		Com_Printf_State(PRINT_FAIL, "Could not prepare update.exe (error %lu).\n", (unsigned long)GetLastError());
		return 0;
	}
	if (!Update_AnsiToUtf8(root, root_utf8, sizeof(root_utf8)) ||
		!Update_AnsiToUtf8(client, client_utf8, sizeof(client_utf8)) ||
		!Update_AnsiToUtf8(update_context.archive, archive_utf8, sizeof(archive_utf8))) {
		DeleteFileA(runtime_path);
		Com_Printf_State(PRINT_FAIL, "Could not convert the installation path to UTF-8.\n");
		return 0;
	}
	root_json = json_object();
	arguments = json_array();
	for (i = 1; i < COM_Argc(); ++i) {
		char argument_utf8[UPDATE_PATH_CAPACITY * 3];
		if (!Update_AnsiToUtf8(COM_Argv(i), argument_utf8, sizeof(argument_utf8))) {
			json_decref(root_json);
			json_decref(arguments);
			DeleteFileA(runtime_path);
			return 0;
		}
		json_array_append_new(arguments, json_string(argument_utf8));
	}
	json_object_set_new(root_json, "schema", json_integer(1));
	json_object_set_new(root_json, "version", json_string(update_context.version));
	json_object_set_new(root_json, "install_root", json_string(root_utf8));
	json_object_set_new(root_json, "archive", json_string(archive_utf8));
	json_object_set_new(root_json, "sha256", json_string(update_context.sha256));
	json_object_set_new(root_json, "client_path", json_string(client_utf8));
	json_object_set_new(root_json, "parent_pid", json_integer(GetCurrentProcessId()));
	json_object_set_new(root_json, "restart", json_true());
	json_object_set_new(root_json, "arguments", arguments);
	result = json_dump_file(root_json, plan_path, JSON_INDENT(2) | JSON_ENSURE_ASCII) == 0;
	json_decref(root_json);
	if (!result) {
		DeleteFileA(runtime_path);
		Com_Printf_State(PRINT_FAIL, "Could not write the update plan.\n");
	}
	return result;
}

static int Update_LaunchInstaller(void)
{
	char plan[UPDATE_PATH_CAPACITY], runtime[UPDATE_PATH_CAPACITY], command[UPDATE_PATH_CAPACITY * 2];
	STARTUPINFOA startup;
	PROCESS_INFORMATION process;
	if (!Update_WritePlan(plan, sizeof(plan), runtime, sizeof(runtime)))
		return 0;
	snprintf(command, sizeof(command), "\"%s\" --plan \"%s\"", runtime, plan);
	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_HIDE;
	memset(&process, 0, sizeof(process));
	if (!CreateProcessA(runtime, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process)) {
		Com_Printf_State(PRINT_FAIL, "Could not launch update.exe (error %lu).\n", (unsigned long)GetLastError());
		return 0;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	Com_Printf("Update prepared. The client will close and restart automatically.\n");
	Cbuf_AddText("quit\n");
	return 1;
}

static void Update_Command(void)
{
	update_state_t state;
	SDL_LockMutex(update_context.mutex);
	state = update_context.state;
	SDL_UnlockMutex(update_context.mutex);
	switch (state) {
	case UPDATE_CHECKING:
		Com_Printf("An update check is already running.\n");
		break;
	case UPDATE_DOWNLOADING:
		Com_Printf("An update download is already running.\n");
		break;
	case UPDATE_AVAILABLE:
		Update_StartWorker(UPDATE_DOWNLOADING, Update_DownloadThread, "ezquake-update-download");
		Com_Printf("The update download started in the background.\n");
		break;
	case UPDATE_READY:
		Update_LaunchInstaller();
		break;
	default:
		Update_StartWorker(UPDATE_CHECKING, Update_CheckThread, "ezquake-update-check");
		Com_Printf("The update check started in the background.\n");
		break;
	}
}

static void Update_StatusCommand(void)
{
	char message[UPDATE_MESSAGE_CAPACITY];
	SDL_LockMutex(update_context.mutex);
	strlcpy(message, update_context.message[0] ? update_context.message : "no update check has run yet", sizeof(message));
	SDL_UnlockMutex(update_context.mutex);
	Com_Printf("Updater: %s\n", message);
}

void ClientUpdate_Init(void)
{
	memset(&update_context, 0, sizeof(update_context));
	Update_CleanupRuntimeCopies();
	update_context.mutex = SDL_CreateMutex();
	update_context.state = UPDATE_IDLE;
	Cvar_SetCurrentGroup(CVAR_GROUP_SYSTEM_SETTINGS);
	Cvar_Register(&autoupdate);
	Cvar_ResetCurrentGroup();
	Cmd_AddCommand("update", Update_Command);
	Cmd_AddCommand("update_check", Update_Command);
	Cmd_AddCommand("update_status", Update_StatusCommand);
}

void ClientUpdate_StartAutoCheck(void)
{
	if (update_context.mutex)
		Update_StartWorker(UPDATE_CHECKING, Update_CheckThread, "ezquake-update-check");
}

void ClientUpdate_Frame(void)
{
	int done, notify;
	update_state_t state;
	char message[UPDATE_MESSAGE_CAPACITY];
	if (!update_context.mutex) return;
	SDL_LockMutex(update_context.mutex);
	done = update_context.thread_done;
	notify = update_context.notification_pending;
	state = update_context.state;
	strlcpy(message, update_context.message, sizeof(message));
	update_context.notification_pending = 0;
	SDL_UnlockMutex(update_context.mutex);
	if (done && update_context.thread) {
		SDL_WaitThread(update_context.thread, NULL);
		update_context.thread = NULL;
	}

	if (done && autoupdate.integer) {
		if (state == UPDATE_AVAILABLE) {
			Update_StartWorker(UPDATE_DOWNLOADING, Update_DownloadThread, "ezquake-update-download");
			return;
		}
		if (state == UPDATE_READY) {
			SDL_LockMutex(update_context.mutex);
			update_context.state = UPDATE_INSTALLING;
			update_context.thread_done = 0;
			SDL_UnlockMutex(update_context.mutex);
			if (!Update_LaunchInstaller()) {
				SDL_LockMutex(update_context.mutex);
				update_context.state = UPDATE_FAILED;
				update_context.thread_done = 0;
				SDL_UnlockMutex(update_context.mutex);
			}
			return;
		}
	}

	if (notify && message[0] &&
		(state == UPDATE_CURRENT || state == UPDATE_FAILED || !autoupdate.integer))
		Com_Printf("Updater: %s\n", message);
}

void ClientUpdate_Shutdown(void)
{
	if (update_context.thread) {
		SDL_LockMutex(update_context.mutex);
		update_context.cancel_requested = 1;
		SDL_UnlockMutex(update_context.mutex);
		SDL_WaitThread(update_context.thread, NULL);
		update_context.thread = NULL;
	}
	if (update_context.mutex) {
		SDL_DestroyMutex(update_context.mutex);
		update_context.mutex = NULL;
	}
}

#else
void ClientUpdate_Init(void) {}
void ClientUpdate_StartAutoCheck(void) {}
void ClientUpdate_Frame(void) {}
void ClientUpdate_Shutdown(void) {}
#endif
