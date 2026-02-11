#include "LcdDriver.h"

uint8_t slice_num;

struct screen_attr_struct {
    uint16_t width;
    uint16_t height;
    uint8_t scan_dir;
} screen_attr;

uint16_t *frame_buffer = NULL;
uint16_t image_size = 0;

// Internal function prototypes
static void SetGpioMode(uint8_t pin, uint8_t mode);
static void LcdSendCommand(uint8_t reg);
static void LcdSendDataByte(uint8_t data);
static void LcdSendDataWord(uint16_t data);
static void LcdSetWindows(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);


uint8_t LcdModuleInit() {
#ifndef ARDUINO
    stdio_init_all();
#endif

    // SPI Config: ST7789 controller over SPI1.
    spi_init(SPI_PORT, 10000 * 1000);
    gpio_set_function(LCD_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);
    
    // GPIO Config for panel control lines.
    SetGpioMode(LCD_RST_PIN, GPIO_OUT);
    SetGpioMode(LCD_DC_PIN, GPIO_OUT);
    // Repeated by upstream sample code; left as-is for compatibility.
    SetGpioMode(LCD_CS_PIN, GPIO_OUT);
    SetGpioMode(LCD_BL_PIN, GPIO_OUT);

    SetGpioMode(LCD_CS_PIN, GPIO_OUT);
    SetGpioMode(LCD_BL_PIN, GPIO_OUT);

    SetGpioMode(LCD_KEY_A, GPIO_IN);
    SetGpioMode(LCD_KEY_B, GPIO_IN);
    SetGpioMode(LCD_KEY_X, GPIO_IN);
    SetGpioMode(LCD_KEY_Y, GPIO_IN);
    SetGpioMode(LCD_KEY_UP, GPIO_IN);
    SetGpioMode(LCD_KEY_DOWN, GPIO_IN);
    SetGpioMode(LCD_KEY_LEFT, GPIO_IN);
    SetGpioMode(LCD_KEY_RIGHT, GPIO_IN);
    SetGpioMode(LCD_KEY_CTRL, GPIO_IN);

    // Idle pin levels before init transaction stream.
    gpio_put(LCD_CS_PIN, 1);
    gpio_put(LCD_DC_PIN, 0);
    gpio_put(LCD_BL_PIN, 1);
    
    
    // PWM Config: backlight duty cycle is expressed in 0..100 "percent-like"
    // units by LcdBacklightPercent().
    gpio_set_function(LCD_BL_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(LCD_BL_PIN);
    pwm_set_wrap(slice_num, 100);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 1);
    pwm_set_clkdiv(slice_num,50);
    pwm_set_enabled(slice_num, true);
    
    return 0;
}


uint16_t *LcdDisplayInit(uint8_t scan_dir) {
    // Hardware reset sequence recommended by panel vendor.
    gpio_put(LCD_RST_PIN, 1);
    sleep_ms(100);
    gpio_put(LCD_RST_PIN, 0);
    sleep_ms(100);
    gpio_put(LCD_RST_PIN, 1);
    sleep_ms(100);

    // Cache orientation and logical size for drawing helpers.
    screen_attr.scan_dir = scan_dir;
    screen_attr.width = SCREEN_WIDTH_HEIGHT;
    screen_attr.height = SCREEN_WIDTH_HEIGHT;
    uint8_t memory_access_reg;

    if (frame_buffer != NULL) {
        free(frame_buffer);
        frame_buffer = NULL;
    }
    image_size = screen_attr.width * screen_attr.height;
    // One RGB565 word per pixel.
    frame_buffer = (uint16_t *)malloc(image_size * 2);

    // MADCTL bits differ by scan direction.
    if(scan_dir == SCAN_DIR_HORIZONTAL) {
        memory_access_reg = 0X70;
    } else {
        memory_access_reg = 0X00;
    }

    // Controller init command sequence (ST7789 register programming).
    // These magic values come from Waveshare's known-good defaults.
    LcdSendCommand(0x36); //MX, MY, RGB mode
    LcdSendDataByte(memory_access_reg);	//0x08 set RGB

    LcdSendCommand(0x3A);
    LcdSendDataByte(0x05);

    LcdSendCommand(0xB2);
    LcdSendDataByte(0x0C);
    LcdSendDataByte(0x0C);
    LcdSendDataByte(0x00);
    LcdSendDataByte(0x33);
    LcdSendDataByte(0x33);

    LcdSendCommand(0xB7);  //Gate Control
    LcdSendDataByte(0x35);

    LcdSendCommand(0xBB);  //VCOM Setting
    LcdSendDataByte(0x19);
    LcdSendCommand(0xC0); //LCM Control     
    LcdSendDataByte(0x2C);

    LcdSendCommand(0xC2);  //VDV and VRH Command Enable
    LcdSendDataByte(0x01);
    LcdSendCommand(0xC3);  //VRH Set
    LcdSendDataByte(0x12);
    LcdSendCommand(0xC4);  //VDV Set
    LcdSendDataByte(0x20);
    LcdSendCommand(0xC6);  //Frame Rate Control in Normal Mode
    LcdSendDataByte(0x0F);

    LcdSendCommand(0xD0);  // Power Control 1
    LcdSendDataByte(0xA4);
    LcdSendDataByte(0xA1);

    LcdSendCommand(0xE0);  //Positive Voltage Gamma Control
    LcdSendDataByte(0xD0);
    LcdSendDataByte(0x04);
    LcdSendDataByte(0x0D);
    LcdSendDataByte(0x11);
    LcdSendDataByte(0x13);
    LcdSendDataByte(0x2B);
    LcdSendDataByte(0x3F);
    LcdSendDataByte(0x54);
    LcdSendDataByte(0x4C);
    LcdSendDataByte(0x18);
    LcdSendDataByte(0x0D);
    LcdSendDataByte(0x0B);
    LcdSendDataByte(0x1F);
    LcdSendDataByte(0x23);

    LcdSendCommand(0xE1);  //Negative Voltage Gamma Control
    LcdSendDataByte(0xD0);
    LcdSendDataByte(0x04);
    LcdSendDataByte(0x0C);
    LcdSendDataByte(0x11);
    LcdSendDataByte(0x13);
    LcdSendDataByte(0x2C);
    LcdSendDataByte(0x3F);
    LcdSendDataByte(0x44);
    LcdSendDataByte(0x51);
    LcdSendDataByte(0x2F);
    LcdSendDataByte(0x1F);
    LcdSendDataByte(0x1F);
    LcdSendDataByte(0x20);
    LcdSendDataByte(0x23);

    LcdSendCommand(0x21);  //Display Inversion On

    LcdSendCommand(0x11);  //Sleep Out

    LcdSendCommand(0x29);  //Display On

    return frame_buffer;
}


