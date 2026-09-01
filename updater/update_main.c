#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <jansson.h>
#include <minizip/unzip.h>
#include <minizip/ioapi.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "update_common.h"

#pragma comment(lib, "bcrypt.lib")

#define UPDATE_PATH_CAPACITY 4096
#define UPDATE_HASH_SIZE 32

typedef struct update_file_s {
	char relative[UPDATE_PATH_CAPACITY];
	int had_original;
} update_file_t;

typedef struct update_plan_s {
	wchar_t plan_path[UPDATE_PATH_CAPACITY];
	wchar_t install_root[UPDATE_PATH_CAPACITY];
	wchar_t archive[UPDATE_PATH_CAPACITY];
	wchar_t client_path[UPDATE_PATH_CAPACITY];
	wchar_t version[128];
	wchar_t expected_hash[65];
	DWORD parent_pid;
	int restart;
	json_t *arguments;
} update_plan_t;

static FILE *update_log;

static void Update_Log(const char *format, ...)
{
	va_list args;
	if (!update_log)
		return;
	va_start(args, format);
	vfprintf(update_log, format, args);
	va_end(args);
	fflush(update_log);
}

static int Update_Utf8ToWide(const char *input, wchar_t *output, size_t output_count)
{
	int count;
	if (!input || !output || output_count == 0)
		return 0;
	count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, (int)output_count);
	return count > 0;
}

static int Update_WideToUtf8(const wchar_t *input, char *output, size_t output_count)
{
	int count;
	if (!input || !output || output_count == 0)
		return 0;
	count = WideCharToMultiByte(CP_UTF8, 0, input, -1, output, (int)output_count, NULL, NULL);
	return count > 0;
}

static voidpf ZCALLBACK Update_ZipOpenWide(voidpf opaque, const void *filename, int mode)
{
	const wchar_t *file_mode = L"rb";
	(void)opaque;
	if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_WRITE)
		file_mode = (mode & ZLIB_FILEFUNC_MODE_EXISTING) ? L"r+b" : L"wb";
	else if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READWRITEFILTER)
		file_mode = (mode & ZLIB_FILEFUNC_MODE_EXISTING) ? L"r+b" : L"w+b";
	return _wfopen((const wchar_t *)filename, file_mode);
}

static uLong ZCALLBACK Update_ZipRead(voidpf opaque, voidpf stream, void *buffer, uLong size)
{
	(void)opaque;
	return (uLong)fread(buffer, 1, size, (FILE *)stream);
}

static uLong ZCALLBACK Update_ZipWrite(voidpf opaque, voidpf stream, const void *buffer, uLong size)
{
	(void)opaque;
	return (uLong)fwrite(buffer, 1, size, (FILE *)stream);
}

static ZPOS64_T ZCALLBACK Update_ZipTell(voidpf opaque, voidpf stream)
{
	(void)opaque;
	return (ZPOS64_T)_ftelli64((FILE *)stream);
}

static long ZCALLBACK Update_ZipSeek(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin)
{
	int file_origin = origin == ZLIB_FILEFUNC_SEEK_CUR ? SEEK_CUR :
		origin == ZLIB_FILEFUNC_SEEK_END ? SEEK_END : SEEK_SET;
	(void)opaque;
	return _fseeki64((FILE *)stream, (__int64)offset, file_origin);
}

static int ZCALLBACK Update_ZipClose(voidpf opaque, voidpf stream)
{
	(void)opaque;
	return fclose((FILE *)stream);
}

static int ZCALLBACK Update_ZipError(voidpf opaque, voidpf stream)
{
	(void)opaque;
	return ferror((FILE *)stream);
}

static void Update_NormalizeSeparators(wchar_t *path)
{
	wchar_t *p;
	for (p = path; *p; ++p)
		if (*p == L'/') *p = L'\\';
}

static int Update_JoinPath(wchar_t *output, size_t output_count, const wchar_t *root, const wchar_t *relative)
{
	int written = _snwprintf(output, output_count, L"%ls%ls%ls", root,
		root[0] && root[wcslen(root) - 1] != L'\\' ? L"\\" : L"", relative);
	if (written < 0 || (size_t)written >= output_count)
		return 0;
	return 1;
}

