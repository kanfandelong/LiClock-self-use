#include "AppManager.h"
#include "USB.h"
#include "USBAudioCard.h"
#include "ESP8266Audio.h"
#include "esp_dsp.h"

// ==================== 应用类 ====================
class USBAudioSpeakerApp : public AppBase
{
private:
    /* data */
public:
    USBAudioSpeakerApp()
    {
        name = "usb_audio_speaker";       // 内部标识符
        title = "USB 声卡";         // 显示标题
        description = "仅播放（扬声器）"; // 简要描述
        image = NULL;
    }
    float calculateAutoGain(float *spectrum, int len);
    void show_display_fft();
    static void onBtnL_Click(void *param);
    void setup();
    USBAudioCard *uac = nullptr;
    AudioOutputI2S *i2s_output = nullptr;
    static const size_t RING_BUFFER_SIZE = 8192;
    uint16_t SAMPLES = 256;
    float smoothingFactor = 0.7f; // 平滑控制, 0~1，越大越平滑
    float fft_gain = 1.0;
    float *ring_buffer = nullptr;
    uint32_t write_index = 0;
    float *curveScaling;
    float *vReal;
    float *vImag;
    float *previousSpectrum;
    float *wind;     // 窗系数
    float *fft_data; // 复数交叠数组（实部/虚部交替）
    bool flag_btnl_click = false;
};
static USBAudioSpeakerApp app;

// ==================== 回调函数 ====================
static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // if (event_base == ARDUINO_USB_EVENTS)
    // {
    //     arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
    //     switch (event_id)
    //     {
    //     case ARDUINO_USB_STARTED_EVENT:
    //         log_i("USB PLUGGED");
    //         break;
    //     case ARDUINO_USB_STOPPED_EVENT:
    //         log_i("USB UNPLUGGED");
    //         break;
    //     case ARDUINO_USB_SUSPEND_EVENT:
    //         log_i("USB SUSPENDED: remote_wakeup_en: %u", data->suspend.remote_wakeup_en);
    //         break;
    //     case ARDUINO_USB_RESUME_EVENT:
    //         log_i("USB RESUMED");
    //         break;
    //     default:
    //         break;
    //     }
    // }
    // else if (event_base == ARDUINO_USB_AUDIO_CARD_EVENTS)
    // {
    //     arduino_usb_audio_card_event_data_t *data = (arduino_usb_audio_card_event_data_t *)event_data;
    //     switch (event_id)
    //     {
    //     case ARDUINO_USB_AUDIO_CARD_VOLUME_EVENT:
    //         log_i("VOLUME CH:%d DB:%d", data->volume.channel, data->volume.db);
    //         break;
    //     case ARDUINO_USB_AUDIO_CARD_MUTE_EVENT:
    //         log_i("MUTE CH:%d MUTED:%d", data->mute.channel, data->mute.muted);
    //         break;
    //     case ARDUINO_USB_AUDIO_CARD_SAMPLE_RATE_EVENT:
    //         log_i("SAMPLE RATE:%lu", data->sample_rate.rate);
    //         break;
    //     case ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT:
    //         log_i("INTERFACE %s ENABLED:%d", data->interface_enable.interface ? "MIC" : "SPK", data->interface_enable.enable);
    //         break;
    //     default:
    //         break;
    //     }
    // }
}

static void onSpkData(void *data, uint16_t len)
{
    if (app.uac)
    {
        int16_t *buffer = (int16_t *)data;
        int32_t sample[2];
        int sample_count = len / sizeof(int16_t); // 总样本数
        for (int i = 0; i < sample_count; i += 2) // 每次取两个样本（左右各一）
        {
            float left = (float)buffer[i];
            float right = (float)buffer[i + 1];
            // 混合
            float mono = (left + right) * 0.5f;
            app.ring_buffer[app.write_index] = mono;
            app.write_index = (app.write_index + 1) & (app.RING_BUFFER_SIZE - 1);
        }
        app.uac->applyVolume(data, len);
        for (int i = 0; i < sample_count; i += 2) // 每次取两个样本（左右各一）
        {
            sample[0] = (int32_t)buffer[i] << 16;
            sample[1] = (int32_t)buffer[i + 1] << 16;

            app.i2s_output->ConsumeSample(sample);
        }
    }
}

