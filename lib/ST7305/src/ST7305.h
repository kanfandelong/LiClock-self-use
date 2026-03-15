#ifndef _ST7305_H_
#define _ST7305_H_

#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_GFX.h"

#define MAX_X 384
#define MAX_Y 168
#define PHYSICAL_WIDTH 384
#define PHYSICAL_HEIGHT 168
// 预定义常量，避免重复计算
#define BYTES_PER_ROW 42							  // 每行的字节数
#define TOTAL_ROWS 192								  // 总行数
#define BYTES_PER_BUFFER (BYTES_PER_ROW * TOTAL_ROWS) // 每个缓冲区的字节数

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

enum PowerMode
{
	POWER_MODE_LPM,
	POWER_MODE_HPM
};
enum blendmode
{
	OR,
	AND,
	XOR
};

class ST7305 : public Adafruit_GFX
{
public:
	ST7305(int16_t w, int16_t h, SPIClass *spi, int8_t cs_pin, int8_t dc_pin,
		   int8_t rst_pin, int8_t te_pin = -1);

	bool begin(bool reset = true);
	void display();
	void clearDisplay(uint16_t color = 0XFF);
	void clearScreen(uint16_t color = 0XFF) { clearDisplay(color); };
	void setvoltage(st7305_voltage_t fps);
	void drawPixel(int16_t x, int16_t y, uint16_t color);
	// void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
	// void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
	void setRotation(uint8_t m);
	void setDrawWindow(int16_t x = 0, int16_t y = 0, int16_t w = MAX_X, int16_t h = MAX_Y);
	void setPowerMode(PowerMode mode);
	void display_on(bool enabled = true);
	void display_sleep(bool enabled = true);
	void display_Inversion(bool enabled);
	void set(uint8_t cmd, uint8_t data);
	void set(uint8_t cmd, uint8_t *data, size_t len);
	void debug_log(bool debug)
	{
		log_out = debug;
	}
	/**
	 * @brief 切换当前显示缓冲区
	 *
	 * 该函数在内部维护4个缓冲区（_buffers），
	 * 通过指定的 buffer_index切换到对应的缓冲区。
	 * 如果索引超出范围，函数直接返回，不会修改当前缓冲区。
	 *
	 * @param buffer_index 要切换到的缓冲区索引，取值范围 0~2
	 */
	void swapBuffer(uint16_t buffer_index)
	{
		if (buffer_index > 3)
			return;
		current_buffer_idx = buffer_index;
		buffer = _buffers[buffer_index];
	};
	/**
	 * @brief 复制缓冲区内容
	 *
	 * 将源缓冲区 (from) 的内容复制到目标缓冲区 (to)。
	 * 两个缓冲区的索引均必须在 0-3 范围内，
	 * 超出范围时函数直接返回，不执行复制操作。
	 *
	 * @param to   目标缓冲区索引
	 * @param from 源缓冲区索引
	 */
	void copyBuffer(uint16_t to, uint16_t from)
	{
		if (from > 3 || to > 3)
			return;
		memcpy(_buffers[to], _buffers[from], 8064);
	};
	/**
	 * @brief 比较两个缓冲区是否相同
	 *
	 * 使用 memcmp 对两个缓冲区的内容进行逐字节比较，
	 * 若完全相同返回 true，否则返回 false。
	 * 索引超出范围时直接返回 false。
	 *
	 * @param to   目标缓冲区索引
	 * @param from 源缓冲区索引
	 * @return true  两缓冲区内容完全相同
	 * @return false 两缓冲区内容不同或索引非法
	 */
	bool cmpBuffer(uint16_t to, uint16_t from)
	{
		if (from > 3 || to > 3)
			return false;
		int value = memcmp(_buffers[to], _buffers[from], 8064);
		if (value == 0)
			return true;
		else
			return false;
	};
	/**
	 * @brief 获取当前活动缓冲区的指针
	 *
	 * 返回指向当前选中缓冲区的 uint8_t* 指针，
	 * 供外部直接访问或修改显示数据。
	 *
	 * @return uint8_t* 当前缓冲区指针
	 */
	uint8_t *getBuffer()
	{
		return buffer;
	}
	/**
	 * @brief 将两个缓冲区进行混合（图层合成）
	 *
	 * 该函数在内部遍历指定的源缓冲区 `srcIdx` 与目标缓冲区 `destIdx`
	 * 的每个字节，根据 `mode` 进行位运算后写回目标缓冲区。
	 *
	 * 按位 OR（常用于叠加两层）
	 * 按位 AND
	 * 按位 XOR
	 * 直接覆盖（dst = src）
	 *
	 * 索引超出范围（>3）时函数直接返回，不会修改任何缓冲区。
	 *
	 * @param destIdx 目标缓冲区索引
	 * @param srcIdx  源缓冲区索引
	 * @param mode    混合模式，默认OR
	 */
	void blendBuffers(uint16_t destIdx, uint16_t srcIdx, blendmode mode = OR)
	{
		if (destIdx > 3 || srcIdx > 3)
			return;
		uint32_t *dst = (uint32_t *)_buffers[destIdx];
		uint32_t *src = (uint32_t *)_buffers[srcIdx];

		// 如果屏幕像素逻辑相反，需要将 mode 映射为等效操作
		// 假设有一个全局变量或类成员指示屏幕是否反相
		const bool pixelInverted = true; // 请根据实际情况设置

		blendmode actualMode = mode;
		if (pixelInverted)
		{
			switch (mode)
			{
			case OR:
				actualMode = AND;
				break; // 反相屏上 OR 等效于正常逻辑的 AND
			case AND:
				actualMode = OR;
				break; // 反相屏上 AND 等效于正常逻辑的 OR
			case XOR:
				actualMode = XOR;
				break; // XOR 保持不变
			default:
				actualMode = mode;
				break; // OVERWRITE 不变
			}
		}

		for (size_t i = 0; i < BYTES_PER_BUFFER / 4; ++i)
		{
			uint32_t d = dst[i];
			uint32_t s = src[i];
			uint32_t result;

			switch (actualMode)
			{
			case OR:
				result = d | s;
				break;
			case AND:
				result = d & s;
				break;
			case XOR:
				result = d ^ s;
				break;
			default: // OVERWRITE
				result = s;
				break;
			}
			dst[i] = result;
		}
	}
	uint16_t current_buffer_idx = 0;

private:
	// VGH-VGL  13 ~ 32v  ,  VSH-VSL  -0.3 ~ +6.2V
	// VSHP电压 = 3.7 + 0.02*设置值，3.7~6.2V
	// VSLP电压 = 0.02*设置值 ，0~2V
	// VSHN电压 = -2.5 - 0.02*设置值，-5.3~2.5V
	// VSLN电压 = 1 - 0.02*设置值，-1.8~1.0V
	// 高刷新率需要更低的VGL、VSHN电压，0.25刷新率越低VSHN -2.5V电压

