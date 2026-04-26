#include "ST7305_DMA.h"

ST7305_DMA::ST7305_DMA(int16_t w, int16_t h,
                       spi_host_device_t spi_host,
                       int8_t sclk_pin, int8_t mosi_pin, int8_t miso_pin,
                       int8_t cs_pin, int8_t dc_pin,
                       int8_t rst_pin, int8_t te_pin) : Adafruit_GFX(w, h),
                                                        _spi_host(spi_host),
                                                        _sclk_pin(sclk_pin),
                                                        _mosi_pin(mosi_pin),
                                                        _miso_pin(miso_pin),
                                                        _cs_pin(cs_pin),
                                                        _dc_pin(dc_pin),
                                                        _rst_pin(rst_pin),
                                                        _te_pin(te_pin),
                                                        rotation(0)
{ // Initialize rotation to

    memset(&_dma_trans, 0, sizeof(_dma_trans));
    // 像素数据结构为：
    // P1 P3 P5 P7
    // P2 P5 P6 P8

    // P0 P2 P4 P6
    // P1 P3 P5 P7

    // 对应一个byte数据的：
    // BIT7 BIT5 BIT3 BIT1
    // BIT6 BIT4 BIT2 BIT0
}

static void IRAM_ATTR st7305_pre_transfer_cb(spi_transaction_t *t)
{
    uintptr_t combined = (uintptr_t)t->user;
    uint8_t dc = combined & 1;
    ST7305_DMA *instance = (ST7305_DMA *)(combined & ~1);
    gpio_set_level((gpio_num_t)instance->_dc_pin, dc);
}

bool ST7305_DMA::begin(bool reset)
{
    esp_err_t ret;

    // 分配 4 个绘制缓冲区（仍使用 PSRAM 友好分配）
    log_i("缓冲区初始化...");
    if (!psramFound())
    {
        _buffers[0] = (uint8_t *)malloc(BYTES_PER_BUFFER);
        _buffers[1] = (uint8_t *)malloc(BYTES_PER_BUFFER);
        _buffers[2] = (uint8_t *)malloc(BYTES_PER_BUFFER);
        _buffers[3] = (uint8_t *)malloc(BYTES_PER_BUFFER);
    }
    else
    {
        _buffers[0] = (uint8_t *)ps_malloc(BYTES_PER_BUFFER);
        _buffers[1] = (uint8_t *)ps_malloc(BYTES_PER_BUFFER);
        _buffers[2] = (uint8_t *)ps_malloc(BYTES_PER_BUFFER);
        _buffers[3] = (uint8_t *)ps_malloc(BYTES_PER_BUFFER);
    }
    buffer = _buffers[0];
    memset(_buffers[0], 0xFF, BYTES_PER_BUFFER);
    memset(_buffers[1], 0xFF, BYTES_PER_BUFFER);
    memset(_buffers[2], 0xFF, BYTES_PER_BUFFER);
    memset(_buffers[3], 0xFF, BYTES_PER_BUFFER);

    // 分配 DMA 专用缓冲区（要求内存具备 DMA 能力）
    log_i("分配 DMA 缓冲区...");
    dma_buffer[0] = (uint8_t *)heap_caps_malloc(BYTES_PER_BUFFER, MALLOC_CAP_DMA);
    dma_buffer[1] = (uint8_t *)heap_caps_malloc(BYTES_PER_BUFFER, MALLOC_CAP_DMA);
    if (!dma_buffer[0] || !dma_buffer[1])
    {
        log_e("DMA 缓冲区分配失败");
        return false;
    }
    _active_dma_idx = -1;
    _pending_dma_idx = -1;

    // 创建同步对象
    _te_semaphore = xSemaphoreCreateBinary();
    _dma_mutex = xSemaphoreCreateMutex();
    if (!_te_semaphore || !_dma_mutex)
    {
        log_e("信号量创建失败");
        return false;
    }

    // 创建后台任务
    log_i("启动后台刷新任务...");
    xTaskCreatePinnedToCore(
        display_task,          // 任务函数
        "st7305_disp",         // 任务名
        4096,                  // 栈大小（字节）
        this,                  // 任务参数（传递对象实例）
        6,                     // 优先级（可调）
        &_display_task_handle, // 任务句柄
        1                      // 运行核心（通常为1）
    );

    // 配置 TE 引脚中断（如果 TE 引脚有效）
    log_i("TE 引脚及中断初始化...");
    if (_te_pin >= 0)
    {
        pinMode(_te_pin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(_te_pin), te_isr_handler, this, RISING);
    }
    else
    {
        log_w("TE 引脚未配置, 请使用display(true)忽略TE信号, 以确保正常刷新");
    }

    pinMode(_cs_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_rst_pin, OUTPUT_OPEN_DRAIN | PULLUP);
    digitalWrite(_cs_pin, HIGH);

    // 硬件复位
    log_i("屏幕复位...");
    if (reset)
    {
        gpio_hold_dis((gpio_num_t)_rst_pin);
        digitalWrite(_rst_pin, HIGH);
        delay(50);
        digitalWrite(_rst_pin, LOW);
        delay(5);
        digitalWrite(_rst_pin, HIGH);
        gpio_hold_en((gpio_num_t)_rst_pin);
        delay(120);
    }

    // 初始化 SPI 总线
    log_i("初始化 SPI 总线...");
    spi_bus_config_t buscfg = {
        .mosi_io_num = _mosi_pin,
        .miso_io_num = -1,
        .sclk_io_num = _sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BYTES_PER_BUFFER + 8, // 稍大于最大传输
    };
    ret = spi_bus_initialize(_spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        log_e("SPI 总线初始化失败: %d", ret);
        return false;
    }

    // 添加设备
    spi_device_interface_config_t devcfg = {};
    devcfg.mode = 0;                          // SPI 模式 0
    devcfg.clock_speed_hz = 33 * 1000 * 1000; // 33 MHz
    devcfg.spics_io_num = _cs_pin;
    devcfg.queue_size = 7;
    devcfg.pre_cb = st7305_pre_transfer_cb; // 设置 DC 的回调
    devcfg.flags = 0;

    ret = spi_bus_add_device(_spi_host, &devcfg, &_spi_handle);
    if (ret != ESP_OK)
    {
        log_e("添加 SPI 设备失败: %d", ret);
        spi_bus_free(_spi_host);
        return false;
    }

    // 初始化屏幕寄存器
    log_i("初始化屏幕...");
    initDisplay();

    clearDisplay();
    return true;
}

