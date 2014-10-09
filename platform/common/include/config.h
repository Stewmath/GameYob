#pragma once

// Function keys
enum {
    FUNC_KEY_NONE,
    FUNC_KEY_A, FUNC_KEY_B, FUNC_KEY_LEFT, FUNC_KEY_RIGHT, FUNC_KEY_UP, FUNC_KEY_DOWN, FUNC_KEY_START, FUNC_KEY_SELECT,
    FUNC_KEY_MENU, FUNC_KEY_MENU_PAUSE, FUNC_KEY_SAVE, FUNC_KEY_AUTO_A, FUNC_KEY_AUTO_B, FUNC_KEY_FAST_FORWARD, FUNC_KEY_FAST_FORWARD_TOGGLE,
    FUNC_KEY_SCALE, FUNC_KEY_RESET,

    NUM_FUNC_KEYS,
};

// Menu keys
enum {
    MENU_KEY_A, MENU_KEY_B, MENU_KEY_UP, MENU_KEY_DOWN, MENU_KEY_LEFT, MENU_KEY_RIGHT,
    MENU_KEY_L, MENU_KEY_R, MENU_KEY_X, MENU_KEY_Y,

    NUM_MENU_KEYS,
};



bool readConfigFile();
void writeConfigFile();

void startKeyConfigChooser();

int mapFuncKey(int gbKey); // Maps a functional key to a physical key.
int mapMenuKey(int menuKey);

