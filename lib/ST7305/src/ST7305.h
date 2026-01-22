#ifndef _ST7305_H_
#define _ST7305_H_

#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_GFX.h"

typedef enum
{
	fps_0003,
	fps_0001,
	fps_5100,
	fps_5100_2,
	fps_3200,
	fps_3200_2,
	fps_1600
} st7305_voltage_t;

class ST7305 : public Adafruit_GFX
{
public:
	ST7305(int16_t w, int16_t h, SPIClass *spi, int8_t cs_pin, int8_t dc_pin,
		   int8_t rst_pin, int8_t te_pin = -1);

	bool begin(bool reset = true);
	void display();
	void clearDisplay(uint16_t color = 0XFF);
	void clearScreen(uint16_t color = 0XFF) { clearDisplay(color); };
	void drawPixel(int16_t x, int16_t y, uint16_t color);
	void setRotation(uint8_t m);
	void setvoltage(st7305_voltage_t fps);
	void Low_Power_Mode();
	void High_Power_Mode();
	void display_on(bool enabled = true);
	void display_sleep(bool enabled = true);
	void display_Inversion(bool enabled);
	void set(uint8_t cmd, uint8_t data);
	void set(uint8_t cmd, uint8_t *data, size_t len);
	void swapBuffer(uint16_t buffer_index)
	{
		if (buffer_index > 2)
			return;
		current_buffer_idx = buffer_index;
		buffer = _buffers[buffer_index];
	};
	void copyBuffer(uint16_t to, uint16_t from)
	{
		if (from > 2 || to > 2)
			return;
		memcpy(_buffers[to], _buffers[from], 8064);
	};
	uint16_t current_buffer_idx = 0;

private:
	// 0.25Hz 0x12,0x14,115,0,75,50,
	// 1Hz 0x08,0x06,115,0,50,50,
	// 32Hz 0x0a,0x06,115,0,50,50,
	// 51Hz 0x0a,0x06,115,0,50,50,
	const uint8_t voltageSet[7][6] = {
		{0x08, 0x0a, 65, 0, 50, 50},
		{0x0b, 0x0a, 115, 0, 50, 50},
		{0x16, 0x14, 115, 0, 125, 50}, // VGH=17V;VGL=-15V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
		{0x13, 0x1a, 115, 0, 50, 50},
		{0x08, 0x0a, 65, 0, 0, 50},	   // VGH=12V;VGL=-10V; VSHP2; VSLP2 ; VSHN2 ; VSLN2
		{0x08, 0x06, 65, 0, 50},	   // VGH=12V;VGL=-8V; VSHP3; VSLP3 ; VSHN3 ; VSLN3
		{0x08, 0x06, 15, 0, 0, 50}	   // VGH=12V;VGL=-8V; VSHP4; VSLP4 ; VSHN4 ; VSLN4
	};
	SPIClass *_spi;
	int8_t _cs_pin;
	int8_t _dc_pin;
	int8_t _rst_pin;
	int8_t _te_pin;
	uint8_t rotation; // 0, 1, 2, or 3 corresponding to 0, 90, 180, 270 degrees

	uint8_t *buffer; // 384 * 21 bytes (each byte maps to 8 vertical pixels)
	uint8_t _buffers[3][8064];

	bool HPM_MODE = false;
	bool LPM_MODE = false;
	uint8_t OSCSET = 0x26;

	void sendCommand(uint8_t command);
	void sendData(uint8_t data);
	void sendData(uint8_t *data, size_t len);
	void initDisplay();
	// void convertBuffer();
};

#endif // _ST7305_H_
