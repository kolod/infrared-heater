
#pragma once
#include "config.h"
#include "st7789.h"

extern void display_set_window(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom);
extern void display_color_fill(uint16_t color, size_t length);
extern void display_color_fill_dma(uint16_t color, size_t length);
extern void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
extern void display_fill_screen(uint16_t color);
extern void display_draw_filed_rect(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom, uint16_t color);
extern void display_draw_rect(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom, uint16_t border_color, uint16_t fore_color);

extern void display_task(void *pvParameters);

extern TaskHandle_t hDisplayTask;
