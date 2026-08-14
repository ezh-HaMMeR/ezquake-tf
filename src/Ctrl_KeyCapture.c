#include "quakedef.h"
#include "Ctrl_KeyCapture.h"
#include "keys.h"

static int CKeyCapture_ModifiedKey(int key)
{
	if (key < 0 || key >= UNKNOWN || Key_IsModifierKeyNumber(key)) {
		return key;
	}
	if (keydown[K_CTRL] || keydown[K_LCTRL] || keydown[K_RCTRL]) {
		return Key_ComboKeynum(KEY_COMBO_CTRL, key);
	}
	if (keydown[K_ALT] || keydown[K_LALT] || keydown[K_RALT]) {
		return Key_ComboKeynum(KEY_COMBO_ALT, key);
	}
	if (keydown[K_SHIFT] || keydown[K_LSHIFT] || keydown[K_RSHIFT]) {
		return Key_ComboKeynum(KEY_COMBO_SHIFT, key);
	}
	return key;
}

void CKeyCapture_Init(key_capture_control_t *control, int max_keys)
{
	memset(control, 0, sizeof(*control));
	control->max_keys = bound(1, max_keys, KEY_CAPTURE_MAX_KEYS);
}

void CKeyCapture_SetKeys(key_capture_control_t *control, const int *keys, int key_count)
{
	control->key_count = bound(0, key_count, control->max_keys);
	if (control->key_count) {
		memcpy(control->keys, keys, control->key_count * sizeof(control->keys[0]));
	}
}

void CKeyCapture_Draw(const key_capture_control_t *control, int x, int y, int width, qbool active)
{
	char text[256] = { 0 };
	char output[288];
	int i;

	if (control->capturing) {
		strlcpy(text, "<press key>", sizeof(text));
	}
	else if (!control->key_count) {
		strlcpy(text, "<unbound>", sizeof(text));
	}
	else {
		for (i = 0; i < control->key_count; ++i) {
			if (i) strlcat(text, ", ", sizeof(text));
			strlcat(text, Key_KeynumToString(control->keys[i]), sizeof(text));
		}
	}

	snprintf(output, sizeof(output), active ? "[%*.*s]" : " %-*.*s ",
		width, width, text, width, width, text);
	Draw_String(x, y, output);
}

void CKeyCapture_Begin(key_capture_control_t *control)
{
	control->capturing = true;
}

void CKeyCapture_Cancel(key_capture_control_t *control)
{
	control->capturing = false;
}

qbool CKeyCapture_Key(key_capture_control_t *control, int key)
{
	int captured;

	if (!control->capturing) {
		if (key == K_ENTER || key == K_MOUSE1) {
			control->capturing = true;
			return true;
		}
		if (key == K_BACKSPACE || key == K_DEL) {
			control->key_count = 0;
			return true;
		}
		return false;
	}

	if (key == K_ESCAPE) {
		control->capturing = false;
		return true;
	}
	if (Key_IsModifierKeyNumber(key)) {
		return true;
	}

	captured = CKeyCapture_ModifiedKey(key);
	if (captured < 0 || captured >= KEY_MAX_KEYS) {
		return true;
	}
	if (control->key_count < control->max_keys) {
		control->keys[control->key_count++] = captured;
	}
	else {
		memmove(control->keys, control->keys + 1,
			(control->max_keys - 1) * sizeof(control->keys[0]));
		control->keys[control->max_keys - 1] = captured;
	}
	control->capturing = false;
	return true;
}
