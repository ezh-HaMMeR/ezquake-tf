#include "cfg_editor_parser.h"
#include "cfg_editor_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition, message) \
	do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

static unsigned char *ReadFile(const char *path, size_t *length)
{
	FILE *file;
	long size;
	unsigned char *data;

	*length = 0;
	file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "FAIL: cannot open %s\n", path);
		failures++;
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "FAIL: cannot size %s\n", path);
		failures++;
		fclose(file);
		return NULL;
	}
	data = (unsigned char *)malloc(size ? (size_t)size : 1);
	if (!data || (size && fread(data, 1, (size_t)size, file) != (size_t)size)) {
		fprintf(stderr, "FAIL: cannot read %s\n", path);
		failures++;
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	*length = (size_t)size;
	return data;
}

static void CheckRoundTrip(const char *path, const unsigned char *input, size_t input_length)
{
	cfg_document_t document;
	unsigned char *output = NULL;
	size_t output_length = 0;
	size_t i;

	CHECK(CFGDoc_Parse(&document, path, input, input_length), "parse succeeds");
	if (!document.source_name) {
		return;
	}
	CHECK(CFGDoc_Serialize(&document, &output, &output_length), "serialize succeeds");
	CHECK(output_length == input_length, "round-trip length is unchanged");
	CHECK(output_length != input_length || !memcmp(output, input, input_length), "round-trip bytes are unchanged");
	for (i = 0; i < document.node_count; ++i) {
		CHECK(document.nodes[i].byte_start <= document.nodes[i].byte_end, "node byte range is ordered");
		if (i) {
			CHECK(document.nodes[i - 1].byte_end == document.nodes[i].byte_start, "node byte ranges are contiguous");
		}
	}
	printf("PASS: %s (%lu bytes, %lu nodes)\n", path,
		(unsigned long)input_length, (unsigned long)document.node_count);
	free(output);
	CFGDoc_Free(&document);
}

static void TestSyntheticDocument(void)
{
	static const unsigned char input[] =
		"exec settings.cfg\r\n"
		"\r\n"
		"m_pitch \"-0.022\"; bind mouse1 \"+attack0 0;wait\" // inline\r\n"
		"// comment with ; separator\n"
		"alias test \"echo \\\"quoted\\\";wait\"\n"
		"name \"player\x86\"";
	cfg_document_t document;
	cfg_node_t *node;
	unsigned char *output = NULL;
	size_t output_length = 0;

	CheckRoundTrip("synthetic.cfg", input, sizeof(input) - 1);
	CHECK(CFGDoc_Parse(&document, "synthetic.cfg", input, sizeof(input) - 1), "synthetic parse succeeds");
	if (!document.source_name) {
		return;
	}
	CHECK(document.node_count == 7, "quoted semicolon does not split a node");
	CHECK(document.nodes[0].kind == CFG_NODE_EXEC, "exec is classified");
	CHECK(document.nodes[1].kind == CFG_NODE_BLANK, "blank line is classified");
	CHECK(document.nodes[2].kind == CFG_NODE_COMMAND, "direct cvar form is classified as command");
	CHECK(document.nodes[3].kind == CFG_NODE_BIND, "bind after separator is classified");
	CHECK(document.nodes[4].kind == CFG_NODE_COMMENT, "comment line is classified");
	CHECK(document.nodes[5].kind == CFG_NODE_ALIAS, "alias is classified");
	CHECK(document.nodes[6].line_start == 6, "line source coordinate is recorded");

	node = &document.nodes[2];
	CHECK(CFGDoc_ReplaceNode(&document, node->id,
		(const unsigned char *)"m_pitch \"0.022\"\n// inserted through textarea\n",
		strlen("m_pitch \"0.022\"\n// inserted through textarea\n")), "node replacement succeeds");
	CHECK(document.nodes[3].line_start == 5, "later line coordinates are reindexed after multiline replacement");
	CHECK(CFGDoc_Serialize(&document, &output, &output_length), "modified document serializes");
	CHECK(strstr((const char *)output, "bind mouse1") != NULL, "unmodified later node remains present");
	free(output);
	CFGDoc_Free(&document);
}

