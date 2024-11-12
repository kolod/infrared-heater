
#include "config.h"

#include "task_controller.h"
#include "task_display.h"

/* Set STM32 to 84 MHz. */
static inline void clock_setup(void) {
	rcc_clock_setup_pll(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_84MHZ]);
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_SPI2);
    rcc_periph_clock_enable(RCC_DMA1);
}

static inline void gpio_setup(void) {
	// LED
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_set_output_options(LED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, LED_PIN);
    
    // Display
}

static inline void setup_spi(void) {
    
}

int main(void) {
    clock_setup();
    gpio_setup();
    setup_spi();

    // Initialize interrupt priority grouping
    scb_set_priority_grouping(SCB_AIRCR_PRIGROUP_GROUP16_NOSUB);
    nvic_set_priority(NVIC_PENDSV_IRQ, 15<<4);
    nvic_set_priority(NVIC_SYSTICK_IRQ, 15<<4);

    // System
    //nvic_set_priority(NVIC_MEM_MANAGE_IRQ, 0);
    //nvic_set_priority(NVIC_BUS_FAULT_IRQ, 0);
    //nvic_set_priority(NVIC_USAGE_FAULT_IRQ, 0);
    //nvic_set_priority(NVIC_SV_CALL_IRQ, 0);
    //nvic_set_priority(NVIC_PENDSV_IRQ, 0xE0);
    //nvic_set_priority(NVIC_SYSTICK_IRQ, 0xE0);

    cm_enable_interrupts();

    //if (xTaskCreate(controller_task, "Controller", 512*4, NULL, 4, &hControllerTask) != pdPASS)
	//	__assert(__FILE__, __LINE__, "Controller task creation failed");

    if (xTaskCreate(display_task, "Display", 512, NULL, 1, &hDisplayTask) != pdPASS) 
        __assert(__FILE__, __LINE__, "Display task creation failed.");

    /* Infinite loop */
	vTaskStartScheduler();
    __assert(__FILE__, __LINE__, "Scheduler starting failed");
}
