
#include "display_p.h"
#include "display.h"
#include "st7789.h"


TaskHandle_t hDisplayTask;
QueueHandle_t hDisplayQueue;
uint16_t display_dma_buffer[FONT_MAX_GLIPH_SIZE];
volatile size_t display_dma_pixels_to_transfer;

////////////////////////////////////////////////////////////////////////////////// INTERNAL ///

static inline void _display_reset_dma_interrupts(void) {
#if (DISPLAY_DMA_STREAM == DMA_STREAM0) || (DISPLAY_DMA_STREAM == DMA_STREAM1) || (DISPLAY_DMA_STREAM == DMA_STREAM2) || (DISPLAY_DMA_STREAM == DMA_STREAM3)
    DMA_LIFCR(DISPLAY_DMA) |= DMA_ISR_MASK(DISPLAY_DMA_STREAM);
#elif (DISPLAY_DMA_STREAM == DMA_STREAM4) || (DISPLAY_DMA_STREAM == DMA_STREAM5) || (DISPLAY_DMA_STREAM == DMA_STREAM6) || (DISPLAY_DMA_STREAM == DMA_STREAM7)
    DMA_HIFCR(DISPLAY_DMA) |= DMA_ISR_MASK(DISPLAY_DMA_STREAM);
#endif
}

static inline void _display_wait_spi(void) {
    while (!(SPI_SR(DISPLAY_SPI) & SPI_SR_TXE));
    while (SPI_SR(DISPLAY_SPI) & SPI_SR_BSY);
}

static inline void _display_set_command(void) {
    gpio_clear(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
}

static inline void _display_set_data(void) {
    gpio_set(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
}

static inline void _display_set_cs_high(void) {
    gpio_set(DISPLAY_SPI_PORT, DISPLAY_CS_PIN);
}

static inline void display_set_cs_low(void) {
    gpio_clear(DISPLAY_SPI_PORT, DISPLAY_CS_PIN);
}

static inline void _display_hard_reset(void) {
    gpio_clear(DISPLAY_RST_PORT, DISPLAY_RST_PIN);
    vTaskDelay(10);
    gpio_set(DISPLAY_RST_PORT, DISPLAY_RST_PIN);
    vTaskDelay(160);
}

static inline void _display_soft_reset(void) {
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    _display_set_command();
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_SWRESET);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(130);
}

static inline void _display_sleep_exit(void) {
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    _display_set_command();
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_SLPOUT);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(500);
}

static inline void _display_sleep_enter(void) {
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    _display_set_command();
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_SLPIN);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(500);
}

void _display_set_window(size_t left, size_t right, size_t top, size_t bottom) {
    left   += DISPLAY_OFFSET_X;
    right  += DISPLAY_OFFSET_X;
    top    += DISPLAY_OFFSET_Y;
    bottom += DISPLAY_OFFSET_Y;
 
    display_dma_pixels_to_transfer = (right - left + 1) * (bottom - top + 1); 

    _display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_CASET);
    _display_wait_spi();
    
    _display_set_data();
    spi_set_dff_16bit(DISPLAY_SPI);
    spi_write(DISPLAY_SPI, left);
    _display_wait_spi();
    spi_write(DISPLAY_SPI, right);
    _display_wait_spi();

    _display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    spi_write(DISPLAY_SPI, ST7789_RASET);
    _display_wait_spi();
    
    _display_set_data();
    spi_set_dff_16bit(DISPLAY_SPI);
    spi_write(DISPLAY_SPI, top);
    _display_wait_spi();
    spi_write(DISPLAY_SPI, bottom);
    _display_wait_spi();

    _display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    spi_write(DISPLAY_SPI, ST7789_RAMWR);
    _display_wait_spi();
    _display_set_data();
}

