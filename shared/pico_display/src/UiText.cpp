#include "UiText.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

namespace {

struct Glyph5x7 {
  char c;
  // Each row uses lower 5 bits (MSB-left in drawChar()).
  uint8_t rows[7];
};

static const Glyph5x7 kGlyphs[] = {
  {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
  {'#', {0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00}},
  {'!', {0x06, 0x04, 0x0C, 0x04, 0x06, 0x02, 0x04}},
  {'"', {0x06, 0x09, 0x1F, 0x0E, 0x0A, 0x04, 0x0A}},
  {'%', {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}},
  {'$', {0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00, 0x00}},
  {'&', {0x04, 0x0A, 0x0A, 0x0A, 0x04, 0x0E, 0x0E}},
  {'*', {0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00}},
  {'+', {0x04, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x04}},
  {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
  {'/', {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}},
  {':', {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00}},
  {';', {0x06, 0x09, 0x06, 0x0C, 0x12, 0x1F, 0x00}},
  {',', {0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x04}},
  {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}},
  {'?', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
  {'=', {0x00, 0x06, 0x09, 0x11, 0x1F, 0x0E, 0x00}},
  {'[', {0x04, 0x04, 0x04, 0x04, 0x0E, 0x04, 0x00}},
  {']', {0x04, 0x0E, 0x04, 0x04, 0x04, 0x04, 0x00}},
  {'~', {0x00, 0x1F, 0x00, 0x0E, 0x00, 0x1F, 0x00}},
  {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
  {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
  {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
  {'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
  {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
  {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
  {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
  {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
  {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
  {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
  {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
  {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
  {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
  {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
  {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
  {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
  {'G', {0x0E, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0E}},
  {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
  {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
  {'J', {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
  {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
  {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
  {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
  {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
  {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
  {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
  {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
  {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
  {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
  {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
  {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
  {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
  {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
  {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
  {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
  {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

// Framebuffer pointer comes from LCD driver and uses RGB565 words.
static uint16_t* sFb = nullptr;
static int sWidth = 0;
static int sHeight = 0;

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;
constexpr int kCellW = 6;
constexpr int kCellH = 8;
constexpr int kLargeScale = 2;
constexpr int kMediumScale = 3;
constexpr int kGiantScale = 7;

static inline uint16_t lcdColor(uint16_t color) {
  // Waveshare ST7789 path expects byte-swapped 16-bit RGB565 in memory.
  return (uint16_t)(((color << 8) & 0xFF00u) | ((color >> 8) & 0x00FFu));
}

static inline bool inRange(int x, int y) {
  return x >= 0 && y >= 0 && x < sWidth && y < sHeight;
}

static inline void putPixel(int x, int y, uint16_t color) {
  if (!sFb || !inRange(x, y)) return;
  sFb[(size_t)y * (size_t)sWidth + (size_t)x] = color;
}

static const uint8_t* glyphExact(char ch) {
  // Font is uppercase-only, so normalize once here.
  const char c = (char)toupper((unsigned char)ch);
  for (size_t i = 0; i < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++i) {
    if (kGlyphs[i].c == c) {
      return kGlyphs[i].rows;
    }
  }
  return nullptr;
}

static const uint8_t* glyphFor(char ch) {
  const uint8_t* exact = glyphExact(ch);
  if (exact) return exact;

  // Unknown characters render as '?' rather than disappearing.
  for (size_t i = 0; i < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++i) {
    if (kGlyphs[i].c == '?') {
      return kGlyphs[i].rows;
    }
  }
  return nullptr;
}

static void drawCharScaled(int x, int y, char ch, uint16_t fg, uint16_t bg, int scale) {
  if (scale <= 0) return;
  const uint8_t* g = glyphFor(ch);
  if (!g) return;

  const int cellW = kCellW * scale;
  const int cellH = kCellH * scale;
  for (int ry = 0; ry < cellH; ++ry) {
    const int gy = ry / scale;
    for (int rx = 0; rx < cellW; ++rx) {
      const int gx = rx / scale;
      // Draw in a 6x8 cell (scaled) so text has built-in inter-character spacing.
      bool on = false;
      if (gy < kGlyphH && gx < kGlyphW) {
        on = (g[gy] & (1u << (kGlyphW - 1 - gx))) != 0;
      }
      putPixel(x + rx, y + ry, on ? fg : bg);
    }
  }
}

static void drawTextScaled(int x, int y, const char* s, uint16_t fg, uint16_t bg, int scale) {
  if (!sFb || !s) return;
  const uint16_t f = lcdColor(fg);
  const uint16_t b = lcdColor(bg);
  int cx = x;
  const int advance = kCellW * scale;
  for (size_t i = 0; s[i] != '\0'; ++i) {
    drawCharScaled(cx, y, s[i], f, b, scale);
    cx += advance;
  }
}

static int textWidthScaled(const char* s, int scale) {
  if (!s || scale <= 0) return 0;
  return (int)strlen(s) * kCellW * scale;
}

} // namespace

void UiInit(uint16_t* frameBuffer, int width, int height) {
  sFb = frameBuffer;
  sWidth = width;
  sHeight = height;
}

void UiClear(uint16_t color) {
  UiFillRect(0, 0, sWidth, sHeight, color);
}

void UiFillRect(int x, int y, int w, int h, uint16_t color) {
  if (!sFb || w <= 0 || h <= 0) return;
  const int x0 = x < 0 ? 0 : x;
  const int y0 = y < 0 ? 0 : y;
  const int x1 = (x + w) > sWidth ? sWidth : (x + w);
  const int y1 = (y + h) > sHeight ? sHeight : (y + h);
  if (x1 <= x0 || y1 <= y0) return;

  const uint16_t c = lcdColor(color);
  // Fill by rows to keep writes linear and cache-friendly.
  for (int yy = y0; yy < y1; ++yy) {
    uint16_t* row = sFb + (size_t)yy * (size_t)sWidth;
    for (int xx = x0; xx < x1; ++xx) {
      row[xx] = c;
    }
  }
}

void UiDrawRect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  UiFillRect(x, y, w, 1, color);
  UiFillRect(x, y + h - 1, w, 1, color);
  UiFillRect(x, y, 1, h, color);
  UiFillRect(x + w - 1, y, 1, h, color);
}

void UiDrawText(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
  drawTextScaled(x, y, s, fg, bg, 1);
}

void UiDrawTextLarge(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
  drawTextScaled(x, y, s, fg, bg, kLargeScale);
}

void UiDrawTextMedium(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
  drawTextScaled(x, y, s, fg, bg, kMediumScale);
}

void UiDrawTextGiant(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
  drawTextScaled(x, y, s, fg, bg, kGiantScale);
}

void UiDrawTextClipped(int x, int y, int maxChars, const char* s, uint16_t fg, uint16_t bg) {
  if (!s || maxChars <= 0) return;
  const size_t len = strlen(s);
  if ((int)len <= maxChars) {
    UiDrawText(x, y, s, fg, bg);
    return;
  }

  char tmp[96];
  if (maxChars >= (int)sizeof(tmp)) maxChars = (int)sizeof(tmp) - 1;

  if (maxChars <= 3) {
    // Very tight label areas still get an obvious truncation marker.
    for (int i = 0; i < maxChars; ++i) tmp[i] = '.';
    tmp[maxChars] = '\0';
  } else {
    const int body = maxChars - 3;
    memcpy(tmp, s, (size_t)body);
    tmp[body + 0] = '.';
    tmp[body + 1] = '.';
    tmp[body + 2] = '.';
    tmp[body + 3] = '\0';
  }
  UiDrawText(x, y, tmp, fg, bg);
}

int UiTextWidth(const char* s) {
  return textWidthScaled(s, 1);
}

int UiTextWidthLarge(const char* s) {
  return textWidthScaled(s, kLargeScale);
}

int UiTextWidthMedium(const char* s) {
  return textWidthScaled(s, kMediumScale);
}

int UiTextWidthGiant(const char* s) {
  return textWidthScaled(s, kGiantScale);
}
