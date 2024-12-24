
#pragma once
#include "config.h"
#include "fonts.h"

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF

#define COLOR_RED     0xF800 
#define COLOR_GREEN   0x07C0
#define COLOR_BLUE    0x001F

// Extract Red, Green, and Blue components from RGB565 
#define GET_R_FROM_RGB565(rgb565) (((rgb565 >> 11) & 0x1F) * 255 / 31) 
#define GET_G_FROM_RGB565(rgb565) (((rgb565 >> 5) & 0x3F) * 255 / 63) 
#define GET_B_FROM_RGB565(rgb565) ((rgb565 & 0x1F) * 255 / 31) 

// Pack Red, Green, and Blue components into RGB565 
#define PACK_RGB565(r, g, b) (((r * 31 / 255) << 11) | ((g * 63 / 255) << 5) | (b * 31 / 255))

extern void display_fill_screen(uint16_t color);
extern void display_fill_rect(uint16_t color, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom);
extern void display_draw_rect(uint16_t color, uint16_t border_color, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom);
extern void display_draw_text(const font_t *font, uint16_t color, uint16_t back_color, uint16_t x, uint16_t y, wchar_t *text);

extern void display_setup(void);
