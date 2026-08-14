#ifndef EZQUAKE_CTRL_CHECKBOX_H
#define EZQUAKE_CTRL_CHECKBOX_H

typedef struct checkbox_control_s {
	qbool checked;
	const char *checked_value;
	const char *unchecked_value;
} checkbox_control_t;

void CCheckbox_Init(checkbox_control_t *control, qbool checked,
	const char *checked_value, const char *unchecked_value);
void CCheckbox_Draw(const checkbox_control_t *control, int x, int y, qbool active);
qbool CCheckbox_Key(checkbox_control_t *control, int key);
const char *CCheckbox_Value(const checkbox_control_t *control);

#endif