void ST7305_DMA::sendCommand(uint8_t cmd)
{
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void *)((uintptr_t)this | 0); // DC = 0
    esp_err_t ret = spi_device_polling_transmit(_spi_handle, &t);
    if (ret != ESP_OK)
    {
        log_e("sendCommand 失败: %d", ret);
    }
}

void ST7305_DMA::sendData(uint8_t data)
{
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    t.user = (void *)((uintptr_t)this | 1); // DC = 1
    esp_err_t ret = spi_device_polling_transmit(_spi_handle, &t);
    if (ret != ESP_OK)
    {
        log_e("sendData 失败: %d", ret);
    }
}

void ST7305_DMA::sendData(uint8_t *data, size_t len)
{
    if (len == 0)
        return;
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = data;
    t.user = (void *)((uintptr_t)this | 1); // DC = 1
    esp_err_t ret = spi_device_polling_transmit(_spi_handle, &t);
    if (ret != ESP_OK)
    {
        log_e("sendData 块失败: %d", ret);
    }
}

void ST7305_DMA::set(uint8_t cmd, uint8_t data)
{
    sendCommand(cmd);
    sendData(data);
}

void ST7305_DMA::set(uint8_t cmd, uint8_t *data, size_t len)
{
    sendCommand(cmd);
    sendData(data, len);
}

void ST7305_DMA::initDisplay()
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

void ST7305_DMA::display(bool ignoreTE)
{
    if (ignoreTE)
    {
        // ---------- 阻塞式同步刷新 ----------
        xSemaphoreTake(_dma_mutex, portMAX_DELAY);

        // 等待上一次 DMA 传输完成（如果有）
        if (_active_dma_idx != -1)
        {
            spi_transaction_t *rtrans;
            spi_device_get_trans_result(_spi_handle, &rtrans, portMAX_DELAY);
            _active_dma_idx = -1;
        }

        uint8_t caset[] = {0x17, 0x17 + 14 - 1};
        uint8_t raset[] = {0x00, 0x00 + 192 - 1};

        sendCommand(0x2A);
        sendData(caset, sizeof(caset));

        sendCommand(0x2B);
        sendData(raset, sizeof(raset));

        sendCommand(0x2C);
        sendData(buffer, BYTES_PER_BUFFER);

        _active_dma_idx = -1;
        _pending_dma_idx = -1;

        xSemaphoreGive(_dma_mutex);
    }
    else
    {
        // ---------- 非阻塞 TE 异步刷新 ----------
        xSemaphoreTake(_dma_mutex, portMAX_DELAY);

        int8_t target_idx;
        if (_pending_dma_idx != -1)
        {
            // 已有待发送帧，直接覆盖该缓冲区（丢弃旧帧）
            target_idx = _pending_dma_idx;
        }
        else
        {
            // 无待发送帧，选择一个空闲缓冲区
            // 空闲缓冲区是当前不在发送的那个（如果 _active_dma_idx 为 -1，两个都空闲，选 0）
            if (_active_dma_idx == 0)
            {
                target_idx = 1;
            }
            else if (_active_dma_idx == 1)
            {
                target_idx = 0;
            }
            else
            {
                target_idx = 0; // 无活跃传输，随便选
            }
        }

        // 将当前绘制缓冲区内容复制到目标 DMA 缓冲区
        memcpy(dma_buffer[target_idx], buffer, BYTES_PER_BUFFER);

        // 更新待发送索引
        _pending_dma_idx = target_idx;

        // 释放互斥锁
        xSemaphoreGive(_dma_mutex);
    }
}

