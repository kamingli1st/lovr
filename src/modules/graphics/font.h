#include "data/rasterizer.h"
#include "lib/map/map.h"
#include <stdbool.h>
#include <stdint.h>

#pragma once

struct Rasterizer;
struct Texture;

typedef map_t(Glyph) map_glyph_t;

typedef enum {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
} HorizontalAlign;

typedef enum {
  ALIGN_TOP,
  ALIGN_MIDDLE,
  ALIGN_BOTTOM
} VerticalAlign;

typedef struct {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t rowHeight;
  uint32_t padding;
  map_glyph_t glyphs;
} FontAtlas;

typedef struct Font {
  struct Rasterizer* rasterizer;
  struct Texture* texture;
  FontAtlas atlas;
  map_int_t kerning;
  float lineHeight;
  float pixelDensity;
  bool flip;
} Font;

Font* lovrFontInit(Font* font, struct Rasterizer* rasterizer);
#define lovrFontCreate(...) lovrFontInit(lovrAlloc(Font), __VA_ARGS__)
void lovrFontDestroy(void* ref);
struct Rasterizer* lovrFontGetRasterizer(Font* font);
void lovrFontRender(Font* font, const char* str, size_t length, float wrap, HorizontalAlign halign, float* vertices, uint16_t* indices, uint16_t baseVertex);
void lovrFontMeasure(Font* font, const char* string, size_t length, float wrap, float* width, uint32_t* lineCount, uint32_t* glyphCount);
float lovrFontGetHeight(Font* font);
float lovrFontGetAscent(Font* font);
float lovrFontGetDescent(Font* font);
float lovrFontGetBaseline(Font* font);
float lovrFontGetLineHeight(Font* font);
void lovrFontSetLineHeight(Font* font, float lineHeight);
bool lovrFontIsFlipEnabled(Font* font);
void lovrFontSetFlipEnabled(Font* font, bool flip);
int32_t lovrFontGetKerning(Font* font, unsigned int a, unsigned int b);
float lovrFontGetPixelDensity(Font* font);
void lovrFontSetPixelDensity(Font* font, float pixelDensity);
Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint);
void lovrFontAddGlyph(Font* font, Glyph* glyph);
void lovrFontExpandTexture(Font* font);
void lovrFontCreateTexture(Font* font);