	// 0组 1/32 Hz
	// 1组 1/51 Hz
	// 2组 0.25 ~ 51 Hz
	// 3组 1 ~ 32 Hz
	// 4组 2 ~ 32 Hz
	// 5组 4 ~ 16 Hz
	const uint8_t voltageSet[6][6] =
		{
			{0x08, 0x0a, 65, 0, 50, 50},  // VGH=12V;VGL=-10V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
			{0x12, 0x0a, 115, 0, 50, 50}, // VGH=16V;VGL=-10V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
			{0x0b, 0x0a, 115, 0, 50, 50}, // VGH=13.5V;VGL=-10V; VSHP1; VSLP1 ; VSHN1 ; VSLN1
			{0x08, 0x0a, 65, 0, 0, 50},	  // VGH=12V;VGL=-10V; VSHP2; VSLP2 ; VSHN2 ; VSLN2
			{0x08, 0x06, 65, 0, 0, 50},	  // VGH=12V;VGL=-8V; VSHP3; VSLP3 ; VSHN3 ; VSLN3
			{0x08, 0x06, 15, 0, 0, 50},	  // VGH=12V;VGL=-8V; VSHP4; VSLP4 ; VSHN4 ; VSLN4
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
	uint8_t *_buffers[4];

	bool HPM_MODE = false;
	bool LPM_MODE = true;
	bool log_out = false;
	uint8_t OSCSET = 0x26;

	void sendCommand(uint8_t command);
	void sendData(uint8_t data);
	void sendData(uint8_t *data, size_t len);
	void initDisplay();
	// void convertBuffer();
};

#endif // _ST7305_H_