static int Update_EnsureDirectory(const wchar_t *path)
{
	int result = SHCreateDirectoryExW(NULL, path, NULL);
	return result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS;
}

static void Update_ParentDirectory(const wchar_t *path, wchar_t *output, size_t output_count)
{
	wchar_t *slash;
	wcsncpy(output, path, output_count - 1);
	output[output_count - 1] = L'\0';
	slash = wcsrchr(output, L'\\');
	if (slash)
		*slash = L'\0';
}

static int Update_DeleteTree(const wchar_t *path)
{
	wchar_t pattern[UPDATE_PATH_CAPACITY];
	wchar_t child[UPDATE_PATH_CAPACITY];
	WIN32_FIND_DATAW data;
	HANDLE find;

	if (!Update_JoinPath(pattern, UPDATE_PATH_CAPACITY, path, L"*"))
		return 0;
	find = FindFirstFileW(pattern, &data);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
				continue;
			if (!Update_JoinPath(child, UPDATE_PATH_CAPACITY, path, data.cFileName)) {
				FindClose(find);
				return 0;
			}
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (!Update_DeleteTree(child)) {
					FindClose(find);
					return 0;
				}
			}
			else {
				SetFileAttributesW(child, FILE_ATTRIBUTE_NORMAL);
				if (!DeleteFileW(child)) {
					FindClose(find);
					return 0;
				}
			}
		} while (FindNextFileW(find, &data));
		FindClose(find);
	}
	return RemoveDirectoryW(path) || GetLastError() == ERROR_PATH_NOT_FOUND;
}

static int Update_ParseHash(const wchar_t *text, unsigned char output[UPDATE_HASH_SIZE])
{
	int i;
	for (i = 0; i < UPDATE_HASH_SIZE; ++i) {
		wchar_t hi = text[i * 2];
		wchar_t lo = text[i * 2 + 1];
		int a = hi >= L'0' && hi <= L'9' ? hi - L'0' : hi >= L'a' && hi <= L'f' ? hi - L'a' + 10 : hi >= L'A' && hi <= L'F' ? hi - L'A' + 10 : -1;
		int b = lo >= L'0' && lo <= L'9' ? lo - L'0' : lo >= L'a' && lo <= L'f' ? lo - L'a' + 10 : lo >= L'A' && lo <= L'F' ? lo - L'A' + 10 : -1;
		if (a < 0 || b < 0)
			return 0;
		output[i] = (unsigned char)((a << 4) | b);
	}
	return text[64] == L'\0';
}

static int Update_FileSha256(const wchar_t *path, unsigned char output[UPDATE_HASH_SIZE])
{
	BCRYPT_ALG_HANDLE algorithm = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	PUCHAR object = NULL;
	DWORD object_size = 0, result_size = 0, read = 0;
	BOOL read_ok;
	HANDLE file = INVALID_HANDLE_VALUE;
	unsigned char buffer[65536];
	int ok = 0;

	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0)
		goto done;
	if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_size, sizeof(object_size), &result_size, 0) < 0)
		goto done;
	object = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, object_size);
	if (!object || BCryptCreateHash(algorithm, &hash, object, object_size, NULL, 0, 0) < 0)
		goto done;
	file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		goto done;
	while ((read_ok = ReadFile(file, buffer, sizeof(buffer), &read, NULL)) && read > 0) {
		if (BCryptHashData(hash, buffer, read, 0) < 0)
			goto done;
	}
	if (!read_ok)
		goto done;
	if (BCryptFinishHash(hash, output, UPDATE_HASH_SIZE, 0) < 0)
		goto done;
	ok = 1;

done:
	if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
	if (hash) BCryptDestroyHash(hash);
	if (object) HeapFree(GetProcessHeap(), 0, object);
	if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
	return ok;
}

