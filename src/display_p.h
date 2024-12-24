#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "fonts.h"

#define DISPLAY_COMMAND_FILL_SCREEN   0x00
#define DISPLAY_COMMAND_FILL_RECT     0x01
#define DISPLAY_COMMAND_DRAW_RECT     0x02

#define DISPLAY_COMMAND_DRAW_TEXT     0x10

typedef struct {
    uint16_t color;
} fill_screen_t;

typedef struct {
    uint16_t color;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
} fill_rect_t;

typedef struct {
    uint16_t color;
    uint16_t border_color;
    uint16_t left;
    uint16_t right;
    uint16_t top;
    uint16_t bottom;
} draw_rect_t;

typedef struct {
    const font_t *font;
    uint16_t fore_color;
    uint16_t back_color;
    uint16_t left;
    uint16_t top;
    wchar_t text[16];
} draw_text_t;

typedef struct {
    uint8_t id;
    union {
        fill_screen_t fill_screen;
        fill_rect_t fill_rect;
        draw_rect_t draw_rect;
        draw_text_t draw_text;
    };
} display_command_t;

typedef union {
    uint32_t raw;
    struct {
        uint8_t a;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };        
} rgb888_t;

typedef union {
    uint16_t raw;
    struct {
        uint16_t b:5;
        uint16_t g:6;
        uint16_t r:5;
    };        
} rgb565_t;

void _display_set_window(size_t left, size_t right, size_t top, size_t bottom);
void _display_color_fill_dma(size_t left, size_t right, size_t top, size_t bottom, uint16_t color);
void _display_fill_screen_dma(uint16_t color);
void _display_copy_dma(size_t left, size_t right, size_t top, size_t bottom);
void _display_draw_rect(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom, uint16_t fore_color, uint16_t border_color);
void _display_draw_text(const font_t *font, uint16_t left, uint16_t top, uint16_t fore_color, uint16_t back_color, wchar_t *text, size_t length);
uint16_t _mix_colors(uint16_t fore_color, uint16_t back_color, uint8_t alpha);


void _display_task(void *pvParameters);
