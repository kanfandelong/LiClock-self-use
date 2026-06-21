#include "AppManager.h"
#include <driver/i2s.h>

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

    // ────────── 2. PDM 麦克风：I2S0 接收 ──────────
    i2s_config_t i2s_rx_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate          = 48000,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 256,
        .use_apll             = false,
    };

    i2s_pin_config_t mic_pins = {
        .mck_io_num   = I2S_PIN_NO_CHANGE,
        .bck_io_num   = I2S_PIN_NO_CHANGE,
        .ws_io_num    = PIN_MIC_CLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = PIN_MIC_DATA,
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_rx_config, 0, NULL);
    if (err != ESP_OK) {
        log_e("I2S0 驱动安装失败");
        return;
    }
    i2s_set_pin(I2S_NUM_0, &mic_pins);
    i2s_set_pdm_rx_down_sample(I2S_NUM_0, I2S_PDM_DSR_8S);

    // ────────── 3. I2S DAC：I2S1 发送 ──────────
    i2s_config_t i2s_tx_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = 48000,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 4,
        .dma_buf_len          = 256,
        .use_apll             = false,
        .bits_per_chan        = I2S_BITS_PER_CHAN_32BIT,
    };

    i2s_pin_config_t dac_pins = {
        .mck_io_num   = PIN_I2S_MCLK,
        .bck_io_num   = PIN_I2S_BCLK,
        .ws_io_num    = PIN_I2S_LRCK,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };

    err = i2s_driver_install(I2S_NUM_1, &i2s_tx_config, 0, NULL);
    if (err != ESP_OK) {
        log_e("I2S1 驱动安装失败");
        return;
    }
    i2s_set_pin(I2S_NUM_1, &dac_pins);

    // ────────── 注册按键回调 ──────────
    hal.btnl.attachClick(onBtnL_Click, this);
    hal.btnl.attachLongPressStart(onBtnL_LongPressStart, this);
    hal.btnr.attachClick(onBtnR_Click, this);
    hal.btnr.attachLongPressStart(onBtnR_LongPressStart, this);
    // 注意：中间键未使用，如需也可注册

    log_i("麦克风 + DAC 已启动，实时监听中...");

    // ────────── 4. 音频环回主循环（含音量和退出控制）──────────
    const int buffer_samples = 256;
    int16_t rx_buffer[buffer_samples];
    int32_t tx_buffer[buffer_samples * 2];
    size_t bytes_read, bytes_written;

    while (true) {
        // 检查按键标志
        if (flag_btnl_click) {
            flag_btnl_click = false;
            // 降低音量
            _gain -= _volume_step;
            if (_gain < _gain_min) _gain = _gain_min;
            log_i("音量降低，当前增益：%.2f", _gain);
        }

        if (flag_btnr_click) {
            flag_btnr_click = false;
            // 增加音量
            _gain += _volume_step;
            if (_gain > _gain_max) _gain = _gain_max;
            log_i("音量增加，当前增益：%.2f", _gain);
        }

        if (flag_btnl_long) {
            flag_btnl_long = false;
            log_i("长按左键，退出麦克风");
            break;  // 跳出循环，准备退出
        }

        // 读取麦克风数据
        err = i2s_read(I2S_NUM_0, rx_buffer, sizeof(rx_buffer), &bytes_read, portMAX_DELAY);
        if (err == ESP_OK) {
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
            i2s_write(I2S_NUM_1, tx_buffer, samples * 4 * 2, &bytes_written, portMAX_DELAY);
        }
        delay(1); // 防止看门狗
    }

    // ────────── 5. 退出清理 ──────────
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_1);

    // 关闭外设电源（可选，根据实际硬件）
    digitalWrite(PIN_DAC_XSMT, LOW);
    digitalWrite(PIN_DAC_EN, LOW);
    digitalWrite(PIN_MIC_VCC, LOW);

    // 返回上层应用
    appManager.goBack();
}