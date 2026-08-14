#include "cfg_editor_parser.h"
#include "cfg_editor_model.h"
#include "cfg_editor_dictionary.h"

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
		"name \"ezh\x9c\xc8" "a\xcd\xcd" "e\xd2\"";
	static const unsigned char expected_name[] = {
		'e', 'z', 'h', 0x9c, 0xc8, 'a', 0xcd, 0xcd, 'e', 0xd2
	};
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
	CHECK(document.nodes[6].kind == CFG_NODE_COMMAND, "raw Quake name is classified as a command");
	CHECK(document.nodes[6].value && strlen(document.nodes[6].value) == sizeof(expected_name) &&
		!memcmp(document.nodes[6].value, expected_name, sizeof(expected_name)),
		"raw Quake name bytes are preserved as the resolved value");

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

static void TestManagedFilesManifest(const char *game_root)
{
	cfg_editor_model_t model;
	char error[512];

	CFGModel_Init(&model);
	CHECK(CFGModel_LoadManifest(&model, "qw/config_editor/managed_files.json",
		game_root, error, sizeof(error)), error[0] ? error : "managed-files manifest loads");
	CHECK(model.file_count == 12, "managed-files manifest loads exactly 12 CFG files");
	CHECK(CFGModel_FindFileConst(&model, "class_scout") != NULL, "class context exists in manifest model");
	CFGModel_Free(&model);
}

static void TestRealDictionaries(const char *game_root)
{
	cfg_editor_model_t model;
	cfg_editor_dictionary_t dictionary;
	cfg_dictionary_apply_result_t applied;
	char error[512];
	size_t file_index, node_index;
	size_t definition_index, option_index;
	size_t main_managed = 0, hud_managed = 0, bind_nodes = 0;
	int found_language = 0, found_player_name = 0, found_grenade_mode = 0;
	int found_grenade1 = 0, found_grenade2 = 0, found_throw = 0;
	int found_bind_help = 0;
	unsigned char *hud_before = NULL, *hud_after = NULL;
	size_t hud_before_length = 0, hud_after_length = 0;

	CFGModel_Init(&model);
	CFGDictionary_Init(&dictionary);
	CHECK(CFGModel_LoadManifest(&model, "qw/config_editor/managed_files.json",
		game_root, error, sizeof(error)), error[0] ? error : "manifest loads for dictionary test");
	CHECK(CFGDictionary_Load(&dictionary, "qw/config_editor/dict_settings.json",
		"qw/config_editor/dict_binds.json", error, sizeof(error)),
		error[0] ? error : "dictionary files load");
	CHECK(dictionary.setting_count == 31, "settings dictionary has the expected definitions");
	CHECK(dictionary.bind_count == 66, "bind dictionary has the expected unique actions");
	for (definition_index = 0; definition_index < dictionary.setting_count; ++definition_index) {
		cfg_setting_definition_t *definition = &dictionary.settings[definition_index];
		CHECK(definition->label && definition->label[0] && definition->label_en && definition->label_en[0],
			"every setting has Russian and English labels");
		CHECK(definition->description && definition->description[0] &&
			definition->description_en && definition->description_en[0],
			"every setting has Russian and English descriptions");
		for (option_index = 0; option_index < definition->option_count; ++option_index)
			CHECK(definition->options[option_index].label && definition->options[option_index].label[0] &&
				definition->options[option_index].label_en && definition->options[option_index].label_en[0],
				"every select option has Russian and English labels");
		found_language += !strcmp(definition->id, "menu_language");
		found_player_name += !strcmp(definition->id, "player_name") && definition->max_length == 15;
		found_grenade_mode += !strcmp(definition->id, "grenade_control_mode") && definition->option_count == 3;
	}
	for (definition_index = 0; definition_index < dictionary.bind_count; ++definition_index) {
		cfg_bind_definition_t *definition = &dictionary.binds[definition_index];
		CHECK(definition->label && definition->label[0] && definition->label_en && definition->label_en[0],
			"every bind has Russian and English labels");
		CHECK(definition->description && definition->description[0] &&
			definition->description_en && definition->description_en[0],
			"every bind has Russian and English descriptions");
		found_grenade1 += !strcmp(definition->command, "+tf_grenade1");
		found_grenade2 += !strcmp(definition->command, "+tf_grenade2");
		found_throw += !strcmp(definition->command, "+tf_throwgren");
		found_bind_help += !strcmp(definition->command, "+showmybinds");
	}
	CHECK(found_language == 1 && found_grenade_mode == 1,
		"language and three-mode grenade settings are defined exactly once");
	CHECK(found_player_name == 1, "player name uses the QuakeWorld 15-byte limit");
	CHECK(found_grenade1 == 1 && found_grenade2 == 1 && found_throw == 1,
		"all three native grenade actions are defined exactly once");
	CHECK(found_bind_help == 1, "held bind help action is defined exactly once");
	CHECK(CFGDictionary_ApplyToModel(&dictionary, &model, &applied, error, sizeof(error)),
		error[0] ? error : "dictionaries apply to file-backed model");
	CHECK(applied.setting_nodes == 67, "all requested main and class setting nodes are matched");
	CHECK(applied.bind_nodes == 66, "all current global and class bind nodes are matched");

	for (file_index = 0; file_index < model.file_count; ++file_index) {
		cfg_model_file_t *file = &model.files[file_index];
		for (node_index = 0; node_index < file->document.node_count; ++node_index) {
			cfg_node_t *node = &file->document.nodes[node_index];
			if (!strcmp(file->id, "main") && node->managed)
				main_managed++;
			if (!strcmp(file->id, "hud") && node->managed) hud_managed++;
			if (node->kind == CFG_NODE_ALIAS && !strncmp(file->id, "class_", 6)) {
				CHECK(!node->managed, "class alias definitions stay in Misc");
			}
			if (node->kind == CFG_NODE_BIND && (!strcmp(file->id, "binds")
				|| !strncmp(file->id, "class_", 6))) {
				bind_nodes++;
				CHECK(node->managed, "every current global and class bind is dictionary-managed");
			}
			if (node->kind == CFG_NODE_SETINFO && !strncmp(file->id, "class_", 6)) {
				CHECK(node->managed, "every current class setinfo is dictionary-managed");
			}
		}
	}
	CHECK(main_managed == 21, "settings.cfg exposes language, grenade mode, and the requested setting block");
	CHECK(hud_managed == 0, "hud.cfg remains entirely outside the settings dictionary");
	CHECK(bind_nodes == 66, "test corpus contains the expected number of bind nodes");
	CHECK(CFGModel_SerializeFile(&model, "hud", &hud_before, &hud_before_length),
		"complete raw HUD document can be loaded into textarea");
	CHECK(CFGModel_ReplaceFileContents(&model, "hud", hud_before, hud_before_length),
		"complete raw HUD textarea can replace its backing document");
	CHECK(CFGModel_SerializeFile(&model, "hud", &hud_after, &hud_after_length),
		"replaced HUD document serializes");
	CHECK(hud_after_length == hud_before_length
		&& !memcmp(hud_after, hud_before, hud_before_length),
		"unchanged full-file HUD textarea preserves every byte");
	free(hud_before);
	free(hud_after);

	CFGDictionary_Free(&dictionary);
	CFGModel_Free(&model);
}

