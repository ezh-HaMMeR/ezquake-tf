#ifndef EZQUAKE_CTRL_KEYCAPTURE_H
#define EZQUAKE_CTRL_KEYCAPTURE_H

#define KEY_CAPTURE_MAX_KEYS 4

typedef struct key_capture_control_s {
	int keys[KEY_CAPTURE_MAX_KEYS];
	int key_count;
	int max_keys;
	qbool capturing;
} key_capture_control_t;

void CKeyCapture_Init(key_capture_control_t *control, int max_keys);
void CKeyCapture_SetKeys(key_capture_control_t *control, const int *keys, int key_count);
void CKeyCapture_Draw(const key_capture_control_t *control, int x, int y, int width, qbool active);
qbool CKeyCapture_Key(key_capture_control_t *control, int key);
void CKeyCapture_Begin(key_capture_control_t *control);
void CKeyCapture_Cancel(key_capture_control_t *control);

#endif