void IRAM_ATTR ST7305_DMA::te_isr_handler(void *arg)
{
    ST7305_DMA *instance = (ST7305_DMA *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(instance->_te_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void ST7305_DMA::display_task(void *pvParameters)
{
    ST7305_DMA *d = (ST7305_DMA *)pvParameters;
    spi_transaction_t *rtrans;
    esp_err_t ret;

    while (1)
    {
        // 等待 TE 信号量（无限等待）
        if (xSemaphoreTake(d->_te_semaphore, portMAX_DELAY) == pdTRUE)
        {
            // 获取互斥锁，保护共享数据
            xSemaphoreTake(d->_dma_mutex, portMAX_DELAY);

            // 检查是否有待发送的帧
            if (d->_pending_dma_idx != -1)
            {
                // 如果有上一次 DMA 传输尚未完成，等待它完成
                if (d->_active_dma_idx != -1)
                {
                    // 等待上一次传输完成
                    ret = spi_device_get_trans_result(d->_spi_handle, &rtrans, portMAX_DELAY);
                    if (ret == ESP_OK)
                    {
                        d->_active_dma_idx = -1; // 标记传输完成
                    }
                    else
                    {
                        log_e("等待上一次 DMA 完成失败: %d", ret);
                    }
                }

                // 现在可以启动新传输
                int8_t buf_to_send = d->_pending_dma_idx;
                d->_active_dma_idx = buf_to_send; // 标记为正在发送
                d->_pending_dma_idx = -1;         // 清空待发送标志

                // 释放互斥锁，避免在发送命令期间阻塞其他操作
                xSemaphoreGive(d->_dma_mutex);

                // 执行实际刷屏（发送命令和 DMA 数据）
                d->displayInternal(buf_to_send);

                // 注意：displayInternal 会 queue 新 DMA 事务，完成后 _active_dma_idx 会在下次循环中清除
                // 但为了立即知道完成，我们可以在 displayInternal 后等待完成？这里我们选择不等待，
                // 让下一个 TE 周期去检查完成。但需要确保 displayInternal 不会阻塞太久。
                // 实际上 displayInternal 仅 queue 事务，很快返回。
            }
            else
            {
                // 无待发送帧，释放互斥锁
                xSemaphoreGive(d->_dma_mutex);
            }
        }
    }
}

void ST7305_DMA::displayInternal(int8_t dma_buf_idx)
{
    // 设置窗口
    uint8_t caset[] = {0x17, 0x17 + 14 - 1};
    uint8_t raset[] = {0x00, 0x00 + 192 - 1};
    sendCommand(0x2A);
    sendData(caset, sizeof(caset));
    sendCommand(0x2B);
    sendData(raset, sizeof(raset));

    // 发送 0x2C 命令
    spi_transaction_t cmd_trans = {};
    uint8_t cmd_2c = 0x2C;
    cmd_trans.length = 8;
    cmd_trans.tx_buffer = &cmd_2c;
    cmd_trans.user = (void *)((uintptr_t)this | 0); // DC = 0
    cmd_trans.flags = 0;                            // 保持 CS 到下一个事务
    esp_err_t ret = spi_device_polling_transmit(_spi_handle, &cmd_trans);
    if (ret != ESP_OK)
    {
        log_e("发送 0x2C 命令失败: %d", ret);
        return;
    }

    // 准备 DMA 数据事务
    memset(&_dma_trans, 0, sizeof(_dma_trans));
    _dma_trans.length = BYTES_PER_BUFFER * 8;
    _dma_trans.tx_buffer = dma_buffer[dma_buf_idx];
    _dma_trans.user = (void *)((uintptr_t)this | 1); // DC = 1
    _dma_trans.flags = 0;                            // 事务结束后释放 CS

    ret = spi_device_queue_trans(_spi_handle, &_dma_trans, portMAX_DELAY);
    if (ret != ESP_OK)
    {
        log_e("DMA 队列提交失败: %d", ret);
        return;
    }
    // 注意：这里不等待完成，由后台任务的下一次 TE 处理完成状态
}

void ST7305_DMA::displayBlocking(int8_t dma_buf_idx)
{
    // 1. 设置显示窗口（同 displayInternal）
    uint8_t caset[] = {0x17, 0x17 + 14 - 1};
    uint8_t raset[] = {0x00, 0x00 + 192 - 1};
    sendCommand(0x2A);
    sendData(caset, sizeof(caset));
    sendCommand(0x2B);
    sendData(raset, sizeof(raset));

    // 2. 发送写内存命令（0x2C）
    spi_transaction_t cmd_trans = {};
    uint8_t cmd_2c = 0x2C;
    cmd_trans.length = 8;
    cmd_trans.tx_buffer = &cmd_2c;
    cmd_trans.user = (void *)((uintptr_t)this | 0); // DC = 0
    esp_err_t ret = spi_device_polling_transmit(_spi_handle, &cmd_trans);
    if (ret != ESP_OK)
    {
        log_e("发送 0x2C 命令失败: %d", ret);
        return;
    }

    // 3. 使用阻塞式传输发送整帧像素数据
    spi_transaction_t data_trans = {};
    data_trans.length = BYTES_PER_BUFFER * 8;
    data_trans.tx_buffer = dma_buffer[dma_buf_idx];
    data_trans.user = (void *)((uintptr_t)this | 1);     // DC = 1
    ret = spi_device_transmit(_spi_handle, &data_trans); // 阻塞直到传输完成
    if (ret != ESP_OK)
    {
        log_e("DMA 阻塞传输失败: %d", ret);
    }
}

void ST7305_DMA::clearDisplay(uint16_t color)
{
    memset(buffer, int(color), BYTES_PER_BUFFER);
}

// 预计算查找表
static uint8_t BIT_MASK_LUT[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

// Y坐标到垂直字节偏移的查找表 (y % 4)
static uint8_t Y_BYTE_OFFSET[4] = {0, 2, 4, 6};

IRAM_ATTR void ST7305_DMA::drawPixel(int16_t x, int16_t y, uint16_t color)
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

/**
 * @brief 将当前缓冲区向指定方向滑动一个块（2列×4行或4行×2列，取决于方向）
 * @param dir 滑动方向
 * @param new_buffer 新屏幕缓冲区
 *
 * 根据方向更新当前缓冲区，滑动后空缺部分从新缓冲区相应位置填充。
 */
void ST7305_DMA::slideOneBlock(SlideDirection dir, uint8_t new_buffer, uint8_t step)
{
    if (new_buffer > 3)
        return;
    uint8_t *_new_buffer = _buffers[new_buffer];
    const uint16_t cols = BYTES_PER_ROW; // 水平块数（192）
    const uint16_t rows = TOTAL_ROWS;    // 垂直块数（42）

    switch (dir)
    {
    case SLIDE_LEFT:
    {
        uint16_t new_col = cols - 1 - step;
        for (uint16_t yb = 0; yb < rows; ++yb)
        {
            for (uint16_t col = cols - 1; col > 0; --col)
            {
                buffer[col * rows + yb] = buffer[(col - 1) * rows + yb];
            }
            buffer[0 * rows + yb] = _new_buffer[new_col * rows + yb];
        }
        break;
    }
    case SLIDE_RIGHT:
    {
        uint16_t new_col = step;
        for (uint16_t yb = 0; yb < rows; ++yb)
        {
            // 原内容左移一列
            for (uint16_t col = 0; col < cols - 1; ++col)
            {
                buffer[col * rows + yb] = buffer[(col + 1) * rows + yb];
            }
            buffer[(cols - 1) * rows + yb] = _new_buffer[new_col * rows + yb];
        }
        break;
    }
    case SLIDE_UP:
    {
        uint16_t new_row = rows - 1 - step;
        for (uint16_t col = 0; col < cols; ++col)
        {
            uint8_t *col_start = buffer + col * rows;
            const uint8_t *new_col_start = _new_buffer + col * rows;
            memmove(col_start + 1, col_start, rows - 1);
            col_start[0] = new_col_start[new_row];
        }
        break;
    }
    case SLIDE_DOWN:
    {
        uint16_t new_row = step;
        for (uint16_t col = 0; col < cols; ++col)
        {
            uint8_t *col_start = buffer + col * rows;
            const uint8_t *new_col_start = _new_buffer + col * rows;
            memmove(col_start, col_start + 1, rows - 1);
            col_start[rows - 1] = new_col_start[new_row];
        }
        break;
    }
    }
}

/**
 * @brief 执行全屏滑动特效（带缓动效果）
 * @param dir 滑动方向
 * @param duration_ms 滑动总时长（毫秒）
 * @param new_buffer 目标全屏缓冲区（大小与当前缓冲区相同）
 *
 * 滑动过程按块进行（每块为2列×4行像素），共分192步（左右）或42步（上下）。
 * 每步的延时动态计算，使得越接近终点速度越慢，总时长近似为 duration_ms。
 */
void ST7305_DMA::slideScreenFull(SlideDirection dir, uint32_t duration_ms, uint8_t new_buffer)
{
    uint16_t steps = (dir == SLIDE_LEFT || dir == SLIDE_RIGHT) ? BYTES_PER_ROW : TOTAL_ROWS;
    uint32_t remaining_time = duration_ms;

    for (uint16_t step = 0; step < steps; ++step)
    {
        uint32_t delay_ms = remaining_time / (steps - step);
        if (delay_ms == 0)
            delay_ms = 1;

        slideOneBlock(dir, new_buffer, step); // 传入当前步数
        display();
        delay(delay_ms); // 请替换为实际延时函数
        remaining_time -= delay_ms;
    }
}

void ST7305_DMA::drawbitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color)
{
    const int16_t physWidth = PHYSICAL_WIDTH;
    const int16_t physHeight = PHYSICAL_HEIGHT;
    const int16_t xStart = x, yStart = y;
    const uint8_t bytesPerRow = (w + 7) / 8;

    switch (rotation)
    {
    case 1: // 0°
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = yStart + j;
            if (srcY < 0 || srcY >= physHeight)
                continue; // 跳过整行
            for (int16_t i = 0; i < w; ++i)
            {
                int16_t srcX = xStart + i;
                if (srcX < 0 || srcX >= physWidth)
                    continue;

                uint8_t b = pgm_read_byte(&bitmap[j * bytesPerRow + (i >> 3)]);
                bool pixel = (b & (0x80 >> (i & 7))) == 0; // 位为0时绘制
                if (pixel)
                {
                    uint16_t col = srcX >> 1;
                    uint8_t y_div4 = srcY >> 2;
                    uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                    uint8_t y_mod4 = srcY & 0x03;
                    uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (srcX & 0x01);
                    uint8_t bit_mask = BIT_MASK_LUT[bit_offset];
                    if (color)
                        buffer[byte_index] |= bit_mask;
                    else
                        buffer[byte_index] &= ~bit_mask;
                }
            }
        }
        break;
    }
    case 2: // 90°
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = yStart + j;
            for (int16_t i = 0; i < w; ++i)
            {
                int16_t srcX = xStart + i;
                int16_t new_x = physWidth - srcY - 1;
                int16_t new_y = srcX;
                if (new_x < 0 || new_x >= physWidth || new_y < 0 || new_y >= physHeight)
                    continue;

                uint8_t b = pgm_read_byte(&bitmap[j * bytesPerRow + (i >> 3)]);
                bool pixel = (b & (0x80 >> (i & 7))) == 0;
                if (pixel)
                {
                    uint16_t col = new_x >> 1;
                    uint8_t y_div4 = new_y >> 2;
                    uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                    uint8_t y_mod4 = new_y & 0x03;
                    uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                    uint8_t bit_mask = BIT_MASK_LUT[bit_offset];
                    if (color)
                        buffer[byte_index] |= bit_mask;
                    else
                        buffer[byte_index] &= ~bit_mask;
                }
            }
        }
        break;
    }
    case 3: // 180°
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = yStart + j;
            for (int16_t i = 0; i < w; ++i)
            {
                int16_t srcX = xStart + i;
                int16_t new_x = physWidth - srcX - 1;
                int16_t new_y = physHeight - srcY - 1;
                if (new_x < 0 || new_x >= physWidth || new_y < 0 || new_y >= physHeight)
                    continue;

                uint8_t b = pgm_read_byte(&bitmap[j * bytesPerRow + (i >> 3)]);
                bool pixel = (b & (0x80 >> (i & 7))) == 0;
                if (pixel)
                {
                    uint16_t col = new_x >> 1;
                    uint8_t y_div4 = new_y >> 2;
                    uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                    uint8_t y_mod4 = new_y & 0x03;
                    uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                    uint8_t bit_mask = BIT_MASK_LUT[bit_offset];
                    if (color)
                        buffer[byte_index] |= bit_mask;
                    else
                        buffer[byte_index] &= ~bit_mask;
                }
            }
        }
        break;
    }
    default: // 270°
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = yStart + j;
            for (int16_t i = 0; i < w; ++i)
            {
                int16_t srcX = xStart + i;
                int16_t new_x = srcY;
                int16_t new_y = physHeight - srcX - 1;
                if (new_x < 0 || new_x >= physWidth || new_y < 0 || new_y >= physHeight)
                    continue;

                uint8_t b = pgm_read_byte(&bitmap[j * bytesPerRow + (i >> 3)]);
                bool pixel = (b & (0x80 >> (i & 7))) == 0;
                if (pixel)
                {
                    uint16_t col = new_x >> 1;
                    uint8_t y_div4 = new_y >> 2;
                    uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                    uint8_t y_mod4 = new_y & 0x03;
                    uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                    uint8_t bit_mask = BIT_MASK_LUT[bit_offset];
                    if (color)
                        buffer[byte_index] |= bit_mask;
                    else
                        buffer[byte_index] &= ~bit_mask;
                }
            }
        }
        break;
    }
    }
}