static void TestInstalledGameRoot(const char *game_root)
{
	static const char *config_names[] = {
		"binds.cfg", "demoman.cfg", "engineer.cfg", "hud.cfg", "hwguy.cfg",
		"medic.cfg", "pyro.cfg", "scout.cfg", "settings.cfg", "sniper.cfg",
		"soldier.cfg", "spy.cfg"
	};
	size_t index;

	TestManagedFilesManifest(game_root);
	TestRealDictionaries(game_root);
	for (index = 0; index < sizeof(config_names) / sizeof(config_names[0]); ++index) {
		char path[1024];
		size_t length;
		unsigned char *data;
		int written = snprintf(path, sizeof(path), "%s/fortress/%s", game_root, config_names[index]);
		CHECK(written >= 0 && (size_t)written < sizeof(path), "installed CFG path fits test buffer");
		if (written < 0 || (size_t)written >= sizeof(path)) continue;
		data = ReadFile(path, &length);
		if (data) {
			CheckRoundTrip(path, data, length);
			free(data);
		}
	}
}

int main(int argc, char **argv)
{
	TestSyntheticDocument();
	TestFileBackedModel();
	if (argc == 3 && !strcmp(argv[1], "--game-root")) {
		TestInstalledGameRoot(argv[2]);
	}
	else if (argc != 1) {
		fprintf(stderr, "usage: %s [--game-root <installed-game-root>]\n", argv[0]);
		return 2;
	}

	if (failures) {
		fprintf(stderr, "%d test(s) failed\n", failures);
		return 1;
	}
	printf("All CFG editor parser tests passed.\n");
	return 0;
}
