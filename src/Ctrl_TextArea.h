#ifndef EZQUAKE_CTRL_TEXTAREA_H
#define EZQUAKE_CTRL_TEXTAREA_H

typedef struct textarea_source_line_s {
	char file[64];
	size_t node_id;
	int source_line;
} textarea_source_line_t;

typedef struct textarea_control_s {
	char *text;
	size_t length;
	size_t capacity;
	size_t cursor;
	int width;
	int height;
	int first_row;
	int first_column;
	qbool read_only;
	qbool show_source_gutter;
	qbool use_console_font;
	textarea_source_line_t *sources;
	size_t source_count;
} textarea_control_t;

void CTextArea_Init(textarea_control_t *control, int width, int height);
void CTextArea_Free(textarea_control_t *control);
qbool CTextArea_SetText(textarea_control_t *control, const char *text, size_t length);
qbool CTextArea_SetSources(textarea_control_t *control,
	const textarea_source_line_t *sources, size_t source_count);
void CTextArea_Draw(textarea_control_t *control, int x, int y, qbool active);
qbool CTextArea_Key(textarea_control_t *control, int key, wchar unichar);
const char *CTextArea_Text(const textarea_control_t *control, size_t *length);

#endif