void _display_color_fill_dma(size_t left, size_t right, size_t top, size_t bottom, uint16_t color) {
    display_dma_buffer[0] = color;  
    
    _display_set_window(left, right, top, bottom);

    dma_set_memory_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, (uint32_t) display_dma_buffer);
    dma_disable_memory_increment_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_enable_transfer_complete_interrupt(DISPLAY_DMA, DISPLAY_DMA_STREAM); 

    if (display_dma_pixels_to_transfer > 0x0000FFFF) {
        dma_enable_double_buffer_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
        dma_set_memory_address_1(DISPLAY_DMA, DISPLAY_DMA_STREAM, (uint32_t) display_dma_buffer);
        dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, 0x0000FFFF);
        display_dma_pixels_to_transfer -= 0x0000FFFF;
    } else {
        dma_disable_double_buffer_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
        dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, display_dma_pixels_to_transfer);
        display_dma_pixels_to_transfer = 0;
    }
    
    dma_enable_stream(DISPLAY_DMA, DISPLAY_DMA_STREAM);

    spi_set_dff_16bit(DISPLAY_SPI);
    spi_enable_tx_dma(DISPLAY_SPI);

    // Nonblocking wait for dma transfer complete
    xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
}

void _display_fill_screen_dma(uint16_t color) {
    _display_color_fill_dma(0, DISPLAY_WIDTH, 0, DISPLAY_HEIGHT, color);
}

void _display_copy_dma(size_t left, size_t right, size_t top, size_t bottom) {
    _display_set_window(left, right, top, bottom);

    dma_set_memory_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, (uint32_t) display_dma_buffer);
    dma_enable_memory_increment_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_enable_transfer_complete_interrupt(DISPLAY_DMA, DISPLAY_DMA_STREAM);

    if (display_dma_pixels_to_transfer > 0x0000FFFF) {
        dma_enable_double_buffer_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
        dma_set_memory_address_1(DISPLAY_DMA, DISPLAY_DMA_STREAM, (uint32_t) display_dma_buffer);
        dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, 0x0000FFFF);
        display_dma_pixels_to_transfer -= 0x0000FFFF;
    } else {
        dma_disable_double_buffer_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
        dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, display_dma_pixels_to_transfer);
        display_dma_pixels_to_transfer = 0;
    }
    
    dma_enable_stream(DISPLAY_DMA, DISPLAY_DMA_STREAM);

    spi_set_dff_16bit(DISPLAY_SPI);
    spi_enable_tx_dma(DISPLAY_SPI);

    // Nonblocking wait for dma transfer complete
    xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
}

void _display_draw_rect(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom, uint16_t fore_color, uint16_t border_color) {
    _display_color_fill_dma(left  + 1, right-1  , top    - 1, bottom + 1, fore_color  );
    _display_color_fill_dma(left     , right    , top       , top    - 1, border_color);
    _display_color_fill_dma(left     , left  + 1, top    - 1, bottom + 1, border_color);
    _display_color_fill_dma(right - 1, right    , top    - 1, bottom + 1, border_color);
    _display_color_fill_dma(left     , right    , bottom    , bottom + 1, border_color);
}

uint16_t _mix_colors(uint16_t fore_color, uint16_t back_color, uint8_t alpha) {
    if (alpha == 0x00) return back_color;
    if (alpha == 0xFF) return fore_color;

    // Extract RGB components from RGB565 
    uint32_t fg_r = GET_R_FROM_RGB565(fore_color); 
    uint32_t fg_g = GET_G_FROM_RGB565(fore_color); 
    uint32_t fg_b = GET_B_FROM_RGB565(fore_color); 
    uint32_t bg_r = GET_R_FROM_RGB565(back_color); 
    uint32_t bg_g = GET_G_FROM_RGB565(back_color); 
    uint32_t bg_b = GET_B_FROM_RGB565(back_color);

    // Mix the colors using alpha
    uint8_t result_r = (uint8_t) (bg_r * (255 - alpha) / 255 + fg_r * alpha / 255); 
    uint8_t result_g = (uint8_t) (bg_g * (255 - alpha) / 255 + fg_g * alpha / 255); 
    uint8_t result_b = (uint8_t) (bg_b * (255 - alpha) / 255 + fg_b * alpha / 255);

    // Pack the result into RGB565 
    return PACK_RGB565(result_r, result_g, result_b);
}

