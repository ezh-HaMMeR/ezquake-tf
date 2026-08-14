#include "quakedef.h"
#include "utils.h"
#include "Ctrl_TextArea.h"
#include "keys.h"
#include "textencoding.h"

static void CTextArea_DrawText(int x, int y, const char *text, qbool console_font)
{
	wchar wide[1024];
	int input = 0, output = 0, length;

	if (!console_font) {
		Draw_String(x, y, text);
		return;
	}
	length = (int)strlen(text);
	while (input < length && output < (int)(sizeof(wide) / sizeof(wide[0])) - 1) {
		int initial = input;
		wchar decoded = TextEncodingDecodeUTF8((char *)text, &input);
		if (!decoded && text[initial]) {
			decoded = (unsigned char)text[initial];
			input = initial;
		}
		wide[output++] = decoded;
		++input;
	}
	wide[output] = 0;
	// Textareas use the alternate brown/gold gradient and the system FreeType
	// face when it is available. The renderer falls back to the console charset.
	Draw_ConsoleString(x, y, wide, NULL, 0, true, 1, true);
}

static qbool CTextArea_Reserve(textarea_control_t *control, size_t required)
{
	char *resized;
	size_t capacity = max((size_t)256, control->capacity);

	while (capacity < required) capacity *= 2;
	if (capacity == control->capacity) return true;
	resized = Q_realloc(control->text, capacity);
	if (!resized) return false;
	control->text = resized;
	control->capacity = capacity;
	return true;
}

static size_t CTextArea_LineStart(const textarea_control_t *control, size_t position)
{
	while (position && control->text[position - 1] != '\n') --position;
	return position;
}

static size_t CTextArea_LineEnd(const textarea_control_t *control, size_t position)
{
	while (position < control->length && control->text[position] != '\n') ++position;
	if (position && control->text[position - 1] == '\r') --position;
	return position;
}

static int CTextArea_RowAt(const textarea_control_t *control, size_t position)
{
	int row = 0;
	size_t i;
	for (i = 0; i < position && i < control->length; ++i) row += control->text[i] == '\n';
	return row;
}

static size_t CTextArea_RowStart(const textarea_control_t *control, int wanted_row)
{
	int row = 0;
	size_t i;
	if (wanted_row <= 0) return 0;
	for (i = 0; i < control->length; ++i) {
		if (control->text[i] == '\n' && ++row == wanted_row) return i + 1;
	}
	return control->length;
}

static void CTextArea_EnsureVisible(textarea_control_t *control)
{
	int row = CTextArea_RowAt(control, control->cursor);
	int column = (int)(control->cursor - CTextArea_LineStart(control, control->cursor));
	if (row < control->first_row) control->first_row = row;
	if (row >= control->first_row + control->height) control->first_row = row - control->height + 1;
	if (column < control->first_column) control->first_column = column;
	if (column >= control->first_column + control->width) control->first_column = column - control->width + 1;
}

static qbool CTextArea_InsertSourceRows(textarea_control_t *control, const char *bytes, size_t length)
{
	textarea_source_line_t current = { 0 }, upper = { 0 }, *resized;
	size_t newline_count = 0, row, insert_at, index;
	qbool at_line_start;

	if (!control->show_source_gutter || !control->source_count) return true;
	row = (size_t)CTextArea_RowAt(control, control->cursor);
	if (row >= control->source_count) {
		textarea_source_line_t appended = control->sources[control->source_count - 1];
		resized = Q_realloc(control->sources, (row + 1) * sizeof(*control->sources));
		if (!resized) return false;
		control->sources = resized;
		appended.source_line = 0;
		appended.inserted = true;
		while (control->source_count <= row)
			control->sources[control->source_count++] = appended;
	}
	for (index = 0; index < length; ++index) newline_count += bytes[index] == '\n';
	if (!newline_count) return true;

	at_line_start = control->cursor == CTextArea_LineStart(control, control->cursor);
	current = control->sources[min(row, control->source_count - 1)];
	upper = row ? control->sources[min(row - 1, control->source_count - 1)] : current;
	insert_at = min(row + 1, control->source_count);
	resized = Q_realloc(control->sources,
		(control->source_count + newline_count) * sizeof(*control->sources));
	if (!resized) return false;
	control->sources = resized;
	memmove(control->sources + insert_at + newline_count, control->sources + insert_at,
		(control->source_count - insert_at) * sizeof(*control->sources));

	if (at_line_start && row < control->source_count) {
		textarea_source_line_t inserted = row ? upper : current;
		inserted.source_line = 0;
		inserted.inserted = true;
		control->sources[row] = inserted;
		for (index = 0; index < newline_count; ++index) {
			if (index + 1 == newline_count) control->sources[insert_at + index] = current;
			else control->sources[insert_at + index] = inserted;
		}
	}
	else {
		current.source_line = 0;
		current.inserted = true;
		for (index = 0; index < newline_count; ++index)
			control->sources[insert_at + index] = current;
	}
	control->source_count += newline_count;
	return true;
}

