#pragma once

char* startFileChooser(const char* extensions[], bool romExtensions, bool canQuit=false);
bool isFileChooserOn();
void saveFileChooserStatus();
void loadFileChooserStatus();

void selectBorder();
