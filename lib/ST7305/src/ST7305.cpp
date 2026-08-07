#include "ST7305.h"

ST7305::ST7305(int16_t w, int16_t h, SPIClass *spi, int8_t cs_pin, int8_t dc_pin,
               int8_t rst_pin, int8_t te_pin) : Adafruit_GFX(w, h),
                                                _spi(spi),
                                                _cs_pin(cs_pin),
                                                _dc_pin(dc_pin),
                                                _rst_pin(rst_pin),
                                                _te_pin(te_pin), rotation(0)
{ // Initialize rotation to
    // temp_buffer = (uint8_t *)malloc(192 * 14 * 3);
    // 像素数据结构为：
    // P1 P3 P5 P7
    // P2 P5 P6 P8

    // P0 P2 P4 P6
    // P1 P3 P5 P7

    // 对应一个byte数据的：
    // BIT7 BIT5 BIT3 BIT1
    // BIT6 BIT4 BIT2 BIT0
}

typedef union {
    uint32_t value;
    struct {
            uint32_t clkcnt_l:       6;                     /*it must be equal to spi_clkcnt_N.*/
            uint32_t clkcnt_h:       6;                     /*it must be floor((spi_clkcnt_N+1)/2-1).*/
            uint32_t clkcnt_n:       6;                     /*it is the divider of spi_clk. So spi_clk frequency is system/(spi_clkdiv_pre+1)/(spi_clkcnt_N+1)*/
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
            uint32_t clkdiv_pre:     4;                     /*it is pre-divider of spi_clk.*/
            uint32_t reserved:       9;  					/*reserved*/
#else
            uint32_t clkdiv_pre:    13;                     /*it is pre-divider of spi_clk.*/
#endif
            uint32_t clk_equ_sysclk: 1;                     /*1: spi_clk is eqaul to system 0: spi_clk is divided from system clock.*/
    };
} spiClk_t;

bool ST7305::begin(bool reset)
{
    log_i("缓冲区初始化...");
    _buffers[0] = (uint8_t*) ps_malloc(BYTES_PER_BUFFER);
    _buffers[1] = (uint8_t*) ps_malloc(BYTES_PER_BUFFER);
    _buffers[2] = (uint8_t*) ps_malloc(BYTES_PER_BUFFER);
    _buffers[3] = (uint8_t*) ps_malloc(BYTES_PER_BUFFER);
    buffer = _buffers[0];
    memset(_buffers[0], 0xFF, sizeof(BYTES_PER_BUFFER));
    memset(_buffers[1], 0xFF, sizeof(BYTES_PER_BUFFER));
    memset(_buffers[2], 0xFF, sizeof(BYTES_PER_BUFFER));
    memset(_buffers[3], 0xFF, sizeof(BYTES_PER_BUFFER));
    log_i("GPIO初始化...");
    // Initialize pins
    pinMode(_cs_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_rst_pin, OUTPUT_OPEN_DRAIN | PULLUP);
    if (_te_pin >= 0)
    {
        pinMode(_te_pin, INPUT);
    }

    digitalWrite(_cs_pin, HIGH);

    log_i("屏幕复位...");
    if (reset)
    {
        // Hardware reset
        gpio_hold_dis((gpio_num_t)_rst_pin);
        digitalWrite(_rst_pin, HIGH);
        delay(50);
        digitalWrite(_rst_pin, LOW);
        delay(5);
        digitalWrite(_rst_pin, HIGH);
        gpio_hold_en((gpio_num_t)_rst_pin);
        delay(120);
    }
    
    log_i("初始化SPI总线...");
    _spi->begin(CONFIG_SPI_SCK, -1, CONFIG_SPI_MOSI, -1);
    _spi->setFrequency(33333333);
    #define ClkRegToFreq(reg) (apb_freq / (((reg)->clkdiv_pre + 1) * ((reg)->clkcnt_n + 1)))
    uint32_t apb_freq = getApbFrequency();
    spiClk_t reg = { _spi->getClockDivider() };
    uint32_t freq = ClkRegToFreq(&reg);
    log_i("目标时钟频率33.3M,实际时钟频率: %lu", freq);
    
    log_i("初始化屏幕...");
    initDisplay();

    clearDisplay();
    return true;
}