void ST7305_DMA::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                             int16_t w, int16_t h, uint16_t color)
{

    const int16_t physWidth = PHYSICAL_WIDTH;
    const int16_t physHeight = PHYSICAL_HEIGHT;
    const int16_t byteWidth = (w + 7) / 8; // 每行字节数（整字节对齐）

    // 根据旋转角度分别处理，避免循环内分支
    switch (rotation)
    {
    case 1: // 0°
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t dstY = y + j; // 目标 Y 坐标
            if (dstY < 0 || dstY >= physHeight)
                continue; // 跳过完全超出屏幕的行

            for (int16_t i = 0; i < w; ++i)
            {
                // 获取当前像素的位（LSB 对应第一个像素）
                uint8_t b;
                if (i & 7)
                    b >>= 1;
                else
                    b = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);

                if (b & 0x01) // 位为 1 时绘制
                {
                    int16_t new_x = x + i;
                    int16_t new_y = dstY;

                    // 边界检查
                    if (new_x >= 0 && new_x < physWidth && new_y >= 0 && new_y < physHeight)
                    {
                        // 计算缓冲区索引并设置位
                        uint16_t col = new_x >> 1;
                        uint8_t y_div4 = new_y >> 2;
                        uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                        uint8_t y_mod4 = new_y & 0x03;
                        uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                        uint8_t bit_mask = BIT_MASK_LUT[bit_offset];

                        if (color)
                            buffer[byte_index] |= bit_mask;
                        else
                            buffer[byte_index] &= ~bit_mask;
                    }
                }
            }
        }
        break;
    }

    case 2: // 90° (顺时针旋转)
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = y + j; // 源 Y 坐标

            for (int16_t i = 0; i < w; ++i)
            {
                uint8_t b;
                if (i & 7)
                    b >>= 1;
                else
                    b = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);

                if (b & 0x01)
                {
                    int16_t new_x = physWidth - srcY - 1;
                    int16_t new_y = x + i;

                    if (new_x >= 0 && new_x < physWidth && new_y >= 0 && new_y < physHeight)
                    {
                        uint16_t col = new_x >> 1;
                        uint8_t y_div4 = new_y >> 2;
                        uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                        uint8_t y_mod4 = new_y & 0x03;
                        uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                        uint8_t bit_mask = BIT_MASK_LUT[bit_offset];

                        if (color)
                            buffer[byte_index] |= bit_mask;
                        else
                            buffer[byte_index] &= ~bit_mask;
                    }
                }
            }
        }
        break;
    }

    case 3: // 180°
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = y + j;

            for (int16_t i = 0; i < w; ++i)
            {
                uint8_t b;
                if (i & 7)
                    b >>= 1;
                else
                    b = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);

                if (b & 0x01)
                {
                    int16_t new_x = physWidth - (x + i) - 1;
                    int16_t new_y = physHeight - srcY - 1;

                    if (new_x >= 0 && new_x < physWidth && new_y >= 0 && new_y < physHeight)
                    {
                        uint16_t col = new_x >> 1;
                        uint8_t y_div4 = new_y >> 2;
                        uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                        uint8_t y_mod4 = new_y & 0x03;
                        uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                        uint8_t bit_mask = BIT_MASK_LUT[bit_offset];

                        if (color)
                            buffer[byte_index] |= bit_mask;
                        else
                            buffer[byte_index] &= ~bit_mask;
                    }
                }
            }
        }
        break;
    }

    default: // 270° (顺时针旋转 270°)
    {
        for (int16_t j = 0; j < h; ++j)
        {
            int16_t srcY = y + j;

            for (int16_t i = 0; i < w; ++i)
            {
                uint8_t b;
                if (i & 7)
                    b >>= 1;
                else
                    b = pgm_read_byte(&bitmap[j * byteWidth + (i >> 3)]);

                if (b & 0x01)
                {
                    int16_t new_x = srcY;
                    int16_t new_y = physHeight - (x + i) - 1;

                    if (new_x >= 0 && new_x < physWidth && new_y >= 0 && new_y < physHeight)
                    {
                        uint16_t col = new_x >> 1;
                        uint8_t y_div4 = new_y >> 2;
                        uint16_t byte_index = col * TOTAL_ROWS + y_div4;
                        uint8_t y_mod4 = new_y & 0x03;
                        uint8_t bit_offset = Y_BYTE_OFFSET[y_mod4] + (new_x & 0x01);
                        uint8_t bit_mask = BIT_MASK_LUT[bit_offset];

                        if (color)
                            buffer[byte_index] |= bit_mask;
                        else
                            buffer[byte_index] &= ~bit_mask;
                    }
                }
            }
        }
        break;
    }
    }
}

