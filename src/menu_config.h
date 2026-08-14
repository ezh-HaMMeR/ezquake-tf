#ifndef EZQUAKE_MENU_CONFIG_H
#define EZQUAKE_MENU_CONFIG_H

#include "keys.h"

void Menu_Config_Init(void);
void Menu_Config_Shutdown(void);
void Menu_Config_Enter(void);
void Menu_Config_Draw(void);
void Menu_Config_Key(int key, wchar unichar);
qbool Menu_Config_Mouse_Event(const mouse_state_t *ms);
qbool Menu_Config_IsCapturingKey(void);

#endif
