// This file is auto-generated, do not edit it by hand.
// Generated 2024-12-23 08:44:57.887882+00:00

#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct gliph_t{
    const wchar_t key;
    const uint8_t * const data;
    const struct gliph_t * const next;
} gliph_t;

typedef struct {
    const uint16_t height;
    const uint16_t width;
    const uint16_t baseline;
    const gliph_t * const gliph;
} font_t;

extern const uint8_t *font_gliph(const font_t *font, const wchar_t ch);

extern const uint8_t fira_code_data[];
extern const font_t fira_code;

#define FONT_MAX_GLIPH_SIZE 2304