static void TestFileBackedModel(void)
{
	static const unsigned char main_cfg[] =
		"m_pitch \"0.022\"\n"
		"bind w \"+forward\"\n"
		"custom_main_command 1\n";
	static const unsigned char scout_cfg[] =
		"exec settings.cfg\n"
		"m_pitch \"-0.022\"\n"
		"alias scout_misc \"echo test;wait\"\n";
	static const char *parents[] = { "main" };
	cfg_editor_model_t model;
	cfg_setting_result_t setting;
	cfg_source_ref_t *misc = NULL;
	size_t misc_count;
	char label[128];
	unsigned char *serialized = NULL;
	size_t serialized_length = 0;

	CFGModel_Init(&model);
	CHECK(CFGModel_AddFileFromMemory(&model, "main", "settings.cfg", "main", NULL, 0,
		main_cfg, sizeof(main_cfg) - 1), "main model file loads from memory");
	CHECK(CFGModel_AddFileFromMemory(&model, "class_scout", "scout.cfg", "class", parents, 1,
		scout_cfg, sizeof(scout_cfg) - 1), "class model file loads from memory");

	CHECK(CFGModel_ResolveSetting(&model, "main", CFG_STORAGE_CVAR, "m_pitch", NULL, NULL, &setting),
		"base setting resolves");
	CHECK(!strcmp(setting.value, "0.022") && !setting.inherited, "base setting is local");
	CHECK(CFGModel_ResolveSetting(&model, "class_scout", CFG_STORAGE_CVAR, "m_pitch", NULL, NULL, &setting),
		"class setting resolves");
	CHECK(!strcmp(setting.value, "-0.022") && !setting.inherited, "class override wins");
	CHECK(CFGModel_ResolveSetting(&model, "class_scout", CFG_STORAGE_CVAR, "custom_main_command", NULL, NULL, &setting),
		"parent-only setting resolves");
	CHECK(setting.inherited && !strcmp(setting.value, "1"), "parent-only setting is marked inherited");

	CHECK(CFGModel_MarkSettingManaged(&model, CFG_STORAGE_CVAR, "m_pitch", NULL, NULL) == 2,
		"known setting marks both base and override nodes");
	CHECK(CFGModel_MarkBindManaged(&model, "+forward", 0) == 1,
		"known bind is marked without touching global bindings");

	misc_count = CFGModel_CollectMisc(&model, "class_scout", 1, &misc);
	CHECK(misc_count == 2, "class Misc contains only unmanaged source nodes");
	if (misc_count == 2) {
		CFGModel_FormatSourceLabel(&misc[1], label, sizeof(label));
		CHECK(strstr(label, "scout.cfg:3") != NULL, "Misc label contains file and source line");
		CHECK(CFGModel_ReplaceSource(&model, &misc[1],
			(const unsigned char *)"alias scout_misc \"echo changed;wait\"\n",
			strlen("alias scout_misc \"echo changed;wait\"\n")), "Misc replacement targets original node id");
	}
	free(misc);

	CHECK(CFGModel_SerializeFile(&model, "class_scout", &serialized, &serialized_length),
		"model serializes a selected backing file");
	CHECK(strstr((const char *)serialized, "echo changed;wait") != NULL,
		"edited Misc node is written at its source location");
	CHECK(strstr((const char *)serialized, "m_pitch \"-0.022\"") != NULL,
		"unrelated class setting is preserved");
	free(serialized);
	CHECK(CFGModel_FindFile(&model, "class_scout")->dirty, "source edit marks only its file dirty");
	CFGModel_Free(&model);
}

static void TestManagedFilesManifest(const char *first_config_path)
{
	cfg_editor_model_t model;
	char config_root[1024];
	char error[512];
	char *separator;

	if (strlen(first_config_path) >= sizeof(config_root)) {
		CHECK(0, "config corpus path fits test buffer");
		return;
	}
	strcpy(config_root, first_config_path);
	separator = strrchr(config_root, '\\');
	if (!separator) separator = strrchr(config_root, '/');
	if (!separator) {
		CHECK(0, "config corpus has a parent directory");
		return;
	}
	*separator = '\0';

	CFGModel_Init(&model);
	CHECK(CFGModel_LoadManifest(&model, "qw/config_editor/managed_files.json",
		config_root, error, sizeof(error)), error[0] ? error : "managed-files manifest loads");
	CHECK(model.file_count == 12, "managed-files manifest loads exactly 12 CFG files");
	CHECK(CFGModel_FindFileConst(&model, "class_scout") != NULL, "class context exists in manifest model");
	CFGModel_Free(&model);
}

int main(int argc, char **argv)
{
	int i;

	TestSyntheticDocument();
	TestFileBackedModel();
	if (argc > 1) TestManagedFilesManifest(argv[1]);
	for (i = 1; i < argc; ++i) {
		size_t length;
		unsigned char *data = ReadFile(argv[i], &length);
		if (data) {
			CheckRoundTrip(argv[i], data, length);
			free(data);
		}
	}

	if (failures) {
		fprintf(stderr, "%d test(s) failed\n", failures);
		return 1;
	}
	printf("All CFG editor parser tests passed.\n");
	return 0;
}
