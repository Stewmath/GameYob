class Gameboy;

void mgr_init();
void mgr_run();
void mgr_startGb2(const char* filename);
void mgr_swapFocus();
void mgr_setInternalClockGb(Gameboy* g);

void mgr_loadRom(const char* filename);
void mgr_selectRom();
void mgr_save();