// 内部函数：画垂直线（物理坐标，已裁剪）
IRAM_ATTR void ST7305_DMA::drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (h <= 0)
        return;
    int16_t col = x >> 1;
    uint8_t x_mod2 = x & 1;
    int16_t y_start = y;
    int16_t y_end = y + h - 1;
    int16_t start_rg = y_start >> 2;
    int16_t end_rg = y_end >> 2;
    uint8_t start_off = y_start & 3;
    uint8_t end_off = y_end & 3;
    uint8_t full_mask = (x_mod2 == 0) ? 0xAA : 0x55; // 偶列/奇列整行组掩码

    for (int16_t rg = start_rg; rg <= end_rg; rg++)
    {
        uint16_t idx = col * TOTAL_ROWS + rg;
        if (rg == start_rg && rg == end_rg)
        { // 同一行组
            uint8_t mask = 0;
            for (uint8_t off = start_off; off <= end_off; off++)
                mask |= BIT_MASK_LUT[Y_BYTE_OFFSET[off] + x_mod2];
            if (color)
                buffer[idx] |= mask;
            else
                buffer[idx] &= ~mask;
        }
        else if (rg == start_rg)
        { // 起始不完整行组
            uint8_t mask = 0;
            for (uint8_t off = start_off; off < 4; off++)
                mask |= BIT_MASK_LUT[Y_BYTE_OFFSET[off] + x_mod2];
            if (color)
                buffer[idx] |= mask;
            else
                buffer[idx] &= ~mask;
        }
        else if (rg == end_rg)
        { // 结束不完整行组
            uint8_t mask = 0;
            for (uint8_t off = 0; off <= end_off; off++)
                mask |= BIT_MASK_LUT[Y_BYTE_OFFSET[off] + x_mod2];
            if (color)
                buffer[idx] |= mask;
            else
                buffer[idx] &= ~mask;
        }
        else
        { // 完整行组
            if (color)
                buffer[idx] |= full_mask;
            else
                buffer[idx] &= ~full_mask;
        }
    }
}

