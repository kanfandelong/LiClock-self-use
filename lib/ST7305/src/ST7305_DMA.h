#ifndef _ST7305_DMA_H_
#define _ST7305_DMA_H_

#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include <driver/spi_master.h>

#define MAX_X 384
#define MAX_Y 168
#define PHYSICAL_WIDTH 384
#define PHYSICAL_HEIGHT 168
// 预定义常量，避免重复计算
#define TOTAL_ROWS 42								  // 总行数
#define BYTES_PER_ROW 192							  // 每行的字节数
#define BYTES_PER_BUFFER (TOTAL_ROWS * BYTES_PER_ROW) // 每个缓冲区的字节数

typedef enum
{
	fps_0003,
	fps_0001,
	fps_5100,
	fps_5100_2,
	fps_3200,
	fps_3200_2,
	fps_1600
} ST7305_DMA_voltage_t;

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

// 滑动方向枚举
enum SlideDirection
{
	SLIDE_RIGHT,
	SLIDE_LEFT,
	SLIDE_DOWN,
	SLIDE_UP
};

class ST7305_DMA : public Adafruit_GFX
{
public:
	/**
	 * @brief Construct a new ST7305 display driver instance.
	 *
	 * This constructor initializes the display dimensions, SPI host configuration,
	 * and all required GPIO pins. The object inherits from @c Adafruit_GFX to provide
	 * graphics primitives.
	 *
	 * @param w         Width of the display in pixels.
	 * @param h         Height of the display in pixels.
	 * @param spi_host  SPI host device identifier (e.g., HSPI_HOST, VSPI_HOST).
	 * @param sclk_pin  GPIO pin number for the SPI clock (SCLK).
	 * @param mosi_pin  GPIO pin number for MOSI (master‑out‑slave‑in).
	 * @param miso_pin  GPIO pin number for MISO (master‑in‑slave‑out). Use -1 if not used.
	 * @param cs_pin    GPIO pin number for chip‑select (CS).
	 * @param dc_pin    GPIO pin number for data/command control.
	 * @param rst_pin   GPIO pin number for hardware reset.
	 * @param te_pin    GPIO pin number for TE (tearing‑effect) signal; defaults to -1 (disabled).
	 */
	ST7305_DMA(int16_t w, int16_t h,
			   spi_host_device_t spi_host,
			   int8_t sclk_pin, int8_t mosi_pin, int8_t miso_pin,
			   int8_t cs_pin, int8_t dc_pin,
			   int8_t rst_pin, int8_t te_pin = -1);

	void sendCommand(uint8_t command);
	void sendData(uint8_t data);
	void sendData(uint8_t *data, size_t len);
	void receiveData(uint8_t *buffer, size_t len);

	bool begin(bool reset = true);
	/**
	 * @brief 刷新显示内容
	 * 该函数将当前缓冲区的内容发送到显示屏。它会等待 TE 信号（如果 TE 引脚有效）以同步刷新，确保显示稳定。
	 * @param ignoreTE 是否忽略 TE 信号直接刷新，默认为 false。当设置为 true 时，函数会立即刷新显示并强制阻塞5ms以确保数据传输完成
	 * @note 频繁使用忽略TE的刷新会因为阻塞导致性能问题
	 */
	void display(bool ignoreTE = false);
	void clearDisplay(uint16_t color = 0XFF);
	void clearScreen(uint16_t color = 0XFF) { clearDisplay(color); };
	void fillScreen(uint16_t color) { clearDisplay(color); };
	void setvoltage(ST7305_DMA_voltage_t fps);
	void drawPixel(int16_t x, int16_t y, uint16_t color);
	void slideOneBlock(SlideDirection dir, uint8_t new_buffer, uint8_t step);
	void slideScreenFull(SlideDirection dir, uint32_t duration_ms, uint8_t new_buffer);
	void drawbitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);
	void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
					 int16_t w, int16_t h, uint16_t color);
	void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
	void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

	void setRotation(uint8_t m);
	void setDrawWindow(int16_t x = 0, int16_t y = 0, int16_t w = MAX_X, int16_t h = MAX_Y);
	void setPowerMode(PowerMode mode);
	void display_on(bool enabled = true);
	void display_sleep(bool enabled = true);
	void display_Inversion(bool enabled);
	void invertDisplay(bool i) { display_Inversion(i); };
	void set_te_interrupt_mode(int mode) { te_interrupt_mode = mode; }
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
	 * @param buffer_index 要切换到的缓冲区索引，取值范围 0~3
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
	// SPIClass *_spi;
public:
	int te_interrupt_mode;
	int8_t _cs_pin;
	int8_t _dc_pin;
	int8_t _rst_pin;
	int8_t _te_pin;
	int8_t _sclk_pin, _mosi_pin, _miso_pin;

private:
	// 同步对象
	SemaphoreHandle_t _te_semaphore;   // TE 信号量（由 ISR 释放）
	SemaphoreHandle_t _dma_mutex;	   // 保护共享数据的互斥量
	TaskHandle_t _display_task_handle; // 后台刷新任务句柄

	// TE 中断服务例程（静态）
	static void IRAM_ATTR te_isr_handler(void *arg);

	// 后台任务函数（静态）
	static void display_task(void *pvParameters);

	// 内部刷屏逻辑（由后台任务调用）
	void displayInternal(int8_t dma_buf_idx);

	void displayBlocking(int8_t dma_buf_idx);

	spi_host_device_t _spi_host;
	spi_device_handle_t _spi_handle; // SPI 设备句柄
	bool _dma_busy;
	spi_transaction_t _dma_trans;

	uint8_t rotation; // 0, 1, 2, or 3 corresponding to 0, 90, 180, 270 degrees
	uint16_t x_min = 0, x_max = MAX_X;
	uint16_t y_min = 0, y_max = MAX_Y;

	uint8_t *buffer;
	uint8_t *dma_buffer[2]; // 两个 DMA 安全缓冲区
	uint8_t *_buffers[4];
	int8_t _active_dma_idx;	 // 当前正在被 DMA 发送的缓冲区索引（-1 表示无）
	int8_t _pending_dma_idx; // 已填充好等待发送的缓冲区索引（-1 表示无）

	bool HPM_MODE = false;
	bool LPM_MODE = true;
	bool log_out = false;

	void initDisplay();
	void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color);
	void drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color);
	static inline uint16_t gx_uint16_min(uint16_t a, uint16_t b)
	{
		return (a < b ? a : b);
	};
	static inline uint16_t gx_uint16_max(uint16_t a, uint16_t b)
	{
		return (a > b ? a : b);
	};
};

#endif // _ST7305_DMA_H_