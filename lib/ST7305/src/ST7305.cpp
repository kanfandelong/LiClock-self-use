#include "ST7305.h"

ST7305::ST7305(int16_t w, int16_t h, SPIClass *spi, int8_t cs_pin, int8_t dc_pin,
               int8_t rst_pin, int8_t te_pin) : Adafruit_GFX(w, h),
                                                _spi(spi),
                                                _cs_pin(cs_pin),
                                                _dc_pin(dc_pin),
                                                _rst_pin(rst_pin),
                                                _te_pin(te_pin), rotation(0)
{ // Initialize rotation to

    // buffer = (uint8_t *)malloc(384 * 21);
    buffer = _buffers[0];
    memset(_buffers[0], 0xFF, sizeof(_buffers[0]));
    memset(_buffers[1], 0xFF, sizeof(_buffers[1]));
    memset(_buffers[2], 0xFF, sizeof(_buffers[2]));
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

bool ST7305::begin(bool reset)
{
    // Initialize pins
    pinMode(_cs_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_rst_pin, OUTPUT_OPEN_DRAIN | PULLUP);
    if (_te_pin >= 0)
    {
        pinMode(_te_pin, INPUT);
    }

    digitalWrite(_cs_pin, HIGH);

    if (reset){
        // Hardware reset
        gpio_hold_dis((gpio_num_t)_rst_pin);
        digitalWrite(_rst_pin, HIGH);
        delay(50);
        digitalWrite(_rst_pin, LOW);
        delay(100);
        digitalWrite(_rst_pin, HIGH);
        gpio_hold_en((gpio_num_t)_rst_pin);
    }
    // Initialize display
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
    _spi->transfer(data, len);
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
    sendCommand(0xD6); sendData(0x13); sendData(0x02);  // NVM Load Control
    sendCommand(0xD1); sendData(0x01);  // Booster Enable
    sendCommand(0xC0); sendData(0x08); sendData(0x06);  // Gate Voltage Setting
    
    sendCommand(0xC1);  // VSHP Setting (4.8V)
    sendData(0x3C); sendData(0x3E); sendData(0x3C); sendData(0x3C);
    
    sendCommand(0xC2);  // VSLP Setting (0.98V)
    sendData(0x23); sendData(0x21); sendData(0x23); sendData(0x23);
    
    sendCommand(0xC4);  // VSHN Setting (-3.6V)
    sendData(0x5A); sendData(0x5C); sendData(0x5A); sendData(0x5A);
    
    sendCommand(0xC5);  // VSLN Setting (0.22V)
    sendData(0x37); sendData(0x35); sendData(0x37); sendData(0x37);
    
    sendCommand(0xB2); sendData(0x05);  // Frame Rate Control
    
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
    
    sendCommand(0xB7); sendData(0x13);  // Source EQ Enable
    sendCommand(0xB0); sendData(0x60);  // Gate Line Setting: 384 line
    
    sendCommand(0x11);  // Sleep out
    delay(100);
    
    sendCommand(0xC9); sendData(0x00);  // Source Voltage Select
    sendCommand(0x36); sendData(0x00);  // Memory Data Access Control
    sendCommand(0x3A); sendData(0x11);  // Data Format Select
    sendCommand(0xB9); sendData(0x20);  // Gamma Mode Setting
    sendCommand(0xB8); sendData(0x29);  // Panel Setting
    
    sendCommand(0x2A); sendData(0x17); sendData(0x24);  // Column Address Setting
    sendCommand(0x2B); sendData(0x00); sendData(0xBF);  // Row Address Setting
    
    if (_te_pin >= 0) {
        sendCommand(0x35); sendData(0x00);  // TE
    }
    
    sendCommand(0xD0); sendData(0xFF);  // Auto power down
    sendCommand(0x39);  // 低功耗模式
    sendCommand(0x29);  // Display on
    setvoltage(fps_5100);
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
    sendData(buffer, 8064);

    // log_i("display time: %ld ms", millis() - begin);
}

void ST7305::clearDisplay(uint16_t color)
{
    memset(buffer, int(color), 384 * 21);
}

void ST7305::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= width() || y < 0 || y >= height())
    {
        return;
    }

    int16_t new_x, new_y;

    switch (rotation)
    {
    case 1: // 90 degrees
        new_x = y;
        new_y = x;
        break;
    case 2: // 180 degrees
        new_x = x;
        new_y = width() - y - 1;
        break;
    case 3: // 270 degrees
        new_x = height() - y - 1;
        new_y = width() - x - 1;
        break;
    default: // 0 degrees
        new_x = height() - x - 1;
        new_y = y;
        break;
    }

    // Calculate byte position and bit position within byte
    // uint16_t byte_idx = (new_y >> 3) * 384 + new_x;
    // uint8_t bit_pos = new_y & 0x07;

    // if (color) {
    //     buffer[byte_idx] |= (1 << bit_pos);
    // } else {
    //     buffer[byte_idx] &= ~(1 << bit_pos);
    // }

    uint real_x = new_x / 4; // 0->0, 3->0, 4->1, 7->1
    uint real_y = new_y / 2; // 0->0, 1->0, 2->1, 3->1
    uint write_byte_index = real_y * 42 + real_x;
    uint one_two = (new_y % 2 == 0) ? 0 : 1; // 0 1
    uint line_bit_4 = new_x % 4;             // 0 1 2 3
    uint8_t write_bit = 7 - (line_bit_4 * 2 + one_two);

    // Serial.printf("x: %u, y: %u, real_y: %u, real_x: %u, write_byte_index: %u, one_two: %u, line_bit_4: %u, write_bit: %u\n\n", x, y, real_y, real_x, write_byte_index, one_two, line_bit_4, write_bit);

    if (color != 0)
    {
        // 将指定位置的 bit 置为 1
        buffer[write_byte_index] |= (1 << write_bit);
    }
    else
    {
        // 将指定位置的 bit 置为 0
        buffer[write_byte_index] &= ~(1 << write_bit);
    }
}
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

