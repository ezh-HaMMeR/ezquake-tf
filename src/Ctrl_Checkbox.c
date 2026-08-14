#include "quakedef.h"
#include "Ctrl_Checkbox.h"
#include "keys.h"

void CCheckbox_Init(checkbox_control_t *control, qbool checked,
	const char *checked_value, const char *unchecked_value)
{
	memset(control, 0, sizeof(*control));
	control->checked = checked;
	control->checked_value = checked_value ? checked_value : "1";
	control->unchecked_value = unchecked_value ? unchecked_value : "0";
}

void CCheckbox_Draw(const checkbox_control_t *control, int x, int y, qbool active)
{
	char text[8];

	snprintf(text, sizeof(text), active ? "[%c]" : " %c ", control->checked ? 'x' : ' ');
	Draw_String(x, y, text);
}

qbool CCheckbox_Key(checkbox_control_t *control, int key)
{
	if (key != K_ENTER && key != K_SPACE && key != K_MOUSE1) {
		return false;
	}
	control->checked = !control->checked;
	return true;
}

const char *CCheckbox_Value(const checkbox_control_t *control)
{
	return control->checked ? control->checked_value : control->unchecked_value;
}
