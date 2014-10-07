class Gameboy;

extern Gameboy* gameboy;
extern Gameboy* gb2;

void mgr_init();
void mgr_runFrame();
void mgr_startGb2(const char* filename);
void mgr_swapFocus();
void mgr_setInternalClockGb(Gameboy* g);

void mgr_loadRom(const char* filename);
void mgr_selectRom();
void mgr_save();

void mgr_updateVBlank();

void mgr_exit();