void ST7305::Low_Power_Mode()
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

        // VLC=3.6V (12/-5)(delta Vp=0.6V)
        sendCommand(0xC1); // VSHP Setting (4.8V)
        sendData(115);     // VSHP1
        sendData(0X3E);    // VSHP2
        sendData(0X3C);    // VSHP3
        sendData(0X3C);    // VSHP4

        sendCommand(0xC2); // VSLP Setting (0.98V)
        sendData(0);       // VSLP1
        sendData(0X21);    // VSLP2
        sendData(0X23);    // VSLP3
        sendData(0X23);    // VSLP4

        sendCommand(0xC4); // VSHN Setting (-3.6V)
        sendData(50);      // VSHN1
        sendData(0X5C);    // VSHN2
        sendData(0X5A);    // VSHN3
        sendData(0X5A);    // VSHN4

        sendCommand(0xC5); // VSLN Setting (0.22V)
        sendData(50);      // VSLN1
        sendData(0X35);    // VSLN2
        sendData(0X37);    // VSLN3
        sendData(0X37);    // VSLN4

        sendCommand(0xC9); // Source Voltage Select
        sendData(0X00);    // VSHP1; VSLP1 ; VSHN1 ; VSLN1

        delay(20);

        sendCommand(0x39); // LPM:Low Power Mode ON
        delay(100);
    }
}

void ST7305::High_Power_Mode()
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

        sendCommand(0x38); // HPM:high Power Mode ON
        delay(10);
        sendCommand(0xC0); // Gate Voltage Setting
        sendData(0X12);    // VGH 00:8V  04:10V  08:12V   0E:15V   12:17V
        sendData(0X0a);    // VGL 00:-5V   04:-7V   0A:-10V
        // VLC=3.6V (12/-5)(delta Vp=0.6V)
        sendCommand(0xC1); // VSHP Setting (4.8V)
        sendData(115);     // VSHP1
        sendData(0X3E);    // VSHP2
        sendData(0X3C);    // VSHP3
        sendData(0X3C);    // VSHP4

        sendCommand(0xC2); // VSLP Setting (0.98V)
        sendData(0);       // VSLP1
        sendData(0X21);    // VSLP2
        sendData(0X23);    // VSLP3
        sendData(0X23);    // VSLP4

        sendCommand(0xC4); // VSHN Setting (-3.6V)
        sendData(50);      // VSHN1
        sendData(0X5C);    // VSHN2
        sendData(0X5A);    // VSHN3
        sendData(0X5A);    // VSHN4

        sendCommand(0xC5); // VSLN Setting (0.22V)
        sendData(50);      // VSLN1
        sendData(0X35);    // VSLN2
        sendData(0X37);    // VSLN3
        sendData(0X37);    // VSLN4

        sendCommand(0xC9); // Source Voltage Select
        sendData(0X00);    // VSHP1; VSLP1 ; VSHN1 ; VSLN1

        delay(10);
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
            sendCommand(0x38); // HPM:high Power Mode ON
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