#ifndef menu_h
#define menu_h

#include "client/menu/qmenu.h"

typedef void (*MenuDrawFunc)();
typedef const char * (*MenuKeyFunc)(int key);

float ClampCvar(float min, float max, float value);

void M_Banner(char *name);
void M_ForceMenuOff();
void M_PopMenu();
void M_PushMenu(MenuDrawFunc draw, MenuKeyFunc key);

const char* Default_MenuKey(menuframework_s *m, int key);
void Key_ClearTyping();

extern char *m_popup_string;
extern int m_popup_endtime;
void M_Popup();

void M_DrawTextBox(int x, int y, int width, int lines);
void M_DrawCursor(int x, int y, int f);

// Number of the frames of the spinning quake logo.
#define NUM_CURSOR_FRAMES 15

extern char *menu_in_sound;
extern char *menu_move_sound;
extern char *menu_out_sound;
extern qboolean m_entersound; // play after drawing a frame, so caching won't disrupt the sound

extern qboolean MenuVideo_restartNeeded;

extern menuframework_s s_multiplayer_menu;

void MenuAddressBook_enter();
void MenuCredits_enter();
void MenuDMOptions_enter();
void MenuDownloadOptions_enter();
void MenuGame_enter();

void MenuGraphics_init();
void MenuGraphics_enter();
void MenuGraphics_apply();

void MenuJoinServer_enter();
void MenuKeys_enter();
void MenuLoadGame_enter();
void MenuMain_enter();
void MenuMouse_enter();
void MenuMultiplayer_enter();
void MenuOptions_enter();
void MenuPlayerConfig_enter();
void MenuQuit_enter();
void MenuSaveGame_enter();
void MenuSound_enter();
void MenuStartServer_enter();
void MenuStick_enter();
void MenuVideo_enter();

#endif