void ST7305::sendCommand(uint8_t command)
{
    digitalWrite(_dc_pin, LOW); // Command mode
    digitalWrite(_cs_pin, LOW);
    _spi->transfer(command);
    digitalWrite(_cs_pin, HIGH);
}

void ST7305::sendData(uint8_t data)
{
    digitalWrite(_dc_pin, HIGH); // Data mode
    digitalWrite(_cs_pin, LOW);
    _spi->transfer(data);
    digitalWrite(_cs_pin, HIGH);
}

void ST7305::sendData(uint8_t *data, size_t len)
{
    digitalWrite(_dc_pin, HIGH); // Data mode
    digitalWrite(_cs_pin, LOW);
    // _spi->transfer(data, len);
    _spi->writeBytes(data, len);
    digitalWrite(_cs_pin, HIGH);
}

void ST7305::set(uint8_t cmd, uint8_t data)
{
    sendCommand(cmd);
    sendData(data);
}

void ST7305::set(uint8_t cmd, uint8_t *data, size_t len)
{
    sendCommand(cmd);
    sendData(data, len);
}

void ST7305::initDisplay()
{
    sendCommand(0xD6); // NVM Load Control
    sendData(0x17);
    sendData(0x02);

    sendCommand(0xD1); // Booster Enable
    sendData(0x01);

    sendCommand(0xC0); // Gate Voltage Setting
    sendData(voltageSet[0][0]);
    sendData(voltageSet[0][1]);

    sendCommand(0xC1); // VSHP Setting (4.8V)
    sendData(voltageSet[0][2]);
    sendData(voltageSet[1][2]);
    sendData(voltageSet[2][2]);
    sendData(voltageSet[3][2]);

    sendCommand(0xC2); // VSLP Setting (0.98V)
    sendData(voltageSet[0][3]);
    sendData(voltageSet[1][3]);
    sendData(voltageSet[2][3]);
    sendData(voltageSet[3][3]);

    sendCommand(0xC4); // VSHN Setting (-3.6V)
    sendData(voltageSet[0][4]);
    sendData(voltageSet[1][4]);
    sendData(voltageSet[2][4]);
    sendData(voltageSet[3][4]);

    sendCommand(0xC5); // VSLN Setting (0.22V)
    sendData(voltageSet[0][5]);
    sendData(voltageSet[1][5]);
    sendData(voltageSet[2][5]);
    sendData(voltageSet[3][5]);

    // 配合下面Frame Rate Control设置HPM刷新率{0xA6 0xE9  16/32Hz} {0x80 0xE9  25.5/51Hz}
    sendCommand(0xD8); // OSC Setting
    sendData(0x80);
    sendData(0xE9);

    // 0X00 HPM=16Hz LPM=0.25HzHz；0X10 HPM=32Hz  LPM=0.25Hz
    // 0X01 HPM=16Hz LPM=0.5HzHz； 0X11 HPM=32Hz  LPM=0.5Hz
    // 0X02 HPM=16Hz LPM=1Hz；0X12 HPM=32Hz  LPM=1Hz
    // 0X03 HPM=16Hz LPM=2Hz；0X13 HPM=32Hz  LPM=2Hz
    // 0X04 HPM=16Hz LPM=4Hz；0X12 HPM=32Hz  LPM=4Hz
    // 0X05 HPM=16Hz LPM=8Hz；0X15 HPM=32Hz  LPM=8Hz
    sendCommand(0xB2); // Frame Rate Control
    sendData(0x12);    //

    // Update Period Gate EQ Control in HPM
    sendCommand(0xB3);
    uint8_t b3_data[] = {0xE5, 0xF6, 0x17, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x71};
    sendData(b3_data, sizeof(b3_data));

    // Update Period Gate EQ Control in LPM
    sendCommand(0xB4);
    uint8_t b4_data[] = {0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
    sendData(b4_data, sizeof(b4_data));

    sendCommand(0x62); // Gate Timing Control
    uint8_t g62_data[] = {0x32, 0x03, 0x1F};
    sendData(g62_data, sizeof(g62_data));

    sendCommand(0xB7);
    sendData(0x13); // Source EQ Enable
    sendCommand(0xB0);
    sendData(0x60); // Gate Line Setting: 384 line

    sendCommand(0x11); // Sleep out
    delay(120);

    sendCommand(0xC9);
    sendData(0x00); // Source Voltage Select
    sendCommand(0x36);
    sendData(0x00); // Memory Data Access Control
    sendCommand(0x3A);
    sendData(0x11); // Data Format Select
    sendCommand(0xB9);
    sendData(0x20); // Gamma Mode Setting
    sendCommand(0xB8);
    sendData(0x29); // Panel Setting

    sendCommand(0x2A);
    sendData(0x17);
    sendData(0x24); // Column Address Setting
    sendCommand(0x2B);
    sendData(0x00);
    sendData(0xBF); // Row Address Setting

    if (_te_pin >= 0)
    {
        sendCommand(0x35);
        sendData(0x00); // TE
    }

    sendCommand(0xD0); // Auto power down
    sendData(0xFF);
    sendCommand(0x39); // 低功耗模式
    sendCommand(0x29); // Display on
    // setvoltage(fps_5100);
    delay(100);
}

// void ST7305::convertBuffer() {
//     uint16_t k = 0;
//     for (uint16_t i = 0; i < 384; i += 2) {
//         // Convert 2 columns
//         for (uint16_t j = 0; j < 21; j += 3) {
//             for (uint8_t y = 0; y < 3; y++) {
//                 uint8_t b1 = buffer[(j + y) * 384 + i];
//                 uint8_t b2 = buffer[(j + y) * 384 + i + 1];

//                 // First 4 bits
//                 uint8_t mix = 0;
//                 mix |= ((b1 & 0x01) << 7);
//                 mix |= ((b2 & 0x01) << 6);
//                 mix |= ((b1 & 0x02) << 4);
//                 mix |= ((b2 & 0x02) << 3);
//                 mix |= ((b1 & 0x04) << 1);
//                 mix |= ((b2 & 0x04) << 0);
//                 mix |= ((b1 & 0x08) >> 2);
//                 mix |= ((b2 & 0x08) >> 3);
//                 temp_buffer[k++] = mix;

//                 // Second 4 bits
//                 b1 >>= 4;
//                 b2 >>= 4;
//                 mix = 0;
//                 mix |= ((b1 & 0x01) << 7);
//                 mix |= ((b2 & 0x01) << 6);
//                 mix |= ((b1 & 0x02) << 4);
//                 mix |= ((b2 & 0x02) << 3);
//                 mix |= ((b1 & 0x04) << 1);
//                 mix |= ((b2 & 0x04) << 0);
//                 mix |= ((b1 & 0x08) >> 2);
//                 mix |= ((b2 & 0x08) >> 3);
//                 temp_buffer[k++] = mix;
//             }
//         }
//     }
// }

void ST7305::display()
{
    // convertBuffer();

    // Set display window
    uint8_t caset[] = {0x17, 0x17 + 14 - 1};
    uint8_t raset[] = {0x00, 0x00 + 192 - 1};

    sendCommand(0x2A);
    sendData(caset, sizeof(caset));

    sendCommand(0x2B);
    sendData(raset, sizeof(raset));

    // long begin = millis();

    // if (_te_pin >= 0)
    //     while (digitalRead(_te_pin) == HIGH)
    //         delay(1);

    sendCommand(0x2C);
    sendData(buffer, BYTES_PER_BUFFER);
}

void ST7305::clearDisplay(uint16_t color)
{
    memset(buffer, int(color), BYTES_PER_BUFFER);
}

// 预计算查找表
static uint8_t BIT_MASK_LUT[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

// Y坐标到垂直字节偏移的查找表 (y % 4)
static uint8_t Y_BYTE_OFFSET[4] = {0, 2, 4, 6};

IRAM_ATTR void ST7305::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    // 原始坐标边界检查
    if (x < x_min || x > x_max || y < y_min || y > y_max)
    {
        return;
    }

    // 应用旋转变换
    int16_t new_x, new_y;
    switch (rotation)
    {
    case 1: // 0°
        // No rotation, coordinates unchanged
        new_x = x;
        new_y = y;
        break;
    case 2: // 90°
        // Clockwise 90° rotation
        new_x = PHYSICAL_WIDTH - y - 1;
        new_y = x;
        break;
    case 3: // 180°
        new_x = PHYSICAL_WIDTH - x - 1;
        new_y = PHYSICAL_HEIGHT - y - 1;
        break;
    default: // 270°
        // Clockwise 270° rotation (or 90° counter‑clockwise)
        new_x = y;
        new_y = PHYSICAL_HEIGHT - x - 1;
        break;
    }

    // 旋转后坐标边界检查（使用显示缓冲区的尺寸）
    if (log_out)
        log_i("new_x:%3d new_y:%3d", new_x, new_y);
    if (new_x < 0 || new_x >= PHYSICAL_WIDTH || new_y < 0 || new_y >= PHYSICAL_HEIGHT)
        return;

    // 计算CGRAM中的字节偏移
    uint16_t col = new_x >> 1;   // 每2列共享一个CGRAM字节
    uint8_t y_div4 = new_y >> 2; // y / 4，确定垂直字节位置

    // 计算一维数组索引
    uint16_t byte_index = col * 42 + y_div4;
    if (byte_index < BYTES_PER_BUFFER)
    {
        // 计算位掩码
        uint8_t y_mod4 = new_y & 0x03; // y % 4
        uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
        uint8_t bit_mask = BIT_MASK_LUT[bit_offset];

        // 设置或清除位
        if (color)
        {
            buffer[byte_index] |= bit_mask;
        }
        else
        {
            buffer[byte_index] &= ~bit_mask;
        }
    }
}

// 预计算查找表
static uint8_t BIT_MASK_VLine_LUT[2][8] = {
    {0x00, 0x02, 0x0a, 0x2a, 0xaa, 0xa8, 0xa0, 0x80},
    {0x00, 0x01, 0x05, 0x15, 0x55, 0x54, 0x50, 0x40},
};

/* void ST7305::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    // 边界检查
    if (x < x_min || x > x_max || y < y_min || y > y_max || h <= 0)
        return;

    // 旋转处理，直接调用drawFastHLine
    if (rotation == 1) {
        // 0°
    } else if (rotation == 2) {
        // 90°，竖线变横线
        drawFastHLine(y, PHYSICAL_WIDTH - x - 1, h, color);
        return;
    } else if (rotation == 3) {
        // 180°
        drawFastVLine(PHYSICAL_WIDTH - x - 1, PHYSICAL_HEIGHT - y - h, h, color);
        return;
    } else {
        // 270°，竖线变横线
        drawFastHLine(PHYSICAL_HEIGHT - y - h, x, h, color);
        return;
    }

    // 限制在物理范围内
    int16_t y_end = y + h;
    if (y_end > y_max + 1) y_end = y_max + 1;
    if (y_end > PHYSICAL_HEIGHT) y_end = PHYSICAL_HEIGHT;

    // 逐像素优化为字节操作
    for (int16_t yy = y; yy < y_end;) {
        int16_t col = x >> 1;
        int16_t y_div4 = yy >> 2;
        int16_t byte_index = col * 42 + y_div4;
        int16_t y_mod4 = yy & 0x03;
        int16_t bit_base = Y_BYTE_OFFSET[y_mod4] + (x & 0x01);

        // 最多一次可画8个像素（同一字节）
        int16_t remain = y_end - yy;
        int16_t max_draw = 8 - bit_base;
        int16_t draw_len = remain < max_draw ? remain : max_draw;

        uint8_t mask = BIT_MASK_VLine_LUT[x & 0x01][draw_len];
        mask <<= (8 - bit_base - draw_len);

        if (color)
            buffer[byte_index] |= mask;
        else`
            buffer[byte_index] &= ~mask;

        yy += draw_len;
    }
} */

static uint8_t BIT_MASK_HLine_LUT[4][3] = {
    {0x00, 0x40, 0xc0},
    {0x00, 0x10, 0x30},
    {0x00, 0x04, 0x0c},
    {0x00, 0x01, 0x03},
};

/* void ST7305::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    // 边界检查
    if (y < y_min || y > y_max || x < x_min || x > x_max || w <= 0)
        return;

    // 旋转处理，直接调用drawFastVLine
    if (rotation == 2) {
        // 90°，横线变竖线
        drawFastVLine(y, PHYSICAL_WIDTH - x - 1, w, color);
        return;
    } else if (rotation == 3) {
        // 180°
        drawFastHLine(PHYSICAL_WIDTH - x - w, PHYSICAL_HEIGHT - y - 1, w, color);
        return;
    } else if (rotation == 0) {
        // 270°，横线变竖线
        drawFastVLine(PHYSICAL_HEIGHT - y - 1, x, w, color);
        return;
    }

    // 限制在物理范围内
    int16_t x_end = x + w;
    if (x_end > x_max + 1) x_end = x_max + 1;
    if (x_end > PHYSICAL_WIDTH) x_end = PHYSICAL_WIDTH;

    // 逐像素优化为字节操作
    for (int16_t xx = x; xx < x_end;) {
        int16_t col = xx >> 1;
        int16_t y_div4 = y >> 2;
        int16_t byte_index = col * 42 + y_div4;
        int16_t y_mod4 = y & 0x03;
        int16_t bit_base = Y_BYTE_OFFSET[y_mod4] + (xx & 0x01);

        // 最多一次可画2个像素（同一字节）
        int16_t remain = x_end - xx;
        int16_t max_draw = 2 - (xx & 0x01);
        int16_t draw_len = remain < max_draw ? remain : max_draw;

        uint8_t mask = BIT_MASK_HLine_LUT[y_mod4][draw_len];
        mask >>= (xx & 0x01 ? 0 : 2 - draw_len);

        if (color)
            buffer[byte_index] |= mask;
        else
            buffer[byte_index] &= ~mask;

        xx += draw_len;
    }
} */

void ST7305::setRotation(uint8_t m)
{
    rotation = m % 4; // Ensure rotation is within 0-3
}

void ST7305::setvoltage(st7305_voltage_t fps)
{
    uint8_t table = (uint8_t)fps;
    sendCommand(0xC0);
    sendData(voltageSet[table][0]);
    sendData(voltageSet[table][1]); // Gate Voltage Setting

    sendCommand(0xC1); // VSHP Setting (4.8V)
    sendData(voltageSet[table][2]);
    sendData(voltageSet[table][2]);
    sendData(voltageSet[table][2]);
    sendData(voltageSet[table][2]);

    sendCommand(0xC2); // VSLP Setting (0.98V)
    sendData(voltageSet[table][3]);
    sendData(voltageSet[table][3]);
    sendData(voltageSet[table][3]);
    sendData(voltageSet[table][3]);

    sendCommand(0xC4); // VSHN Setting (-3.6V)
    sendData(voltageSet[table][4]);
    sendData(voltageSet[table][4]);
    sendData(voltageSet[table][4]);
    sendData(voltageSet[table][4]);

    sendCommand(0xC5); // VSLN Setting (0.22V)
    sendData(voltageSet[table][5]);
    sendData(voltageSet[table][5]);
    sendData(voltageSet[table][5]);
    sendData(voltageSet[table][5]);
}

void ST7305::setDrawWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    x_min = x;
    x_max = x + w;
    y_min = y;
    y_max = y + h;
}