// 内部函数：画水平线（物理坐标，已裁剪）
IRAM_ATTR void ST7305_DMA::drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (w <= 0)
        return;
    int16_t rg = y >> 2;
    uint8_t y_off = y & 3;
    int16_t x_start = x;
    int16_t x_end = x + w - 1;
    int16_t start_col = x_start >> 1;
    int16_t end_col = x_end >> 1;
    uint8_t start_bit = x_start & 1;
    uint8_t end_bit = x_end & 1;
    uint8_t two_bit = BIT_MASK_LUT[Y_BYTE_OFFSET[y_off]] | BIT_MASK_LUT[Y_BYTE_OFFSET[y_off] + 1];

    if (start_col == end_col)
    {
        uint16_t idx = start_col * TOTAL_ROWS + rg;
        uint8_t mask;
        if (start_bit == 0 && end_bit == 1)
            mask = two_bit;
        else if (start_bit == 0)
            mask = BIT_MASK_LUT[Y_BYTE_OFFSET[y_off]];
        else
            mask = BIT_MASK_LUT[Y_BYTE_OFFSET[y_off] + 1];
        if (color)
            buffer[idx] |= mask;
        else
            buffer[idx] &= ~mask;
    }
    else
    {
        // 起始列组
        uint16_t idx_start = start_col * TOTAL_ROWS + rg;
        uint8_t mask_start = (start_bit == 0) ? two_bit : BIT_MASK_LUT[Y_BYTE_OFFSET[y_off] + 1];
        if (color)
            buffer[idx_start] |= mask_start;
        else
            buffer[idx_start] &= ~mask_start;

        // 中间完整列组
        for (int16_t col = start_col + 1; col < end_col; col++)
        {
            uint16_t idx = col * TOTAL_ROWS + rg;
            if (color)
                buffer[idx] |= two_bit;
            else
                buffer[idx] &= ~two_bit;
        }

        // 结束列组
        if (end_col > start_col)
        {
            uint16_t idx_end = end_col * TOTAL_ROWS + rg;
            uint8_t mask_end = (end_bit == 1) ? two_bit : BIT_MASK_LUT[Y_BYTE_OFFSET[y_off]];
            if (color)
                buffer[idx_end] |= mask_end;
            else
                buffer[idx_end] &= ~mask_end;
        }
    }
}