void LcdBacklightPercent(uint8_t percent) {
    if(percent < 0 || percent > 100){
        printf("LcdBacklightPercent Error \r\n");
    } else {
        pwm_set_chan_level(slice_num, PWM_CHAN_B, percent);
    }
}


void LcdClearScreen(uint16_t color) {
    uint16_t i;
    
    // Framebuffer is stored in byte-swapped RGB565 for direct SPI transfer.
    color = ((color<<8) & 0xff00) | (color>>8);

    for (i = 0; i < image_size; i++) {
        frame_buffer[i] = color;
    }

    LcdWriteToScreen();
}

void LcdWriteToScreen() {
    // Full-frame flush. Higher-level UI code batches updates before calling.
    LcdSetWindows(0, 0, screen_attr.width, screen_attr.height);
    gpio_put(LCD_DC_PIN, 1);
    gpio_put(LCD_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, (uint8_t *) frame_buffer, image_size * 2); 
    gpio_put(LCD_CS_PIN, 1);
}


uint8_t LcdGetKey(uint8_t key_pin) {
    return gpio_get(key_pin);
}


// Internal functions


static void LcdSendCommand(uint8_t reg) {
    gpio_put(LCD_DC_PIN, 0);
    gpio_put(LCD_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, &reg, 1);
    gpio_put(LCD_CS_PIN, 1);
}

static void SetGpioMode(uint8_t pin, uint8_t mode) {
    gpio_init(pin);
    gpio_set_dir(pin, mode);
    if (mode == GPIO_IN) {
        gpio_pull_up(pin); // Need to pull up
    }
}


static void LcdSendDataByte(uint8_t data) {
    gpio_put(LCD_DC_PIN, 1);
    gpio_put(LCD_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, &data, 1);
    gpio_put(LCD_CS_PIN, 1);
}


static void LcdSendDataWord(uint16_t data) {
    const uint8_t high_byte = (data >> 8) & 0xFF;
    const uint8_t low_byte = data & 0xFF;

    gpio_put(LCD_DC_PIN, 1);
    gpio_put(LCD_CS_PIN, 0);
    spi_write_blocking(SPI_PORT, &high_byte, 1);
    spi_write_blocking(SPI_PORT, &low_byte, 1);
    gpio_put(LCD_CS_PIN, 1);
}


static void LcdSetWindows(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    // Configure ST7789 drawing window, inclusive end coordinates.
    // Data writes after 0x2C will auto-increment within this rectangle.
    //set the X coordinates
    LcdSendCommand(0x2A);
    LcdSendDataByte(0x00);
    LcdSendDataByte(x_start);
	LcdSendDataByte(0x00);
    LcdSendDataByte(x_end-1);

    //set the Y coordinates
    LcdSendCommand(0x2B);
    LcdSendDataByte(0x00);
	LcdSendDataByte(y_start);
	LcdSendDataByte(0x00);
    LcdSendDataByte(y_end-1);

    LcdSendCommand(0X2C);
}
