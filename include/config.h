#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/scb.h>

#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/stm32/f4/usart.h>
#include <libopencm3/stm32/f4/nvic.h>
#include <libopencm3/stm32/f4/spi.h>
#include <libopencm3/stm32/f4/dma.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>

#include "assert.h"

#define PRIORITY_GROUP       3

// LED
#define LED_PORT             GPIOC
#define LED_PIN              GPIO13

// DEBUG
#define DEBUG_PORT           GPIOC
#define DEBUG_PIN            GPIO14

// Display
#define DISPLAY_SPI          SPI2

#define DISPLAY_DC_PORT      GPIOA
#define DISPLAY_DC_PIN       GPIO8

#define DISPLAY_RST_PORT     GPIOA
#define DISPLAY_RST_PIN      GPIO9

#define DISPLAY_BL_PORT      GPIOA
#define DISPLAY_BL_PIN       GPIO10
  
#define DISPLAY_SPI_PORT     GPIOB
#define DISPLAY_CS_PIN       GPIO12
#define DISPLAY_SCK_PIN      GPIO13
#define DISPLAY_MOSI_PIN     GPIO15
#define DISPLAY_MISO_PIN     GPIO14
#define DISPLAY_SPI_GPIO_AF  GPIO_AF5

#define DISPLAY_DMA          DMA1
#define DISPLAY_DMA_STREAM   DMA_STREAM4
#define DISPLAY_DMA_CHANNEL  DMA_SxCR_CHSEL_0
#define DISPLAY_SPI          SPI2

#define DISPLAY_WIDTH        240
#define DISPLAY_HEIGHT       320

#define DISPLAY_OFFSET_X     0
#define DISPLAY_OFFSET_Y     0

#define DISPLAY_GLIPH_WIDTH  32
#define DISPLAY_GLIPH_HEIGHT 48



// USB
#define USB_DP               PA13
#define USB_DN               PA11

// SPI FLASH
#define SPI_CS               PA4
#define SPI_SCK              PA5
#define SPI_MOSI             PA7
#define SPI_MISO             PA6

// SWD
#define SWDIO                PA13
#define SWCLK                PA14
#define SWO                  PB3

// Display

// Keyboard
#define KEYBOARD_RESET       PA2
#define KEYBOARD_SET         PA3
#define KEYBOARD_CENTER      PB0
#define KEYBOARD_RIGHT       PB1
#define KEYBOARD_LEFT        PB2
#define KEYBOARD_DOWN        PB10
#define KEYBOARD_UP          PB12

#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b)) 