static void initCurveScaling()
{
    const int lowFreqCount = hal.pref.getInt("low_freq_count", 5);   // 低频点数量
    const int transitionCount = hal.pref.getInt("trans_cnt", 70);    // 过渡区数量
    const float lowScale = hal.pref.getFloat("low_scale", 0.00011);  // 低频压缩值
    const float highScale = hal.pref.getFloat("high_scale", 0.0009); // 高频压缩值
    const float hf_boost = hal.pref.getFloat("hf_boost", 0.0012f);   // 高频提升值

    for (int i = 0; i < app.SAMPLES / 2; i++)
    {
        // 归一化位置 0~1
        float t = (float)i / (app.SAMPLES / 2 - 1);

        // 低频保持 strong compression，高频趋向 highScale
        if (i < lowFreqCount)
            app.curveScaling[i] = lowScale;
        else if (i < lowFreqCount + transitionCount)
        {
            float t2 = (float)(i - lowFreqCount) / transitionCount;
            // ease-out 效果：前半段变化慢，后半段快，高频延展感更好
            t2 = 1.0f - (1.0f - t2) * (1.0f - t2);
            app.curveScaling[i] = lowScale + (highScale - lowScale) * t2;
        }
        else
        {
            // 高频后半段继续往上翘一点，但不增加参数，用固定函数
            float t3 = (float)(i - lowFreqCount - transitionCount) / (app.SAMPLES / 2 - lowFreqCount - transitionCount);
            // 轻微上翘到 highScale 和 1.0 之间的某个值
            float extra = t3 * t3 * hf_boost; // 0.3 是固定上翘幅度，按需调整
            app.curveScaling[i] = highScale + (1.0f - highScale) * extra;
        }
    }

    esp_err_t ret = dsps_fft2r_init_fc32(NULL, app.SAMPLES);
    if (ret != ESP_OK)
    {
        log_e("FFT init failed: %d", ret);
    }
    dsps_wind_hann_f32(app.wind, app.SAMPLES); // 计算窗系数
}

/**
 * @brief 根据当前频谱数据自动计算合适的增益系数（含防抖与平滑）
 * @param spectrum 频谱幅度数组（通常为 vReal 前 SAMPLES/2 个元素）
 * @param len      数组长度（SAMPLES/2）
 * @return         调整后的 fft_gain 值（建议直接赋给 fft_gain）
 */
float USBAudioSpeakerApp::calculateAutoGain(float *spectrum, int len)
{
    // 可配置参数
    const float TARGET_SATURATION = 0.04f; // 目标饱和比例（4%）
    const float GAIN_MIN = 0.1f;           // 最小增益
    const float GAIN_MAX = 8.0f;          // 最大增益
    const float GAIN_UP_STEP = 1.03f;      // 增益上调系数（3%）
    const float GAIN_DOWN_STEP = 0.94f;    // 增益下调系数（6%）
    const float SATURATION_THRESH = 84.0f; // 饱和判定阈值（与限幅值一致）
    const float LOW_ENERGY_THRESH = 30.0f; // 整体偏低阈值
    const int VOTE_NEEDED = 4;             // 需连续符合判定阈值的次数
    const int ADJUST_INTERVAL = 20;        // 调整间隔帧数

    // 统计饱和柱子数量和最大幅值
    int saturated = 0;
    float max_amp = 0.0f;
    for (int i = 0; i < len; i++)
    {
        float val = spectrum[i];
        if (val >= SATURATION_THRESH)
            saturated++;
        if (val > max_amp)
            max_amp = val;
    }
    float sat_ratio = (float)saturated / len;

    // 强平滑 (alpha 0.03)
    static float smooth_sat = 0.0f;
    static float smooth_max = 0.0f;
    const float alpha = 0.05f;
    smooth_sat = alpha * sat_ratio + (1.0f - alpha) * smooth_sat;
    smooth_max = alpha * max_amp + (1.0f - alpha) * smooth_max;

    static int adjust_counter = 0;
    if (++adjust_counter < ADJUST_INTERVAL)
        return fft_gain;
    adjust_counter = 0;

    // 投票计数
    static int up_votes = 0, down_votes = 0;

    if (smooth_sat > TARGET_SATURATION * 1.5f)
    {
        down_votes++;
        up_votes = 0;
    }
    else if (smooth_sat < TARGET_SATURATION * 0.4f && smooth_max < LOW_ENERGY_THRESH)
    {
        up_votes++;
        down_votes = 0;
    }
    else
    {
        up_votes = 0;
        down_votes = 0;
    }

    float new_gain = fft_gain;

    if (down_votes >= VOTE_NEEDED)
    {
        new_gain *= GAIN_DOWN_STEP;
        down_votes = 0;
    }
    else if (up_votes >= VOTE_NEEDED)
    {
        new_gain *= GAIN_UP_STEP;
        up_votes = 0;
    }

    // 范围限制
    if (new_gain > GAIN_MAX)
        new_gain = GAIN_MAX;
    if (new_gain < GAIN_MIN)
        new_gain = GAIN_MIN;

    if (fft_gain != new_gain)
        log_i("%f => %f", fft_gain, new_gain);

    return new_gain;
}

