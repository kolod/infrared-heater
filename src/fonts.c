#include "config.h"
#include "fonts.h"


const uint8_t *font_gliph(const font_t *font, const wchar_t ch) {
    const gliph_t *pointer = font->gliph;
    while (pointer != NULL) {
        if (pointer->key == ch) return pointer->data;
        pointer = pointer->next;
    }
    return NULL;
}
