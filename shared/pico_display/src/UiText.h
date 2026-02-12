#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <stdint.h>

// Minimal framebuffer text renderer for RGB565 panels.
void UiInit(uint16_t* frameBuffer, int width, int height);
void UiClear(uint16_t color);
void UiFillRect(int x, int y, int w, int h, uint16_t color);
void UiDrawRect(int x, int y, int w, int h, uint16_t color);
void UiDrawText(int x, int y, const char* s, uint16_t fg, uint16_t bg);
void UiDrawTextLarge(int x, int y, const char* s, uint16_t fg, uint16_t bg);
void UiDrawTextMedium(int x, int y, const char* s, uint16_t fg, uint16_t bg);
void UiDrawTextGiant(int x, int y, const char* s, uint16_t fg, uint16_t bg);
void UiDrawTextClipped(int x, int y, int maxChars, const char* s, uint16_t fg, uint16_t bg);

#endif // UI_TEXT_H