void USBAudioSpeakerApp::show_display_fft()
{
    display.clearScreen();
    // 获取当前写指针快照（保证原子性）
    uint32_t current_write = write_index;
    // 计算最新SAMPLES个样本的起始索引
    int start = (current_write - SAMPLES + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
    for (int i = 0; i < SAMPLES; i++)
    {
        vReal[i] = ring_buffer[(start + i) % RING_BUFFER_SIZE];
        // vImag[i] = 0.0f;
    }

    // // 1. 采样数据加窗（汉明窗）
    // FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HANN, FFT_FORWARD);

    // // 2. FFT计算
    // FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);

    // // 3. 复数转幅度
    // FFT.complexToMagnitude(vReal, vImag, SAMPLES);

    // 2. 加窗，填充复数交叠数组
    for (int i = 0; i < SAMPLES; i++)
    {
        fft_data[2 * i] = vReal[i] * wind[i];
        fft_data[2 * i + 1] = 0.0f; // 虚部置零
    }

    // 3. 执行实数 FFT
    dsps_fft2r_fc32(fft_data, SAMPLES);    // 原位计算
    dsps_bit_rev_fc32(fft_data, SAMPLES);  // 位反转排序
    dsps_cplx2reC_fc32(fft_data, SAMPLES); // 分离正负频率（为取幅值做准备）

    // 4. 计算幅度谱，存入 vReal 的前半段
    for (int i = 0; i < SAMPLES / 2; i++)
    {
        float re = fft_data[2 * i];
        float im = fft_data[2 * i + 1];
        vReal[i] = sqrtf(re * re + im * im); // 与 ArduinoFFT 行为一致，无额外缩放
    }

    // 4. 幅度谱对数变换和归一化（可调参数）
    for (int i = 0; i < SAMPLES / 2; i++)
    {

        vReal[i] = vReal[i] * curveScaling[i];

        vReal[i] = vReal[i] * fft_gain; // FFT增益控制

        // 平滑处理
        vReal[i] = smoothingFactor * previousSpectrum[i] + (1 - smoothingFactor) * vReal[i];
    }

    if (hal.pref.getBool("AutoGain", true))
        fft_gain = calculateAutoGain(vReal, SAMPLES / 2);

    for (int i = 0; i < SAMPLES / 2; i++)
    {
        // 限幅处理
        if (vReal[i] > 84.0f)
            vReal[i] = 84.0f;
        if (vReal[i] < 0.0f)
            vReal[i] = 0.0f;
        // 保存数据用于显示和平滑
        previousSpectrum[i] = vReal[i];
    }

    // FFT 参数
    const int FFT_SIZE = SAMPLES;      // 总点数
    const int HALF_FFT = FFT_SIZE / 2; // 正频率点数

    // -------------------------------------------------------------
    // 柱子模式
    // -------------------------------------------------------------
    if (hal.pref.getBool("bar_mode", false))
    {
        const int NUM_BARS = 128;
        int8_t barSpectrum[NUM_BARS];
        const int srcLen = HALF_FFT; // 频谱数组长度

        for (int i = 0; i < NUM_BARS; i++)
        {
            float srcIndex;
            srcIndex = (float)i * (srcLen - 1) / (NUM_BARS - 1);

            int idx = (int)srcIndex;
            float frac = srcIndex - idx;

            if (idx < srcLen - 1)
            {
                barSpectrum[i] = (int8_t)(previousSpectrum[idx] * (1.0f - frac) + previousSpectrum[idx + 1] * frac);
            }
            else
            {
                barSpectrum[i] = (int8_t)previousSpectrum[srcLen - 1];
            }
        }

        // 绘制柱子（与原代码相同）
        for (int i = 0; i < NUM_BARS; i++)
        {
            int barHeight = (int)(barSpectrum[i]);
            if (barHeight > 83)
                barHeight = 83;
            if (barHeight < 0)
                barHeight = 0;
            display.drawFastVLine(i * 3 + 1, 84 - barHeight, barHeight + 1, TFT_BLACK);
            display.drawFastVLine(i * 3 + 2, 84 - barHeight, barHeight + 1, TFT_BLACK);

            display.drawFastVLine(i * 3 + 1, 85, barHeight, TFT_BLACK);
            display.drawFastVLine(i * 3 + 2, 85, barHeight, TFT_BLACK);
        }
    }
    // -------------------------------------------------------------
    // 连续频谱模式
    // -------------------------------------------------------------
    else
    {
        const int DISPLAY_WIDTH = 384;
        const int srcLen = HALF_FFT;

        uint8_t rawMag[DISPLAY_WIDTH];

        // ----- 1. 填充原始幅度（线性或对数映射）-----
        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            float srcIndex;
            srcIndex = (float)x * (srcLen - 1) / (DISPLAY_WIDTH - 1);

            int idx = (int)srcIndex;
            float frac = srcIndex - idx;
            float mag;
            if (idx < srcLen - 1)
                mag = previousSpectrum[idx] * (1.0f - frac) + previousSpectrum[idx + 1] * frac;
            else
                mag = previousSpectrum[idx];

            int barHeight = (int)mag;
            if (barHeight > 83)
                barHeight = 83;
            if (barHeight < 0)
                barHeight = 0;
            rawMag[x] = (uint8_t)barHeight;
        }
        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            display.drawFastVLine(x, 84 - rawMag[x], rawMag[x] + 1, TFT_BLACK);
            display.drawFastVLine(x, 85, rawMag[x], TFT_BLACK);
        }
    }
    display.display();
}