// 公共函数：画垂直线（原始坐标）
IRAM_ATTR void ST7305_DMA::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (x < x_min || x > x_max)
        return;
    int16_t y0 = y, y1 = y + h - 1;
    if (y0 > y_max || y1 < y_min)
        return;
    int16_t ys = gx_uint16_max(y0, y_min);
    int16_t ye = gx_uint16_min(y1, y_max);
    if (ys > ye)
        return;
    int16_t new_h = ye - ys + 1;

    int16_t x0, y0p, x1, y1p;
    // 变换起点
    switch (rotation)
    {
    case 1:
        x0 = x;
        y0p = ys;
        break;
    case 2:
        x0 = PHYSICAL_WIDTH - ys - 1;
        y0p = x;
        break;
    case 3:
        x0 = PHYSICAL_WIDTH - x - 1;
        y0p = PHYSICAL_HEIGHT - ys - 1;
        break;
    default:
        x0 = ys;
        y0p = PHYSICAL_HEIGHT - x - 1;
        break;
    }
    // 变换终点
    switch (rotation)
    {
    case 1:
        x1 = x;
        y1p = ye;
        break;
    case 2:
        x1 = PHYSICAL_WIDTH - ye - 1;
        y1p = x;
        break;
    case 3:
        x1 = PHYSICAL_WIDTH - x - 1;
        y1p = PHYSICAL_HEIGHT - ye - 1;
        break;
    default:
        x1 = ye;
        y1p = PHYSICAL_HEIGHT - x - 1;
        break;
    }

    if (x0 == x1)
    { // 物理垂直线
        if (x0 < 0 || x0 >= PHYSICAL_WIDTH)
            return;
        int16_t yps = min(y0p, y1p), ype = max(y0p, y1p);
        if (yps > PHYSICAL_HEIGHT - 1 || ype < 0)
            return;
        yps = gx_uint16_max(yps, 0);
        ype = gx_uint16_min(ype, PHYSICAL_HEIGHT - 1);
        int16_t ph = ype - yps + 1;
        if (ph > 0)
            drawFastVLineInternal(x0, yps, ph, color);
    }
    else if (y0p == y1p)
    { // 物理水平线
        if (y0p < 0 || y0p >= PHYSICAL_HEIGHT)
            return;
        int16_t xps = min(x0, x1), xpe = max(x0, x1);
        if (xps > PHYSICAL_WIDTH - 1 || xpe < 0)
            return;
        xps = gx_uint16_max(xps, 0);
        xpe = gx_uint16_min(xpe, PHYSICAL_WIDTH - 1);
        int16_t pw = xpe - xps + 1;
        if (pw > 0)
            drawFastHLineInternal(xps, y0p, pw, color);
    }
}