void _display_draw_text(const font_t *font, uint16_t left, uint16_t top, uint16_t fore_color, uint16_t back_color, wchar_t *text, size_t length) {
    // TODO: Add bounds checks
    for (size_t i = 0; i < length; i++) {
        if (text == NULL) break;
        wchar_t wch = *text++;
        
        // Copy data to dma buffer and colorize
        const uint8_t *data = font_gliph(font, wch);
        size_t size = font->height * font->width;
        for (size_t j = 0; j < size ; j++) {
            display_dma_buffer[j] = _mix_colors(fore_color, back_color, data[j]);
        }

        _display_copy_dma(left, left + font->width - 1, top, top + font-> height - 1);
        left += font->width;
    }
}

/////////////////////////////////////////////////////////////////////////////////////// END ///

static inline void display_set_backlight(uint32_t value) {
    if (value) {
        gpio_set(DISPLAY_BL_PORT, DISPLAY_BL_PIN);
    } else {
        gpio_clear(DISPLAY_BL_PORT, DISPLAY_BL_PIN);
    }
}

// void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {    
//     if (x >= DISPLAY_WIDTH)  ASSERT("X must be in range 0..DISPLAY_WIDTH-1");    
//     if (y >= DISPLAY_HEIGHT) ASSERT("Y must be in range 0..DISPLAY_HEIGHT-1");

//     display_set_window(x, y, x, y);
//     display_color_fill(color, 1);
// }

static inline void display_init_gpio(void) {

    // Data / Command
    gpio_mode_setup(DISPLAY_DC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_DC_PIN);
    gpio_set_output_options(DISPLAY_DC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_DC_PIN);

    // Backlight
    gpio_mode_setup(DISPLAY_BL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_BL_PIN);
    gpio_set_output_options(DISPLAY_BL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_BL_PIN);

    // Reset
    gpio_set(DISPLAY_RST_PORT, DISPLAY_RST_PIN);
    gpio_mode_setup(DISPLAY_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_RST_PIN);
    gpio_set_output_options(DISPLAY_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_RST_PIN);

    // SS
    gpio_set(DISPLAY_SPI_PORT, DISPLAY_CS_PIN);
    gpio_mode_setup(DISPLAY_SPI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_CS_PIN);
    gpio_set_output_options(DISPLAY_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_CS_PIN);

    // SPI
    gpio_mode_setup(DISPLAY_SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, DISPLAY_SCK_PIN | DISPLAY_MOSI_PIN);
    gpio_set_af(DISPLAY_SPI_PORT, DISPLAY_SPI_GPIO_AF, DISPLAY_SCK_PIN | DISPLAY_MOSI_PIN);
    gpio_set_output_options(DISPLAY_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_SCK_PIN | DISPLAY_MOSI_PIN);
}

static inline void display_init_spi(void) {
    spi_init_master(DISPLAY_SPI, SPI_CR1_BAUDRATE_FPCLK_DIV_2, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);
}

static inline void display_init_dma(void) {
    dma_stream_reset(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_set_transfer_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    dma_set_peripheral_size(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_PSIZE_16BIT); 
    dma_set_memory_size(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_MSIZE_16BIT);
    dma_disable_peripheral_increment_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_enable_transfer_complete_interrupt(DISPLAY_DMA, DISPLAY_DMA_STREAM);    
    dma_enable_fifo_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_set_fifo_threshold(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxFCR_FTH_4_4_FULL);    
    dma_set_priority(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_PL_VERY_HIGH);

    // dma_set_peripheral_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, SPI_DR(DISPLAY_SPI));  
    dma_set_peripheral_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, 0x4000380C);  
    nvic_enable_irq(NVIC_DMA1_STREAM4_IRQ);
}