void USBAudioSpeakerApp::onBtnL_Click(void *param)
{
    USBAudioSpeakerApp *self = static_cast<USBAudioSpeakerApp *>(param);
    self->flag_btnl_click = true;
}

void USBAudioSpeakerApp::setup()
{
    digitalWrite(PIN_DAC_EN, 1);
    hal.cheak_freq(240);
    display.setPowerMode(POWER_MODE_HPM);
    digitalWrite(PIN_DAC_XSMT, 1);

    SAMPLES = hal.pref.getUInt("fft_samples", 256);
    smoothingFactor = hal.pref.getFloat("fft_smooth_val", 0.7f);
    fft_gain = 2;
    vReal = new float[SAMPLES];
    // vImag = new float[SAMPLES];
    curveScaling = new float[SAMPLES / 2];
    previousSpectrum = new float[SAMPLES / 2];
    memset(previousSpectrum, 0, sizeof(float[SAMPLES / 2]));
    wind = (float *)heap_caps_aligned_alloc(16, SAMPLES * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    fft_data = (float *)heap_caps_aligned_alloc(16, 2 * SAMPLES * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    initCurveScaling();
    ring_buffer = (float *)ps_malloc(sizeof(float[RING_BUFFER_SIZE]));
    if (ring_buffer)
        memset(ring_buffer, 0, sizeof(float) * RING_BUFFER_SIZE);

    // 1. 初始化 I2S（仅输出）
    i2s_output = new AudioOutputI2S(0, hal.pref.getInt("dma_buf_count", 8));
    i2s_output->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
    i2s_output->SetMclk(false);
    // i2s_output->set_ConsumeSample_CB(GetSampleCB);
    i2s_output->begin();
    i2s_output->SetRate(48000);

    // 2. 创建 USB 声卡对象（仅扬声器，无麦克风）
    uac = new USBAudioCard(48000, UAC_BPS_16, UAC_SPK_STEREO, UAC_MIC_NONE);

    // 3. 注册事件和数据回调
    uac->onEvent(usbEventCallback);
    uac->onData(onSpkData);

    // 4. 启动音频设备
    if (!uac->begin())
    {
        log_e("USBAudioCard begin failed");
        return;
    }

    hal.btnl.attachClick(onBtnL_Click, this);

    // 5. 注册 USB 系统事件并启动 USB 栈
    USB.onEvent(usbEventCallback);

    USB.begin();

    log_i("USB Audio Speaker ready");
    flag_btnl_click = false;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    TickType_t xFrequency = pdMS_TO_TICKS(20);
    while (!flag_btnl_click)
    {
        show_display_fft();
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    i2s_output->stop();
    uac->end();
    USB.~ESPUSB();
    delete i2s_output;
    delete uac;
    appManager.goBack();
    digitalWrite(PIN_DAC_XSMT, 0);
    display.setPowerMode(POWER_MODE_LPM);
    digitalWrite(PIN_DAC_EN, 0);
}