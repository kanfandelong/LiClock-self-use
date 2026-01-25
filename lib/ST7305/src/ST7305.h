#ifndef _ST7305_H_
#define _ST7305_H_

#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_GFX.h"


#define MAX_X  384
#define MAX_Y  168
#define PHYSICAL_WIDTH  384
#define PHYSICAL_HEIGHT 168
// 预定义常量，避免重复计算
#define BYTES_PER_ROW 42          // 每行的字节数
#define TOTAL_ROWS 192            // 总行数
#define BYTES_PER_BUFFER (BYTES_PER_ROW * TOTAL_ROWS)  // 每个缓冲区的字节数

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
	void setDrawWindow(int16_t x = 0, int16_t y = 0, int16_t w = MAX_X, int16_t h = MAX_Y);
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
	bool cmpBuffer(uint16_t to, uint16_t from)
	{
		if (from > 2 || to > 2)
			return false;
		int value = memcmp(_buffers[to], _buffers[from], 8064);
		if (value == 0)
			return true;
		else
			return false;
	};
	uint8_t* getBuffer()
  	{
    	return buffer;
  	}
	uint16_t current_buffer_idx = 0;

private:
	//VGH-VGL  13 ~ 32v  ,  VSH-VSL  -0.3 ~ +6.2V
	//VSHP电压 = 3.7 + 0.02*设置值，3.7~6.2V
	//VSLP电压 = 0.02*设置值 ，0~2V
	//VSHN电压 = -2.5 - 0.02*设置值，-5.3~2.5V
	//VSLN电压 = 1 - 0.02*设置值，-1.8~1.0V
	//高刷新率需要更低的VGL、VSHN电压，0.25刷新率越低VSHN -2.5V电压

	//0组 1/32 Hz
	//1组 1/51 Hz
	//2组 0.25 ~ 51 Hz
	//3组 1 ~ 32 Hz
	//4组 2 ~ 32 Hz
	//5组 4 ~ 16 Hz
	const uint8_t voltageSet[6][6] = {
		0x08,0x0a,65,0,50,50,//VGH=12V;VGL=-10V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
		0x12,0x0a,115,0,50,50,//VGH=16V;VGL=-10V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
		0x0b,0x0a,115,0,50,50,//VGH=13.5V;VGL=-10V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
		0x08,0x0a,65,0,0,50,   //VGH=12V;VGL=-10V; VSHP2; VSLP2 ; VSHN2 ; VSLN2
		0x08,0x06,65,0,0,50,   //VGH=12V;VGL=-8V; VSHP3; VSLP3 ; VSHN3 ; VSLN3
		0x08,0x06,15,0,0,50,   //VGH=12V;VGL=-8V; VSHP4; VSLP4 ; VSHN4 ; VSLN4
	};
	SPIClass *_spi;
	int8_t _cs_pin;
	int8_t _dc_pin;
	int8_t _rst_pin;
	int8_t _te_pin;
	uint8_t rotation; // 0, 1, 2, or 3 corresponding to 0, 90, 180, 270 degrees
	uint16_t x_min = 0, x_max = MAX_X;
	uint16_t y_min = 0, y_max = MAX_Y;

	uint8_t *buffer;
	uint8_t _buffers[3][192 * 42];

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