static inline void display_set_color_mode(uint8_t mode) {
    spi_set_dff_8bit(DISPLAY_SPI);
    _display_set_command();
    spi_enable(DISPLAY_SPI);
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_COLMOD);
    _display_wait_spi();
    gpio_set(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
    spi_write(DISPLAY_SPI, mode);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(10);
}

static inline void display_set_memory_mode(uint8_t mode) {    
    spi_set_dff_8bit(DISPLAY_SPI);
    _display_set_command();
    spi_enable(DISPLAY_SPI);
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_MADCTL);
    _display_wait_spi();
    gpio_set(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
    spi_write(DISPLAY_SPI, mode);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(10);
}

static inline void display_inverted(void) {
    _display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_INVON);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(10);
}

static inline void display_normal(void) {
    _display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_NORON);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(10);
}

static inline void display_on(void) {
    _display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_enable(DISPLAY_SPI);
    display_set_cs_low();
    spi_write(DISPLAY_SPI, ST7789_DISPON);
    spi_clean_disable(DISPLAY_SPI);
    _display_set_cs_high();
    vTaskDelay(10);
}

static inline void display_init(void) {
    display_init_spi();
    display_init_gpio();
    display_init_dma();

    _display_hard_reset();
    _display_soft_reset();
    _display_sleep_exit();

    nvic_enable_irq(NVIC_DMA1_STREAM4_IRQ);
    nvic_enable_irq(NVIC_SPI2_IRQ);

    display_set_color_mode(ST7789_16bit | ST7789_RGB_65k);
    display_inverted();
    display_normal();
    display_on();
    display_set_memory_mode(ST7789_MADCTL_RGB);
}


/////////////////////////////////////////////////////////////////////////////////////// ISR ///