static void CTextArea_RemoveSourceRow(textarea_control_t *control, size_t row)
{
	if (!control->show_source_gutter || row >= control->source_count) return;
	memmove(control->sources + row, control->sources + row + 1,
		(control->source_count - row - 1) * sizeof(*control->sources));
	--control->source_count;
}

static qbool CTextArea_Insert(textarea_control_t *control, const char *bytes, size_t length)
{
	if (control->read_only || !length || !CTextArea_Reserve(control, control->length + length + 1)) return false;
	if (!CTextArea_InsertSourceRows(control, bytes, length)) return false;
	memmove(control->text + control->cursor + length, control->text + control->cursor,
		control->length - control->cursor + 1);
	memcpy(control->text + control->cursor, bytes, length);
	control->cursor += length;
	control->length += length;
	return true;
}

void CTextArea_Init(textarea_control_t *control, int width, int height)
{
	memset(control, 0, sizeof(*control));
	control->width = max(1, width);
	control->height = max(1, height);
	CTextArea_SetText(control, "", 0);
}

void CTextArea_Free(textarea_control_t *control)
{
	Q_free(control->text);
	Q_free(control->sources);
	memset(control, 0, sizeof(*control));
}

qbool CTextArea_SetText(textarea_control_t *control, const char *text, size_t length)
{
	if (!CTextArea_Reserve(control, length + 1)) return false;
	if (length) memcpy(control->text, text, length);
	control->text[length] = '\0';
	control->length = length;
	control->cursor = min(control->cursor, length);
	return true;
}

qbool CTextArea_SetSources(textarea_control_t *control,
	const textarea_source_line_t *sources, size_t source_count)
{
	textarea_source_line_t *copy = NULL;
	if (source_count) {
		copy = Q_malloc(source_count * sizeof(*copy));
		if (!copy) return false;
		memcpy(copy, sources, source_count * sizeof(*copy));
	}
	Q_free(control->sources);
	control->sources = copy;
	control->source_count = source_count;
	return true;
}

void CTextArea_Draw(textarea_control_t *control, int x, int y, qbool active)
{
	int visible_row;
	int gutter_chars = control->show_source_gutter ? 18 : 0;
	CTextArea_EnsureVisible(control);

	for (visible_row = 0; visible_row < control->height; ++visible_row) {
		int row = control->first_row + visible_row;
		size_t start = CTextArea_RowStart(control, row);
		size_t end = CTextArea_LineEnd(control, start);
		char line[1024];
		int text_width = bound(1, control->width - gutter_chars, (int)sizeof(line) - 1);
		size_t available = start + control->first_column < end ? end - start - control->first_column : 0;
		size_t copied = min((size_t)text_width, available);
		memset(line, ' ', text_width);
		if (copied) memcpy(line, control->text + start + control->first_column, copied);
		line[text_width] = '\0';

		if (gutter_chars) {
			char gutter[32];
			if ((size_t)row < control->source_count) {
				if (control->sources[row].inserted)
					snprintf(gutter, sizeof(gutter), "%-11.11s %5s ", control->sources[row].file, "+");
				else snprintf(gutter, sizeof(gutter), "%-11.11s %5d ", control->sources[row].file,
					control->sources[row].source_line);
			}
			else snprintf(gutter, sizeof(gutter), "%17s ", "");
			CTextArea_DrawText(x, y + visible_row * 8, gutter, control->use_console_font);
		}
		CTextArea_DrawText(x + gutter_chars * 8, y + visible_row * 8, line,
			control->use_console_font);
	}

	if (active) {
		int row = CTextArea_RowAt(control, control->cursor) - control->first_row;
		int column = (int)(control->cursor - CTextArea_LineStart(control, control->cursor)) - control->first_column;
		if (row >= 0 && row < control->height && column >= 0 && column < control->width - gutter_chars) {
			Draw_Character(x + (gutter_chars + column) * 8, y + row * 8,
				10 + ((int)(cls.realtime * 4) & 1));
		}
	}
}