static int Update_LoadPlan(const wchar_t *path, update_plan_t *plan)
{
	FILE *file;
	json_error_t error;
	json_t *root;
	json_t *value;
	const char *text;

	memset(plan, 0, sizeof(*plan));
	wcsncpy(plan->plan_path, path, UPDATE_PATH_CAPACITY - 1);
	file = _wfopen(path, L"rb");
	if (!file)
		return 0;
	root = json_loadf(file, JSON_REJECT_DUPLICATES, &error);
	fclose(file);
	if (!root || !json_is_object(root)) {
		if (root) json_decref(root);
		return 0;
	}
	value = json_object_get(root, "schema");
	if (!json_is_integer(value) || json_integer_value(value) != 1)
		goto fail;
#define READ_WIDE(field, key) do { value = json_object_get(root, key); text = json_string_value(value); \
	if (!text || !Update_Utf8ToWide(text, plan->field, sizeof(plan->field) / sizeof(plan->field[0]))) goto fail; } while (0)
	READ_WIDE(install_root, "install_root");
	READ_WIDE(archive, "archive");
	READ_WIDE(client_path, "client_path");
	READ_WIDE(version, "version");
	READ_WIDE(expected_hash, "sha256");
#undef READ_WIDE
	value = json_object_get(root, "parent_pid");
	if (!json_is_integer(value))
		goto fail;
	plan->parent_pid = (DWORD)json_integer_value(value);
	value = json_object_get(root, "restart");
	plan->restart = !json_is_false(value);
	plan->arguments = json_object_get(root, "arguments");
	if (!json_is_array(plan->arguments))
		goto fail;
	json_incref(plan->arguments);
	json_decref(root);
	return 1;
fail:
	json_decref(root);
	return 0;
}

