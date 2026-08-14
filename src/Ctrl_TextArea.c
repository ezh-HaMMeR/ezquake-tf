#include "quakedef.h"
#include "utils.h"
#include "Ctrl_TextArea.h"
#include "keys.h"

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

static qbool CTextArea_Insert(textarea_control_t *control, const char *bytes, size_t length)
{
	if (control->read_only || !length || !CTextArea_Reserve(control, control->length + length + 1)) return false;
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
				snprintf(gutter, sizeof(gutter), "%-11.11s %5d ", control->sources[row].file,
					control->sources[row].source_line);
			}
			else snprintf(gutter, sizeof(gutter), "%17s ", "");
			Draw_String(x, y + visible_row * 8, gutter);
		}
		Draw_String(x + gutter_chars * 8, y + visible_row * 8, line);
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
				memmove(control->text + control->cursor - 1, control->text + control->cursor,
					control->length - control->cursor + 1);
				--control->cursor; --control->length;
			}
			break;
		case K_DEL:
			if (!control->read_only && control->cursor < control->length) {
				memmove(control->text + control->cursor, control->text + control->cursor + 1,
					control->length - control->cursor);
				--control->length;
			}
			break;
		case K_ENTER:
			character = '\n';
			CTextArea_Insert(control, &character, 1);
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
