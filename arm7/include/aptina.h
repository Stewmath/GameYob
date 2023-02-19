/*
 * Thanks to Epicpkmn11: https://github.com/Epicpkmn11/dsi-camera/
 * Original code published under public domain (Unlicense)
 */

#ifndef APTINA_H
#define APTINA_H

#include "fifo_vars.h"

#include <nds.h>

#ifdef __cplusplus
extern "C" {
#endif

void init(u8 device);
void activate(u8 device);
void deactivate(u8 device);
void setMode(CaptureMode mode);

#ifdef __cplusplus
}
#endif

#endif // APTINA_H