static int Update_ProcessUsesClient(const wchar_t *client_path, DWORD excluded_pid)
{
	PROCESSENTRY32W entry;
	HANDLE snapshot;
	int found = 0;

	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return 0;
	memset(&entry, 0, sizeof(entry));
	entry.dwSize = sizeof(entry);
	if (Process32FirstW(snapshot, &entry)) {
		do {
			HANDLE process;
			wchar_t path[UPDATE_PATH_CAPACITY];
			DWORD count = UPDATE_PATH_CAPACITY;
			if (entry.th32ProcessID == excluded_pid || entry.th32ProcessID == GetCurrentProcessId())
				continue;
			process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
			if (!process)
				continue;
			if (QueryFullProcessImageNameW(process, 0, path, &count) && !_wcsicmp(path, client_path))
				found = 1;
			CloseHandle(process);
			if (found)
				break;
		} while (Process32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return found;
}

static int Update_WaitForClients(const update_plan_t *plan)
{
	HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, plan->parent_pid);
	DWORD start = GetTickCount();
	if (parent) {
		WaitForSingleObject(parent, INFINITE);
		CloseHandle(parent);
	}
	while (Update_ProcessUsesClient(plan->client_path, 0)) {
		if (GetTickCount() - start > 15 * 60 * 1000)
			return 0;
		Sleep(500);
	}
	return 1;
}

static int Update_ExtractArchive(const update_plan_t *plan, const wchar_t *staging)
{
	unzFile zip;
	zlib_filefunc64_def file_functions;
	int result;

	memset(&file_functions, 0, sizeof(file_functions));
	file_functions.zopen64_file = Update_ZipOpenWide;
	file_functions.zread_file = Update_ZipRead;
	file_functions.zwrite_file = Update_ZipWrite;
	file_functions.ztell64_file = Update_ZipTell;
	file_functions.zseek64_file = Update_ZipSeek;
	file_functions.zclose_file = Update_ZipClose;
	file_functions.zerror_file = Update_ZipError;
	zip = unzOpen2_64(plan->archive, &file_functions);
	if (!zip)
		return 0;
	result = unzGoToFirstFile(zip);
	while (result == UNZ_OK) {
		unz_file_info64 info;
		char name[UPDATE_PATH_CAPACITY];
		wchar_t relative[UPDATE_PATH_CAPACITY];
		wchar_t destination[UPDATE_PATH_CAPACITY];
		wchar_t parent[UPDATE_PATH_CAPACITY];
		HANDLE output;
		unsigned char buffer[65536];
		int read;

		memset(&info, 0, sizeof(info));
		if (unzGetCurrentFileInfo64(zip, &info, name, sizeof(name), NULL, 0, NULL, 0) != UNZ_OK)
			goto fail;
		name[sizeof(name) - 1] = '\0';
		if (name[0] && name[strlen(name) - 1] == '/') {
			result = unzGoToNextFile(zip);
			continue;
		}
		if (!Update_IsSafeManagedPath(name) || !Update_Utf8ToWide(name, relative, UPDATE_PATH_CAPACITY)) {
			Update_Log("Rejected archive path: %s\n", name);
			goto fail;
		}
		Update_NormalizeSeparators(relative);
		if (!Update_JoinPath(destination, UPDATE_PATH_CAPACITY, staging, relative))
			goto fail;
		Update_ParentDirectory(destination, parent, UPDATE_PATH_CAPACITY);
		if (!Update_EnsureDirectory(parent) || unzOpenCurrentFile(zip) != UNZ_OK)
			goto fail;
		output = CreateFileW(destination, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (output == INVALID_HANDLE_VALUE) {
			unzCloseCurrentFile(zip);
			goto fail;
		}
		while ((read = unzReadCurrentFile(zip, buffer, sizeof(buffer))) > 0) {
			DWORD written;
			if (!WriteFile(output, buffer, (DWORD)read, &written, NULL) || written != (DWORD)read) {
				CloseHandle(output);
				unzCloseCurrentFile(zip);
				goto fail;
			}
		}
		CloseHandle(output);
		if (read < 0 || unzCloseCurrentFile(zip) != UNZ_OK)
			goto fail;
		result = unzGoToNextFile(zip);
	}
	unzClose(zip);
	return result == UNZ_END_OF_LIST_OF_FILE;
fail:
	unzClose(zip);
	return 0;
}

static int Update_CollectFilesRecursive(const wchar_t *root, const wchar_t *relative,
	update_file_t **files, size_t *count, size_t *capacity)
{
	wchar_t current[UPDATE_PATH_CAPACITY], pattern[UPDATE_PATH_CAPACITY], child_relative[UPDATE_PATH_CAPACITY];
	WIN32_FIND_DATAW data;
	HANDLE find;
	if (!Update_JoinPath(current, UPDATE_PATH_CAPACITY, root, relative) ||
		!Update_JoinPath(pattern, UPDATE_PATH_CAPACITY, current, L"*"))
		return 0;
	find = FindFirstFileW(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return 1;
	do {
		char relative_utf8[UPDATE_PATH_CAPACITY];
		if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L".."))
			continue;
		if (relative[0])
			_snwprintf(child_relative, UPDATE_PATH_CAPACITY, L"%ls\\%ls", relative, data.cFileName);
		else
			wcsncpy(child_relative, data.cFileName, UPDATE_PATH_CAPACITY - 1);
		child_relative[UPDATE_PATH_CAPACITY - 1] = L'\0';
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!Update_CollectFilesRecursive(root, child_relative, files, count, capacity)) {
				FindClose(find);
				return 0;
			}
			continue;
		}
		if (*count == *capacity) {
			size_t new_capacity = *capacity ? *capacity * 2 : 64;
			update_file_t *new_files = (update_file_t *)realloc(*files, new_capacity * sizeof(**files));
			if (!new_files) {
				FindClose(find);
				return 0;
			}
			*files = new_files;
			*capacity = new_capacity;
		}
		if (!Update_WideToUtf8(child_relative, relative_utf8, sizeof(relative_utf8))) {
			FindClose(find);
			return 0;
		}
		{
			size_t i;
			for (i = 0; relative_utf8[i]; ++i)
				if (relative_utf8[i] == '\\') relative_utf8[i] = '/';
		}
		if (!Update_IsSafeManagedPath(relative_utf8)) {
			FindClose(find);
			return 0;
		}
		strncpy((*files)[*count].relative, relative_utf8, UPDATE_PATH_CAPACITY - 1);
		(*files)[*count].relative[UPDATE_PATH_CAPACITY - 1] = '\0';
		(*files)[*count].had_original = 0;
		++*count;
	} while (FindNextFileW(find, &data));
	FindClose(find);
	return 1;
}

