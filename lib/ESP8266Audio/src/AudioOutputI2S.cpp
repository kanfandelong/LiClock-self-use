#include "AudioOutputI2S.h"

static const int DMA_FRAME_NUM = 256;

AudioOutputI2S::AudioOutputI2S(int port, int dma_buf_count)
    : portNo(port), dma_buf_count(dma_buf_count)
{
    mono = false;
    lsb_justified = false;
    use_mclk = false;
    swap_clocks = false;
#ifdef CONFIG_DAC_32bit
    bps = 32;                           // 基类成员
    bits_per_chan = I2S_SLOT_BIT_WIDTH_32BIT;
    bits_data = I2S_DATA_BIT_WIDTH_32BIT;
#else
    bps = 16;
    bits_per_chan = I2S_SLOT_BIT_WIDTH_16BIT;
    bits_data = I2S_DATA_BIT_WIDTH_16BIT;
#endif

    channels = 2;                        // 基类成员
    hertz = 44100;                       // 基类成员
    bclkPin = 26;
    wclkPin = 25;
    doutPin = 21;
    mclkPin = 0;
    timeout_ms = 100;                           // 100ms 超时
    tx_handle = nullptr;
    playedSampleFrames = 0; 
    SetGain(1.0f);
}

AudioOutputI2S::~AudioOutputI2S()
{
    stop();
}

bool AudioOutputI2S::SetPinout(int bclk, int wclk, int dout)
{
    bclkPin = bclk;
    wclkPin = wclk;
    doutPin = dout;

    if (tx_handle) {
        if (i2s_channel_disable(tx_handle) != ESP_OK) return false;

        i2s_std_gpio_config_t gpio_cfg = {};
        gpio_cfg.mclk = use_mclk ? (gpio_num_t)mclkPin : I2S_GPIO_UNUSED;
        gpio_cfg.bclk = swap_clocks ? (gpio_num_t)wclkPin : (gpio_num_t)bclkPin;
        gpio_cfg.ws   = swap_clocks ? (gpio_num_t)bclkPin : (gpio_num_t)wclkPin;
        gpio_cfg.dout = (gpio_num_t)doutPin;
        gpio_cfg.din  = I2S_GPIO_UNUSED;
        gpio_cfg.invert_flags.mclk_inv = false;
        gpio_cfg.invert_flags.bclk_inv = false;
        gpio_cfg.invert_flags.ws_inv   = false;

        if (i2s_channel_reconfig_std_gpio(tx_handle, &gpio_cfg) != ESP_OK) {
            i2s_channel_enable(tx_handle);
            return false;
        }
        return i2s_channel_enable(tx_handle) == ESP_OK;
    }
    return true;
}

bool AudioOutputI2S::SetPinout(int bclk, int wclk, int dout, int mclk)
{
    mclkPin = mclk;
    return SetPinout(bclk, wclk, dout);
}

bool AudioOutputI2S::SetRate(int hz)
{
    this->hertz = hz;
    if (tx_handle) {
        if (i2s_channel_disable(tx_handle) != ESP_OK) return false;
        i2s_std_clk_config_t clk_cfg = {
            .sample_rate_hz = (uint32_t)AdjustI2SRate(hz),
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        };
        if (i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg) != ESP_OK) {
            i2s_channel_enable(tx_handle);
            return false;
        }
        return i2s_channel_enable(tx_handle) == ESP_OK;
    }
    return true;
}

bool AudioOutputI2S::SetBitsPerSample(int bits)
{
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32) return false;
    this->bps = bits;
    return true;
}

bool AudioOutputI2S::SetChannels(int channels)
{
    if (channels < 1 || channels > 2) return false;
    this->channels = channels;
    return true;
}

bool AudioOutputI2S::SetOutputModeMono(bool mono)
{
    this->mono = mono;
    return true;
}

