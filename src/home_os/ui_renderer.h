#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <Arduino.h>

void renderHome();
void renderStatusBar();
void renderGameSelector(int index, const char* name);

#endif // UI_RENDERER_H
