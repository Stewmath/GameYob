#include <stdarg.h>

void initConsole();
void enterConsole();
void exitConsole();
bool isConsoleEnabled();
int displayConsole();
void addToLog(const char *format, va_list args);
