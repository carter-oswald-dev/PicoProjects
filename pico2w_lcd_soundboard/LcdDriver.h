#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#define SPI_PORT        spi1

#define LCD_DC_PIN      8
#define LCD_CS_PIN      9
#define LCD_CLK_PIN     10
#define LCD_MOSI_PIN    11
#define LCD_RST_PIN     12
#define LCD_BL_PIN      13

#define LCD_KEY_A     15
#define LCD_KEY_B     17
#define LCD_KEY_X     19
#define LCD_KEY_Y     21
#define LCD_KEY_UP     2
#define LCD_KEY_DOWN  18
#define LCD_KEY_LEFT  16
#define LCD_KEY_RIGHT 20
#define LCD_KEY_CTRL   3


#define SCAN_DIR_HORIZONTAL 0
#define SCAN_DIR_VERTICAL   1
#define SCREEN_WIDTH_HEIGHT 240

#ifdef __cplusplus
extern "C" {
#endif

uint8_t LcdModuleInit();
uint16_t *LcdDisplayInit(uint8_t scan_dir);

void LcdBacklightPercent(uint8_t percent);

void LcdClearScreen(uint16_t color);

void LcdWriteToScreen();

uint8_t LcdGetKey(uint8_t key_pin);

#ifdef __cplusplus
}
#endif

#endif // LCD_DRIVER_H