qbool CTextArea_Key(textarea_control_t *control, int key, wchar unichar)
{
	size_t start, end, column, target;
	char character;

	switch (key) {
		case K_LEFTARROW: if (control->cursor) --control->cursor; break;
		case K_RIGHTARROW: if (control->cursor < control->length) ++control->cursor; break;
		case K_HOME: control->cursor = CTextArea_LineStart(control, control->cursor); break;
		case K_END: control->cursor = CTextArea_LineEnd(control, control->cursor); break;
		case K_UPARROW:
		case K_DOWNARROW:
			start = CTextArea_LineStart(control, control->cursor);
			column = control->cursor - start;
			target = CTextArea_RowStart(control, CTextArea_RowAt(control, control->cursor) + (key == K_UPARROW ? -1 : 1));
			end = CTextArea_LineEnd(control, target);
			control->cursor = min(target + column, end);
			break;
		case K_PGUP:
			control->first_row = max(0, control->first_row - control->height);
			control->cursor = CTextArea_RowStart(control, control->first_row);
			break;
		case K_PGDN:
			control->first_row += control->height;
			control->cursor = CTextArea_RowStart(control, control->first_row);
			break;
		case K_BACKSPACE:
			if (!control->read_only && control->cursor) {
				size_t removed = 1;
				if (control->text[control->cursor - 1] == '\n') {
					CTextArea_RemoveSourceRow(control, CTextArea_RowAt(control, control->cursor));
					if (control->cursor >= 2 && control->text[control->cursor - 2] == '\r') removed = 2;
				}
				memmove(control->text + control->cursor - removed, control->text + control->cursor,
					control->length - control->cursor + 1);
				control->cursor -= removed; control->length -= removed;
			}
			break;
		case K_DEL:
			if (!control->read_only && control->cursor < control->length) {
				size_t removed = 1;
				if (control->text[control->cursor] == '\r' && control->cursor + 1 < control->length &&
					control->text[control->cursor + 1] == '\n') removed = 2;
				if (control->text[control->cursor + removed - 1] == '\n')
					CTextArea_RemoveSourceRow(control, CTextArea_RowAt(control, control->cursor) + 1);
				memmove(control->text + control->cursor, control->text + control->cursor + removed,
					control->length - control->cursor - removed + 1);
				control->length -= removed;
			}
			break;
		case K_ENTER:
			if (strstr(control->text, "\r\n")) CTextArea_Insert(control, "\r\n", 2);
			else { character = '\n'; CTextArea_Insert(control, &character, 1); }
			break;
		case 'v': case 'V':
			if (keydown[K_CTRL] && !control->read_only) {
				const char *clipboard = ReadFromClipboard();
				if (clipboard) CTextArea_Insert(control, clipboard, strlen(clipboard));
			}
			break;
		default:
			if (!keydown[K_CTRL] && !keydown[K_ALT] && key >= ' ' && key <= '}' && unichar <= 255) {
				character = (char)unichar;
				CTextArea_Insert(control, &character, 1);
			}
			else return false;
	}
	CTextArea_EnsureVisible(control);
	return true;
}

const char *CTextArea_Text(const textarea_control_t *control, size_t *length)
{
	if (length) *length = control->length;
	return control->text;
}