bool AudioOutputI2S::SetLsbJustified(bool lsbJustified)
{
    this->lsb_justified = lsbJustified;

    if (tx_handle) {
        if (i2s_channel_disable(tx_handle) != ESP_OK) return false;

        // 构造 slot 配置（同上）
        i2s_slot_bit_width_t slot_width;
        if (bits_per_chan == I2S_SLOT_BIT_WIDTH_AUTO) {
            switch (bps) {
                case 8:  slot_width = I2S_SLOT_BIT_WIDTH_8BIT;  break;
                case 16: slot_width = I2S_SLOT_BIT_WIDTH_16BIT; break;
                case 24: slot_width = I2S_SLOT_BIT_WIDTH_24BIT; break;
                case 32: slot_width = I2S_SLOT_BIT_WIDTH_32BIT; break;
                default: slot_width = I2S_SLOT_BIT_WIDTH_16BIT; break;
            }
        } else {
            slot_width = bits_per_chan;
        }

        i2s_std_slot_config_t slot_cfg = {};
        slot_cfg.data_bit_width = bits_data;
        slot_cfg.slot_bit_width = slot_width;
        slot_cfg.slot_mode = (channels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
        slot_cfg.slot_mask = (channels == 1) ? I2S_STD_SLOT_LEFT : I2S_STD_SLOT_BOTH;
        slot_cfg.ws_width = slot_width;
        slot_cfg.ws_pol = false;
        slot_cfg.bit_shift = !lsb_justified;
        slot_cfg.left_align = lsb_justified;
        slot_cfg.big_endian = false;
        slot_cfg.bit_order_lsb = false;

        if (i2s_channel_reconfig_std_slot(tx_handle, &slot_cfg) != ESP_OK) {
            i2s_channel_enable(tx_handle);
            return false;
        }
        return i2s_channel_enable(tx_handle) == ESP_OK;
    }
    return true;
}

bool AudioOutputI2S::SetMclk(bool enabled)
{
    use_mclk = enabled;

    if (tx_handle) {
        if (i2s_channel_disable(tx_handle) != ESP_OK) return false;

        i2s_std_gpio_config_t gpio_cfg = {};
        gpio_cfg.mclk = use_mclk ? (gpio_num_t)mclkPin : I2S_GPIO_UNUSED;
        gpio_cfg.bclk = swap_clocks ? (gpio_num_t)wclkPin : (gpio_num_t)bclkPin;
        gpio_cfg.ws   = swap_clocks ? (gpio_num_t)bclkPin : (gpio_num_t)wclkPin;
        gpio_cfg.dout = (gpio_num_t)doutPin;
        gpio_cfg.din  = I2S_GPIO_UNUSED;
        gpio_cfg.invert_flags.mclk_inv = false;
        gpio_cfg.invert_flags.bclk_inv = false;
        gpio_cfg.invert_flags.ws_inv   = false;

        if (i2s_channel_reconfig_std_gpio(tx_handle, &gpio_cfg) != ESP_OK) {
            i2s_channel_enable(tx_handle);
            return false;
        }
        return i2s_channel_enable(tx_handle) == ESP_OK;
    }
    return true;
}

bool AudioOutputI2S::SetBitsPerChan(i2s_slot_bit_width_t bitsPerChan)
{
    bits_per_chan = bitsPerChan;
    return true;
}

bool AudioOutputI2S::SetBitsdata(i2s_data_bit_width_t data_bit_width)
{
    bits_data = data_bit_width;
    return true;
}

bool AudioOutputI2S::set_ConsumeSample_CB(SampleCB fn)
{
    if (fn) {
        ConsumeSampleCB = fn;
        return true;
    }
    return false;
}

bool AudioOutputI2S::SwapClocks(bool swap_clocks)
{
    if (tx_handle) {
        if (i2s_channel_disable(tx_handle) != ESP_OK) return false;
        this->swap_clocks = swap_clocks;

        i2s_std_gpio_config_t gpio_cfg = {};
        gpio_cfg.mclk = use_mclk ? (gpio_num_t)mclkPin : I2S_GPIO_UNUSED;
        gpio_cfg.bclk = swap_clocks ? (gpio_num_t)wclkPin : (gpio_num_t)bclkPin;
        gpio_cfg.ws   = swap_clocks ? (gpio_num_t)bclkPin : (gpio_num_t)wclkPin;
        gpio_cfg.dout = (gpio_num_t)doutPin;
        gpio_cfg.din  = I2S_GPIO_UNUSED;
        gpio_cfg.invert_flags.mclk_inv = false;
        gpio_cfg.invert_flags.bclk_inv = false;
        gpio_cfg.invert_flags.ws_inv   = false;

        if (i2s_channel_reconfig_std_gpio(tx_handle, &gpio_cfg) != ESP_OK) {
            i2s_channel_enable(tx_handle);
            return false;
        }
        return i2s_channel_enable(tx_handle) == ESP_OK;
    }
    this->swap_clocks = swap_clocks;
    return true;
}

bool AudioOutputI2S::SetTimeout(uint32_t _timeout_ms)
{
    timeout_ms = _timeout_ms;
    return true;
}

bool AudioOutputI2S::begin()
{
    if (tx_handle) return false;

    // ---- 1. 创建 TX 通道 ----
    i2s_chan_config_t chan_cfg = {
        .id = (i2s_port_t)portNo,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = (uint32_t)dma_buf_count,
        .dma_frame_num = (uint32_t)DMA_FRAME_NUM,
        .auto_clear = true,
    };
    if (i2s_new_channel(&chan_cfg, &tx_handle, nullptr) != ESP_OK)
        return false;

    // ---- 2. 配置标准模式 ----
    i2s_std_config_t std_cfg = {};

    // 确定槽位宽度
    i2s_slot_bit_width_t slot_width;
    if (bits_per_chan == I2S_SLOT_BIT_WIDTH_AUTO) {
        switch (bps) {
            case 8:  slot_width = I2S_SLOT_BIT_WIDTH_8BIT;  break;
            case 16: slot_width = I2S_SLOT_BIT_WIDTH_16BIT; break;
            case 24: slot_width = I2S_SLOT_BIT_WIDTH_24BIT; break;
            case 32: slot_width = I2S_SLOT_BIT_WIDTH_32BIT; break;
            default: slot_width = I2S_SLOT_BIT_WIDTH_16BIT; break;
        }
    } else {
        slot_width = bits_per_chan;
    }

    // 槽位配置
    std_cfg.slot_cfg.data_bit_width = bits_data;
    std_cfg.slot_cfg.slot_bit_width = slot_width;
    std_cfg.slot_cfg.slot_mode    = (channels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;
    std_cfg.slot_cfg.slot_mask    = (channels == 1) ? I2S_STD_SLOT_LEFT : I2S_STD_SLOT_BOTH;
    std_cfg.slot_cfg.ws_width     = slot_width;               // WS 宽度 = 槽位宽度
    std_cfg.slot_cfg.ws_pol       = false;
    std_cfg.slot_cfg.bit_shift    = !lsb_justified;          // 标准 I2S 有位移，左对齐无位移
    std_cfg.slot_cfg.left_align   = lsb_justified;           // 左对齐时启用
    std_cfg.slot_cfg.big_endian   = false;
    std_cfg.slot_cfg.bit_order_lsb = false;                  // MSB 先发

    // 时钟配置
    std_cfg.clk_cfg.sample_rate_hz = (uint32_t)AdjustI2SRate(hertz);
    std_cfg.clk_cfg.clk_src        = I2S_CLK_SRC_PLL_240M;
    std_cfg.clk_cfg.mclk_multiple  = I2S_MCLK_MULTIPLE_256;

    // GPIO 配置
    std_cfg.gpio_cfg.mclk = use_mclk ? (gpio_num_t)mclkPin : I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = swap_clocks ? (gpio_num_t)wclkPin : (gpio_num_t)bclkPin;
    std_cfg.gpio_cfg.ws   = swap_clocks ? (gpio_num_t)bclkPin : (gpio_num_t)wclkPin;
    std_cfg.gpio_cfg.dout = (gpio_num_t)doutPin;
    std_cfg.gpio_cfg.din  = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    if (i2s_channel_init_std_mode(tx_handle, &std_cfg) != ESP_OK) {
        i2s_del_channel(tx_handle);
        tx_handle = nullptr;
        return false;
    }

    // ---- 3. 启用通道 ----
    if (i2s_channel_enable(tx_handle) != ESP_OK) {
        i2s_del_channel(tx_handle);
        tx_handle = nullptr;
        return false;
    }

    return true;
}

#ifdef CONFIG_DAC_32bit
bool AudioOutputI2S::ConsumeSample(int32_t sample[2])
{
    if (!tx_handle) return false;

    int32_t ms[2] = { sample[0], sample[1] };
    MakeSampleStereo16(ms);   // 基类可能已重载支持 int32_t*

    if (ConsumeSampleCB)
        ConsumeSampleCB(ms);

    if (mono) {
        int32_t ttl = ms[LEFTCHANNEL] + ms[RIGHTCHANNEL];
        ms[LEFTCHANNEL] = ms[RIGHTCHANNEL] = (ttl >> 1);
    }

    // 32 位模式下，每个声道写 32 位数据，直接组合为 64 位发送？原逻辑是将两个 32 位拼成 64 位写入？
    // 原代码使用 i2s_write 并传入 int samples_data[2]，每个 32 位，共 8 字节。
    // 新 API 可一次写入 8 字节。
    int32_t samples_data[2];
    samples_data[0] = Amplify(ms[RIGHTCHANNEL]);
    samples_data[1] = Amplify(ms[LEFTCHANNEL]);

    size_t bytes_written;
    esp_err_t err = i2s_channel_write(tx_handle, samples_data, sizeof(int32_t) * 2, &bytes_written, timeout_ms);
    if (err == ESP_OK && bytes_written == sizeof(int32_t) * 2) {
        playedSampleFrames++;
        return true;
    }
    return false;
}
#else
bool AudioOutputI2S::ConsumeSample(int16_t sample[2])
{
    if (!tx_handle) return false;

    int16_t ms[2] = { sample[0], sample[1] };
    MakeSampleStereo16(ms);

    if (ConsumeSampleCB)
        ConsumeSampleCB(ms);

    if (mono) {
        int32_t ttl = ms[LEFTCHANNEL] + ms[RIGHTCHANNEL];
        ms[LEFTCHANNEL] = ms[RIGHTCHANNEL] = (ttl >> 1) & 0xffff;
    }

    // 16 位模式：右声道高 16 位，左声道低 16 位，组合为 32 位写入
    uint32_t s32 = ((uint16_t)Amplify(ms[RIGHTCHANNEL]) << 16) |
                   ((uint16_t)Amplify(ms[LEFTCHANNEL]) & 0xffff);

    size_t bytes_written;
    esp_err_t err = i2s_channel_write(tx_handle, &s32, sizeof(s32), &bytes_written, timeout_ms);
    if (err == ESP_OK && bytes_written == sizeof(s32)) {
        playedSampleFrames++;
        return true;
    }
    return false;
}
#endif

void AudioOutputI2S::flush()
{
    if (!tx_handle) return;

    int total_frames = dma_buf_count * DMA_FRAME_NUM;
#ifdef CONFIG_DAC_32bit
    int32_t silence[2] = {0};
    for (int i = 0; i < total_frames; i++) {
        size_t written;
        i2s_channel_write(tx_handle, silence, sizeof(silence), &written, 1000);
    }
#else
    uint32_t silence = 0;
    for (int i = 0; i < total_frames; i++) {
        size_t written;
        i2s_channel_write(tx_handle, &silence, sizeof(silence), &written, 1000);
    }
#endif
}

bool AudioOutputI2S::stop()
{
    if (!tx_handle) return false;

    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = nullptr;
    return true;
}