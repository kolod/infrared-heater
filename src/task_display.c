#include "task_display.h"

TaskHandle_t hDisplayTask;
uint16_t display_dma_buffer[DISPLAY_GLIPH_WIDTH * DISPLAY_GLIPH_HEIGHT];

static inline void display_set_backlight(uint32_t value) {
    if (value) {
        gpio_set(DISPLAY_BL_PORT, DISPLAY_BL_PIN);
    } else {
        gpio_clear(DISPLAY_BL_PORT, DISPLAY_BL_PIN);
    }
}

static inline void display_set_command(void) {
    gpio_clear(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
}

static inline void display_set_data(void) {
    gpio_set(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
}

static inline void display_hard_reset(void) {
    gpio_clear(DISPLAY_RST_PORT, DISPLAY_RST_PIN);
    vTaskDelay(10);
    gpio_set(DISPLAY_RST_PORT, DISPLAY_RST_PIN);
    vTaskDelay(160);
}

static inline void display_write_command_8bit(const uint8_t command) {
    display_set_command();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_xfer(DISPLAY_SPI, command);
}

static inline void display_write_data_8bit(const uint8_t data) {
    display_set_data();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_xfer(DISPLAY_SPI, data);
}

static inline void display_write_data_16bit(const uint16_t data) {
    display_set_data();
    spi_set_dff_16bit(DISPLAY_SPI);
    spi_xfer(DISPLAY_SPI, data);
}

static inline uint8_t display_read_data_8bit(void) {
    display_set_data();
    spi_set_dff_8bit(DISPLAY_SPI);
    spi_set_bidirectional_mode(DISPLAY_SPI);
    spi_read(DISPLAY_SPI);
}

static inline void display_soft_reset(void) {
    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_SWRESET);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(130);
}

static inline void display_sleep_exit(void) {
    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_SLPOUT);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(500);
}

static inline void display_sleep_enter(void) {
    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_SLPIN);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(500);
}

static inline void display_write_data_dma(const uint8_t* data, size_t length) {
    (void) data;
    (void) length;
}

void display_set_window(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom) {
    left   += DISPLAY_OFFSET_X;
    right  += DISPLAY_OFFSET_X;
    top    += DISPLAY_OFFSET_Y;
    bottom += DISPLAY_OFFSET_Y;

    display_write_command_8bit(ST7789_CASET);
    display_write_data_16bit(left);
    display_write_data_16bit(right);

    display_write_command_8bit(ST7789_RASET);
    display_write_data_16bit(top);
    display_write_data_16bit(bottom);

    display_write_command_8bit(ST7789_RAMWR);
}

void display_color_fill(uint16_t color, size_t length) {
    gpio_set(DISPLAY_DC_PORT, DISPLAY_DC_PIN);
    spi_set_dff_16bit(DISPLAY_SPI);
    for (size_t i = length; i; i--)
        spi_xfer(DISPLAY_SPI, color);
}