static int Update_ApplyFiles(const update_plan_t *plan, const wchar_t *staging, const wchar_t *backup)
{
	update_file_t *files = NULL;
	size_t count = 0, capacity = 0, applied = 0, i;
	int ok = 0;

	if (!Update_CollectFilesRecursive(staging, L"", &files, &count, &capacity) || count == 0)
		goto done;
	for (i = 0; i < count; ++i) {
		wchar_t relative[UPDATE_PATH_CAPACITY], source[UPDATE_PATH_CAPACITY], destination[UPDATE_PATH_CAPACITY];
		wchar_t backup_path[UPDATE_PATH_CAPACITY], parent[UPDATE_PATH_CAPACITY];
		if (!Update_Utf8ToWide(files[i].relative, relative, UPDATE_PATH_CAPACITY))
			goto rollback;
		Update_NormalizeSeparators(relative);
		if (!Update_JoinPath(source, UPDATE_PATH_CAPACITY, staging, relative) ||
			!Update_JoinPath(destination, UPDATE_PATH_CAPACITY, plan->install_root, relative) ||
			!Update_JoinPath(backup_path, UPDATE_PATH_CAPACITY, backup, relative))
			goto rollback;
		Update_ParentDirectory(destination, parent, UPDATE_PATH_CAPACITY);
		if (!Update_EnsureDirectory(parent))
			goto rollback;
		if (GetFileAttributesW(destination) != INVALID_FILE_ATTRIBUTES) {
			Update_ParentDirectory(backup_path, parent, UPDATE_PATH_CAPACITY);
			if (!Update_EnsureDirectory(parent) || !MoveFileExW(destination, backup_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				goto rollback;
			files[i].had_original = 1;
		}
		if (!MoveFileExW(source, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			if (files[i].had_original)
				MoveFileExW(backup_path, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
			goto rollback;
		}
		applied = i + 1;
	}
	ok = 1;
	goto done;

rollback:
	while (applied > 0) {
		wchar_t relative[UPDATE_PATH_CAPACITY], destination[UPDATE_PATH_CAPACITY], backup_path[UPDATE_PATH_CAPACITY];
		--applied;
		Update_Utf8ToWide(files[applied].relative, relative, UPDATE_PATH_CAPACITY);
		Update_NormalizeSeparators(relative);
		Update_JoinPath(destination, UPDATE_PATH_CAPACITY, plan->install_root, relative);
		Update_JoinPath(backup_path, UPDATE_PATH_CAPACITY, backup, relative);
		DeleteFileW(destination);
		if (files[applied].had_original)
			MoveFileExW(backup_path, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	}
done:
	free(files);
	return ok;
}

static void Update_AppendQuoted(wchar_t *command, size_t capacity, const wchar_t *argument)
{
	size_t length = wcslen(command), backslashes = 0;
	const wchar_t *p;
	if (length + 3 >= capacity) return;
	if (length) command[length++] = L' ';
	command[length++] = L'"';
	command[length] = L'\0';
	for (p = argument; *p && length + 3 < capacity; ++p) {
		if (*p == L'\\') { ++backslashes; continue; }
		if (*p == L'"') {
			while (backslashes > 0 && length + 2 < capacity) { command[length++] = L'\\'; command[length++] = L'\\'; --backslashes; }
			command[length++] = L'\\'; command[length++] = L'"'; backslashes = 0; continue;
		}
		while (backslashes > 0 && length + 1 < capacity) { command[length++] = L'\\'; --backslashes; }
		command[length++] = *p; backslashes = 0;
	}
	while (backslashes > 0 && length + 2 < capacity) { command[length++] = L'\\'; command[length++] = L'\\'; --backslashes; }
	command[length++] = L'"'; command[length] = L'\0';
}

static int Update_RestartClient(const update_plan_t *plan)
{
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	wchar_t command[32768] = L"";
	size_t i;
	Update_AppendQuoted(command, sizeof(command) / sizeof(command[0]), plan->client_path);
	for (i = 0; i < json_array_size(plan->arguments); ++i) {
		json_t *value = json_array_get(plan->arguments, i);
		const char *argument = json_string_value(value);
		wchar_t wide[UPDATE_PATH_CAPACITY];
		if (argument && Update_Utf8ToWide(argument, wide, UPDATE_PATH_CAPACITY))
			Update_AppendQuoted(command, sizeof(command) / sizeof(command[0]), wide);
	}
	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	memset(&process, 0, sizeof(process));
	if (!CreateProcessW(plan->client_path, command, NULL, NULL, FALSE, 0, NULL, plan->install_root, &startup, &process))
		return 0;
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return 1;
}

static int Update_Run(const update_plan_t *plan)
{
	unsigned char expected[UPDATE_HASH_SIZE], actual[UPDATE_HASH_SIZE];
	wchar_t work[UPDATE_PATH_CAPACITY], staging_root[UPDATE_PATH_CAPACITY], backup_root[UPDATE_PATH_CAPACITY];
	wchar_t staging[UPDATE_PATH_CAPACITY], backup[UPDATE_PATH_CAPACITY];

	if (!Update_ParseHash(plan->expected_hash, expected) || !Update_FileSha256(plan->archive, actual) ||
		memcmp(expected, actual, UPDATE_HASH_SIZE)) {
		Update_Log("Archive SHA-256 verification failed.\n");
		return 0;
	}
	if (!Update_JoinPath(work, UPDATE_PATH_CAPACITY, plan->install_root, L".update") ||
		!Update_JoinPath(staging_root, UPDATE_PATH_CAPACITY, work, L"staging") ||
		!Update_JoinPath(staging, UPDATE_PATH_CAPACITY, staging_root, plan->version) ||
		!Update_JoinPath(backup_root, UPDATE_PATH_CAPACITY, work, L"backup") ||
		!Update_JoinPath(backup, UPDATE_PATH_CAPACITY, backup_root, plan->version))
		return 0;
	Update_DeleteTree(staging);
	if (!Update_EnsureDirectory(staging) || !Update_EnsureDirectory(backup))
		return 0;
	if (!Update_ExtractArchive(plan, staging)) {
		Update_Log("Archive extraction failed.\n");
		return 0;
	}
	if (!Update_WaitForClients(plan)) {
		Update_Log("Timed out waiting for ezquake.exe instances.\n");
		return 0;
	}
	if (!Update_ApplyFiles(plan, staging, backup)) {
		Update_Log("File replacement failed; rollback attempted.\n");
		return 0;
	}
	Update_DeleteTree(staging);
	DeleteFileW(plan->archive);
	DeleteFileW(plan->plan_path);
	return !plan->restart || Update_RestartClient(plan);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show)
{
	int argc, i, ok, silent = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	update_plan_t plan;
	wchar_t log_path[UPDATE_PATH_CAPACITY], log_dir[UPDATE_PATH_CAPACITY];
	const wchar_t *plan_path = NULL;
	(void)instance; (void)previous; (void)command_line; (void)show;
	for (i = 1; i < argc; ++i) {
		if (!wcscmp(argv[i], L"--silent")) silent = 1;
		if (!wcscmp(argv[i], L"--plan") && i + 1 < argc) plan_path = argv[i + 1];
	}
	if (!plan_path || !Update_LoadPlan(plan_path, &plan)) {
		if (!silent)
			MessageBoxW(NULL, L"Не удалось прочитать план обновления.", L"ezquake-tf updater", MB_OK | MB_ICONERROR);
		if (argv) LocalFree(argv);
		return 2;
	}
	Update_JoinPath(log_dir, UPDATE_PATH_CAPACITY, plan.install_root, L".update");
	Update_EnsureDirectory(log_dir);
	Update_JoinPath(log_path, UPDATE_PATH_CAPACITY, log_dir, L"update.log");
	update_log = _wfopen(log_path, L"ab");
	Update_Log("Starting update to %ls\n", plan.version);
	ok = Update_Run(&plan);
	Update_Log(ok ? "Update completed.\n" : "Update failed.\n");
	if (update_log) fclose(update_log);
	json_decref(plan.arguments);
	if (argv) LocalFree(argv);
	if (!ok && !silent)
		MessageBoxW(NULL, L"Обновление не установлено. Подробности: .update\\update.log\nИсходные файлы восстановлены из резервной копии.", L"ezquake-tf updater", MB_OK | MB_ICONERROR);
	return ok ? 0 : 1;
}