// 公共函数：画水平线（原始坐标）
IRAM_ATTR void ST7305_DMA::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (y < y_min || y > y_max)
        return;
    int16_t x0 = x, x1 = x + w - 1;
    if (x0 > x_max || x1 < x_min)
        return;
    int16_t xs = gx_uint16_max(x0, x_min);
    int16_t xe = gx_uint16_min(x1, x_max);
    if (xs > xe)
        return;
    int16_t new_w = xe - xs + 1;

    int16_t x0p, y0p, x1p, y1p;
    // 变换起点
    switch (rotation)
    {
    case 1:
        x0p = xs;
        y0p = y;
        break;
    case 2:
        x0p = PHYSICAL_WIDTH - y - 1;
        y0p = xs;
        break;
    case 3:
        x0p = PHYSICAL_WIDTH - xs - 1;
        y0p = PHYSICAL_HEIGHT - y - 1;
        break;
    default:
        x0p = y;
        y0p = PHYSICAL_HEIGHT - xs - 1;
        break;
    }
    // 变换终点
    switch (rotation)
    {
    case 1:
        x1p = xe;
        y1p = y;
        break;
    case 2:
        x1p = PHYSICAL_WIDTH - y - 1;
        y1p = xe;
        break;
    case 3:
        x1p = PHYSICAL_WIDTH - xe - 1;
        y1p = PHYSICAL_HEIGHT - y - 1;
        break;
    default:
        x1p = y;
        y1p = PHYSICAL_HEIGHT - xe - 1;
        break;
    }

    if (x0p == x1p)
    { // 物理垂直线
        if (x0p < 0 || x0p >= PHYSICAL_WIDTH)
            return;
        int16_t yps = gx_uint16_min(y0p, y1p), ype = gx_uint16_max(y0p, y1p);
        if (yps > PHYSICAL_HEIGHT - 1 || ype < 0)
            return;
        yps = gx_uint16_max(yps, 0);
        ype = gx_uint16_min(ype, PHYSICAL_HEIGHT - 1);
        int16_t ph = ype - yps + 1;
        if (ph > 0)
            drawFastVLineInternal(x0p, yps, ph, color);
    }
    else if (y0p == y1p)
    { // 物理水平线
        if (y0p < 0 || y0p >= PHYSICAL_HEIGHT)
            return;
        int16_t xps = gx_uint16_min(x0p, x1p), xpe = gx_uint16_max(x0p, x1p);
        if (xps > PHYSICAL_WIDTH - 1 || xpe < 0)
            return;
        xps = gx_uint16_max(xps, 0);
        xpe = gx_uint16_min(xpe, PHYSICAL_WIDTH - 1);
        int16_t pw = xpe - xps + 1;
        if (pw > 0)
            drawFastHLineInternal(xps, y0p, pw, color);
    }
}

void ST7305_DMA::setRotation(uint8_t m)
{
    rotation = m % 4; // Ensure rotation is within 0-3
}

void ST7305_DMA::setvoltage(ST7305_DMA_voltage_t fps)
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

void ST7305_DMA::setDrawWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    x_min = x;
    x_max = x + w;
    y_min = y;
    y_max = y + h;
}

void ST7305_DMA::setPowerMode(PowerMode mode)
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

void ST7305_DMA::display_on(bool enabled)
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

void ST7305_DMA::display_sleep(bool enabled)
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

void ST7305_DMA::display_Inversion(bool enabled)
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