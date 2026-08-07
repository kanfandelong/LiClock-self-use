#include "AppManager.h"
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"

class AppMicrophone : public AppBase
{
public:
    AppMicrophone()
    {
        name        = "microphone";
        title       = "麦克风";
        description = "PDM 数字麦克风示例";
        image       = NULL;
    }
    void set();
    void setup();
private:
    // 按键回调（静态）
    static void onBtnL_Click(void *param);
    static void onBtnL_LongPressStart(void *param);
    static void onBtnR_Click(void *param);
    static void onBtnR_LongPressStart(void *param);

    // 标志位
    volatile bool flag_btnl_click   = false;
    volatile bool flag_btnl_long    = false;
    volatile bool flag_btnr_click   = false;
    volatile bool flag_btnr_long    = false;

    // 音量控制
    float _gain          = 1.0f;          // 线性增益，1.0 为原始音量
    float _volume_step   = 0.1f;          // 每次按键调整步长（线性）
    float _gain_min      = 0.0f;
    float _gain_max      = 10.0f;          // 最大允许增益，防止严重削波
};

static AppMicrophone app;

// ──────────────────── 按键回调实现 ────────────────────
void AppMicrophone::onBtnL_Click(void *param)
{
    AppMicrophone *self = static_cast<AppMicrophone *>(param);
    self->flag_btnl_click = true;
}

void AppMicrophone::onBtnL_LongPressStart(void *param)
{
    AppMicrophone *self = static_cast<AppMicrophone *>(param);
    self->flag_btnl_long = true;
}

void AppMicrophone::onBtnR_Click(void *param)
{
    AppMicrophone *self = static_cast<AppMicrophone *>(param);
    self->flag_btnr_click = true;
}

void AppMicrophone::onBtnR_LongPressStart(void *param)
{
    AppMicrophone *self = static_cast<AppMicrophone *>(param);
    self->flag_btnr_long = true;
}

void AppMicrophone::set()
{
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

void AppMicrophone::setup()
{
    // ────────── 1. 供电并使能麦克风和 DAC ──────────
    pinMode(PIN_MIC_VCC, OUTPUT);
    digitalWrite(PIN_MIC_VCC, HIGH);
    delay(100);

    pinMode(PIN_DAC_EN, OUTPUT);
    digitalWrite(PIN_DAC_EN, HIGH);
    pinMode(PIN_DAC_FMT, OUTPUT);
    digitalWrite(PIN_DAC_FMT, LOW);
    pinMode(PIN_DAC_XSMT, OUTPUT);
    digitalWrite(PIN_DAC_XSMT, HIGH);
    delay(10);

    // ────────── 2. 【新 API】I2S0 RX：PDM 数字麦克风 ──────────
    i2s_chan_handle_t rx_handle = NULL;
    
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(48000), // 48kHz 采样率
        .slot_cfg = I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = PIN_MIC_CLK,
            .din = PIN_MIC_DATA,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    // ────────── 3. 【新 API】I2S1 TX：外置 DAC ──────────
    i2s_chan_handle_t tx_handle = NULL;

    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_handle, NULL));

    i2s_std_config_t std_tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000), 
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)PIN_I2S_MCLK,
            .bclk = (gpio_num_t)PIN_I2S_BCLK,
            .ws   = (gpio_num_t)PIN_I2S_LRCK,
            .dout = (gpio_num_t)PIN_I2S_DOUT,
            .din  = (gpio_num_t)I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_tx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    // ────────── 注册按键回调 ──────────
    hal.btnl.attachClick(onBtnL_Click, this);
    hal.btnl.attachLongPressStart(onBtnL_LongPressStart, this);
    hal.btnr.attachClick(onBtnR_Click, this);
    hal.btnr.attachLongPressStart(onBtnR_LongPressStart, this);

    log_i("麦克风 + DAC 已启动，实时监听中...");

    // ────────── 4. 音频环回主循环 ──────────
    const int buffer_samples = 256;
    int16_t rx_buffer[buffer_samples];
    int32_t tx_buffer[buffer_samples * 2];
    size_t bytes_read = 0, bytes_written = 0;

    while (true) {
        // 检查按键标志
        if (flag_btnl_click) {
            flag_btnl_click = false;
            _gain -= _volume_step;
            if (_gain < _gain_min) _gain = _gain_min;
            log_i("音量降低，当前增益：%.2f", _gain);
        }

        if (flag_btnr_click) {
            flag_btnr_click = false;
            _gain += _volume_step;
            if (_gain > _gain_max) _gain = _gain_max;
            log_i("音量增加，当前增益：%.2f", _gain);
        }

        if (flag_btnl_long) {
            flag_btnl_long = false;
            log_i("长按左键，退出麦克风");
            break;  // 退出循环
        }

        // ★★★ 关键修改：改用 i2s_channel_read 读取数据 ★★★
        // 这里的 1000 是超时时间(ms)。如果你没吹气，麦克风也会输出底噪，bytes_read 不会一直为0
        esp_err_t err = i2s_channel_read(rx_handle, rx_buffer, sizeof(rx_buffer), &bytes_read, 1000);
        
        if (err == ESP_OK && bytes_read > 0) {
            int samples = bytes_read / sizeof(int16_t);
            // 应用增益并复制为立体声
            for (int i = 0; i < samples; i++) {
                // 线性增益 + 硬限幅
                float sample = (float)rx_buffer[i] * _gain;
                if (sample > 32767.0f)  sample = 32767.0f;
                if (sample < -32768.0f) sample = -32768.0f;
                int32_t scaled = (int32_t)sample;

                tx_buffer[i * 2]     = scaled << 16;   // 左声道
                tx_buffer[i * 2 + 1] = scaled << 16;   // 右声道
            }
            
            // ★★★ 关键修改：改用 i2s_channel_write 写入数据 ★★★
            err = i2s_channel_write(tx_handle, tx_buffer, samples * 4 * 2, &bytes_written, 1000);
            if (err != ESP_OK) {
                log_e("写入超时");
            } else {
                // 打印当前实际读取的第一个样本，会随着声音实时变化，不再是 -23131
                // log_i("样本: %d", rx_buffer[0]); 
            }
        } else {
            // 如果bytes_read始终为0，说明麦克风硬件时钟没有收到数据
            log_e("读取超时或无数据，bytes_read = %d", bytes_read);
        }
        delay(1); // 防止看门狗
    }

    // ────────── 5. 退出清理（新 API 专用） ──────────
    i2s_channel_disable(rx_handle);
    i2s_channel_disable(tx_handle);
    i2s_del_channel(rx_handle);
    i2s_del_channel(tx_handle);

    // 关闭外设电源
    digitalWrite(PIN_DAC_XSMT, LOW);
    digitalWrite(PIN_DAC_EN, LOW);
    digitalWrite(PIN_MIC_VCC, LOW);

    // 返回上层应用
    appManager.goBack();
}