void ST7305::setPowerMode(PowerMode mode)
{
    if (mode == POWER_MODE_LPM)
    {
        if (LPM_MODE)
        {
            HPM_MODE = false;
            LPM_MODE = true;
        }
        else
        {
            HPM_MODE = false;
            LPM_MODE = true;

            sendCommand(0x38); // HPM:high Power Mode ON

            sendCommand(0xC0); // Gate Voltage Setting
            sendData(voltageSet[0][0]);
            sendData(voltageSet[0][1]);

            sendCommand(0xC1); // VSHP Setting (4.8V)
            sendData(voltageSet[0][2]);
            sendData(voltageSet[1][2]);
            sendData(voltageSet[2][2]);
            sendData(voltageSet[3][2]);

            sendCommand(0xC2); // VSLP Setting (0.98V)
            sendData(voltageSet[0][3]);
            sendData(voltageSet[1][3]);
            sendData(voltageSet[2][3]);
            sendData(voltageSet[3][3]);

            sendCommand(0xC4); // VSHN Setting (-3.6V)
            sendData(voltageSet[0][4]);
            sendData(voltageSet[1][4]);
            sendData(voltageSet[2][4]);
            sendData(voltageSet[3][4]);

            sendCommand(0xC5); // VSLN Setting (0.22V)
            sendData(voltageSet[0][5]);
            sendData(voltageSet[1][5]);
            sendData(voltageSet[2][5]);
            sendData(voltageSet[3][5]);

            sendCommand(0xC9); // Source Voltage Select
            sendData(0X00);    // VSHP1; VSLP1 ; VSHN1 ; VSLN1

            delay(20);
            sendCommand(0x39); // LPM:Low Power Mode ON
            delay(100);
        }
    }
    else if (mode == POWER_MODE_HPM)
    {
        if (HPM_MODE)
        {
            HPM_MODE = true;
            LPM_MODE = false;
        }
        else
        {
            HPM_MODE = true;
            LPM_MODE = false;
            sendCommand(0x39);
            sendCommand(0x38); // HPM:high Power Mode ON
            delay(300);

            sendCommand(0xC0); // Gate Voltage Setting
            sendData(voltageSet[1][0]);
            sendData(voltageSet[1][1]);

            sendCommand(0xC1); // VSHP Setting (4.8V)
            sendData(voltageSet[0][2]);
            sendData(voltageSet[1][2]);
            sendData(voltageSet[2][2]);
            sendData(voltageSet[3][2]);

            sendCommand(0xC2); // VSLP Setting (0.98V)
            sendData(voltageSet[0][3]);
            sendData(voltageSet[1][3]);
            sendData(voltageSet[2][3]);
            sendData(voltageSet[3][3]);

            sendCommand(0xC4); // VSHN Setting (-3.6V)
            sendData(voltageSet[0][4]);
            sendData(voltageSet[1][4]);
            sendData(voltageSet[2][4]);
            sendData(voltageSet[3][4]);

            sendCommand(0xC5); // VSLN Setting (0.22V)
            sendData(voltageSet[0][5]);
            sendData(voltageSet[1][5]);
            sendData(voltageSet[2][5]);
            sendData(voltageSet[3][5]);

            sendCommand(0xC9); // Source Voltage Select
            sendData(0X01);    // VSHP1; VSLP1 ; VSHN1 ; VSLN1

            delay(20);
        }
    }
}

void ST7305::display_on(bool enabled)
{
    if (enabled)
    {
        sendCommand(0x29); // DISPLAY ON
    }
    else
    {
        sendCommand(0x28); // DISPLAY OFF
    }
}

void ST7305::display_sleep(bool enabled)
{
    if (enabled)
    {
        if (LPM_MODE)
        {
            setPowerMode(POWER_MODE_HPM); // HPM:high Power Mode ON
            delay(300);
        }
        sendCommand(0x10); // sleep ON
        delay(100);
    }
    else
    {
        sendCommand(0x11); // sleep OFF
        delay(100);
    }
}

void ST7305::display_Inversion(bool enabled)
{
    if (enabled)
    {
        sendCommand(0x21); // Display Inversion On
    }
    else
    {
        sendCommand(0x20); // Display Inversion Off
    }
}