void display_color_fill_dma(uint16_t color, size_t length) {
    dma_stream_reset(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_set_priority(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_PL_HIGH);
    dma_set_memory_size(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_MSIZE_16BIT);
    dma_set_peripheral_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, SPI_DR(DISPLAY_SPI));
    dma_set_peripheral_size(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_PSIZE_16BIT);
    dma_disable_memory_increment_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_set_transfer_mode(DMA2, DMA_STREAM3, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

    display_dma_buffer[0] = color;
    dma_set_memory_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, (uint32_t) display_dma_buffer);
    dma_set_number_of_data(DISPLAY_DMA, DISPLAY_DMA_STREAM, length);
    dma_enable_stream(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    spi_enable_tx_dma(DISPLAY_SPI);

    // Nonblocking wait for dma transfer complete
    xTaskNotifyWait(0, 0xFFFFFFFF, NULL, portMAX_DELAY);
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {    
    if (x >= DISPLAY_WIDTH)  ASSERT("X must be in range 0..DISPLAY_WIDTH-1");    
    if (y >= DISPLAY_HEIGHT) ASSERT("Y must be in range 0..DISPLAY_HEIGHT-1");

    display_set_window(x, y, x, y);
    display_color_fill(color, 1);
}

void display_fill_screen(uint16_t color) {
    spi_enable(DISPLAY_SPI);
    display_set_window(0, DISPLAY_WIDTH-1, 0, DISPLAY_HEIGHT-1);
    display_color_fill(color, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    spi_disable(DISPLAY_SPI);
}

void display_draw_filed_rect(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom, uint16_t color) {
    display_set_window(left, right, top, bottom);
    display_color_fill(color, (right-left) * (top-bottom));
}

void display_draw_rect(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom, uint16_t border_color, uint16_t fore_color) {
    display_draw_filed_rect(left, right, top, top-1, border_color);
    display_draw_filed_rect(left, left+1, top-1, bottom+1, border_color);
    display_draw_filed_rect(right-1, right, top-1, bottom+1, border_color);
    display_draw_filed_rect(left, right, bottom, bottom+1, border_color);
    display_draw_filed_rect(left+1, right-1, top-1, bottom+1, fore_color);
}

static inline void display_init_spi(void) {

    // Data / Command
    gpio_mode_setup(DISPLAY_DC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_DC_PIN);
    gpio_set_output_options(DISPLAY_DC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_DC_PIN);

    // Backlight
    gpio_mode_setup(DISPLAY_BL_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_BL_PIN);
    gpio_set_output_options(DISPLAY_BL_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_BL_PIN);

    // Backlight
    gpio_mode_setup(DISPLAY_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DISPLAY_RST_PIN);
    gpio_set_output_options(DISPLAY_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_RST_PIN);

    // SPI
    gpio_mode_setup(DISPLAY_SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, DISPLAY_CS_PIN | DISPLAY_SCK_PIN | DISPLAY_MOSI_PIN | DISPLAY_MISO_PIN);
    gpio_set_af(DISPLAY_SPI_PORT, DISPLAY_SPI_GPIO_AF, DISPLAY_CS_PIN | DISPLAY_SCK_PIN | DISPLAY_MOSI_PIN | DISPLAY_MISO_PIN);
    gpio_set_output_options(DISPLAY_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, DISPLAY_CS_PIN | DISPLAY_SCK_PIN | DISPLAY_MOSI_PIN | DISPLAY_MISO_PIN);

    //spi_clean_disable(DISPLAY_SPI);
    spi_disable(DISPLAY_SPI);
    spi_init_master(DISPLAY_SPI, SPI_CR1_BAUDRATE_FPCLK_DIV_2, SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);
    spi_disable_software_slave_management(DISPLAY_SPI);
    spi_enable_ss_output(DISPLAY_SPI);

    spi_enable(DISPLAY_SPI);
}

static inline void display_init_dma(void) {
    dma_stream_reset(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_set_priority(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_PL_HIGH);
    dma_set_memory_size(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_MSIZE_16BIT);
    dma_set_peripheral_address(DISPLAY_DMA, DISPLAY_DMA_STREAM, SPI_DR(DISPLAY_SPI));
    dma_set_peripheral_size(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_PSIZE_16BIT);
    dma_disable_memory_increment_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM);
    dma_set_transfer_mode(DISPLAY_DMA, DISPLAY_DMA_STREAM, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    
    nvic_enable_irq(NVIC_DMA2_STREAM4_IRQ);
}

static inline void display_init(void) {
    display_init_spi();
    display_init_dma();

    display_hard_reset();
    display_soft_reset();
    display_sleep_exit();
    
    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_COLMOD);
    display_write_data_8bit(ST7789_16bit | ST7789_RGB_65k);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(10);

    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_CASET);
    display_write_data_16bit(0);
    display_write_data_16bit(DISPLAY_WIDTH);
    display_write_command_8bit(ST7789_RASET);
    display_write_data_16bit(0);
    display_write_data_16bit(DISPLAY_HEIGHT);
    spi_disable(DISPLAY_SPI);

    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_INVON);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(10);

    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_NORON);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(10);

    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_DISPON);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(10);

    spi_enable(DISPLAY_SPI);
    display_write_command_8bit(ST7789_MADCTL);
    display_write_data_8bit(ST7789_MADCTL_MX | ST7789_MADCTL_MY);
    spi_disable(DISPLAY_SPI);
    vTaskDelay(10);
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
        dma_disable_stream(DISPLAY_DMA, DISPLAY_DMA_STREAM);
        spi_disable_tx_dma(DISPLAY_SPI);
        vTaskGenericNotifyGiveFromISR(hDisplayTask, 0, NULL);
    }
}

////////////////////////////////////////////////////////////////////////////////////// TASK ///

void display_task(void *pvParameters) {
    (void) pvParameters;

    display_set_backlight(100);
    display_init();
    
    for (;;) {
        display_fill_screen(0xFFFF);
        vTaskDelay(2000);
        display_fill_screen(0x0000);
        vTaskDelay(2000);
    }
}