#if DISPLAY_DMA == DMA1
#if DISPLAY_DMA_STREAM == DMA_STREAM0
void dma1_stream0_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM1
void dma1_stream1_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM2
void dma1_stream2_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM3
void dma1_stream3_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM4
void dma1_stream4_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM5
void dma1_stream5_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM6
void dma1_stream6_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM7
void dma1_stream7_isr(void) {
#endif
#elif DISPLAY_DMA == DMA2
#if DISPLAY_DMA_STREAM == DMA_STREAM0
void dma2_stream0_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM1
void dma2_stream1_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM2
void dma2_stream2_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM3
void dma2_stream3_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM4
void dma2_stream4_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM5
void dma2_stream5_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM6
void dma2_stream6_isr(void) {
#elif DISPLAY_DMA_STREAM == DMA_STREAM7
void dma2_stream7_isr(void) {
#endif
#endif
    if (dma_get_interrupt_flag(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_TCIF)) {
        dma_clear_interrupt_flags(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_TCIF);

        if (display_dma_pixels_to_transfer > 0) {
            if (display_dma_pixels_to_transfer > 0x0000FFFF) {
                dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, 0x0000FFFF);
                display_dma_pixels_to_transfer -= 0x0000FFFF;
            } else {
                dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, display_dma_pixels_to_transfer);
                display_dma_pixels_to_transfer = 0;
            }            
        } else {
            spi_disable_tx_dma(DISPLAY_SPI);
            dma_disable_stream(DISPLAY_DMA, DISPLAY_DMA_STREAM);
            dma_disable_transfer_complete_interrupt(DISPLAY_DMA, DISPLAY_DMA_STREAM);
            spi_clean_disable(DISPLAY_SPI);
            _display_set_cs_high();
            vTaskNotifyGiveFromISR(hDisplayTask, pdFALSE);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////// TASK ///

void _display_task(void *pvParameters) {
    (void) pvParameters;
    display_command_t command;

    display_set_backlight(100);
    display_init();
    
    for (;;) {
        if (xQueueReceive(hDisplayQueue, &command, portMAX_DELAY) == pdPASS) {
            switch (command.id) {
            case DISPLAY_COMMAND_FILL_SCREEN:
                _display_fill_screen_dma(
                    command.fill_screen.color
                );
                break;
            
            case DISPLAY_COMMAND_FILL_RECT:
                _display_color_fill_dma(
                    command.fill_rect.left,
                    command.fill_rect.right,
                    command.fill_rect.top,
                    command.fill_rect.bottom,
                    command.fill_rect.color
                );
                break;

            case DISPLAY_COMMAND_DRAW_RECT:
                _display_draw_rect(
                    command.draw_rect.left,
                    command.draw_rect.right,
                    command.draw_rect.top,
                    command.draw_rect.bottom,
                    command.draw_rect.color,
                    command.draw_rect.border_color
                );
                break;

            case DISPLAY_COMMAND_DRAW_TEXT:
                _display_draw_text(
                    command.draw_text.font,
                    command.draw_text.left,
                    command.draw_text.top,
                    command.draw_text.fore_color,
                    command.draw_text.back_color,
                    command.draw_text.text,
                    wcsnlen(command.draw_text.text, sizeof(command.draw_text.text))
                );
                break;

            default:
                break;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////// SETUP ///

void display_setup() {
    hDisplayQueue = xQueueCreate(16, sizeof(display_command_t));
    if (hDisplayQueue == NULL) {
        ASSERT("Display command queue creation failed.");
    }

    if (xTaskCreate(_display_task, "Display", 256, NULL, 1, &hDisplayTask) != pdPASS) {
        ASSERT("Display task creation failed.");
    }
}

/////////////////////////////////////////////////////////////////////////////////////// API ///

void display_fill_screen(uint16_t color) {
    display_command_t command;
    command.id                        = DISPLAY_COMMAND_FILL_SCREEN;
    command.fill_screen.color         = color;
    xQueueSend(hDisplayQueue, &command, portMAX_DELAY);
}

void display_fill_rect(uint16_t color, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom) {
    display_command_t command;
    command.id                        = DISPLAY_COMMAND_FILL_RECT;
    command.fill_rect.color           = color;
    command.fill_rect.left            = left;
    command.fill_rect.right           = right;
    command.fill_rect.top             = top;
    command.fill_rect.bottom          = bottom;
    xQueueSend(hDisplayQueue, &command, portMAX_DELAY);
}

void display_draw_rect(uint16_t color, uint16_t border_color, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom) {
    display_command_t command;
    command.id                        = DISPLAY_COMMAND_DRAW_RECT;
    command.draw_rect.color           = color;
    command.draw_rect.border_color    = border_color;
    command.draw_rect.left            = left + 1;
    command.draw_rect.right           = right - 1;
    command.draw_rect.top             = top - 1;
    command.draw_rect.bottom          = bottom + 1;
    xQueueSend(hDisplayQueue, &command, portMAX_DELAY);
}

void display_draw_text(const font_t *font, uint16_t color, uint16_t back_color, uint16_t x, uint16_t y, wchar_t *text) {
    size_t length = wcslen(text);
    display_command_t command;
    command.id                        = DISPLAY_COMMAND_DRAW_TEXT;
    command.draw_text.font            = font;
    command.draw_text.left            = x;
    command.draw_text.top             = y;
    command.draw_text.fore_color      = color;
    command.draw_text.back_color      = back_color;

    for (;;) {
        size_t len = min(length, sizeof(command.draw_text.text) / sizeof(command.draw_text.text[0]));
        memcpy(command.draw_text.text, text, len * sizeof(command.draw_text.text[0]));
        if (len < sizeof(command.draw_text.text)) command.draw_text.text[len] = 0;
        xQueueSend(hDisplayQueue, &command, portMAX_DELAY);
        length -= len;
        if (length <= 0) break;
        text += len;
        command.draw_text.left += len * font->width;
    }
}

/////////////////////////////////////////////////////////////////////////////////////// END ///
