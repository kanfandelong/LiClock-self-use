#pragma once

#include "AudioOutput.h"
#include "driver/i2s_std.h"
#include "hal/i2s_types.h"

#ifdef CONFIG_DAC_32bit
typedef void (*SampleCB)(int32_t sample[2]);
#else
typedef void (*SampleCB)(int16_t sample[2]);
#endif

class AudioOutputI2S : public AudioOutput
{
public:
    AudioOutputI2S(int port = I2S_NUM_0, int dma_buf_count = 8);
    virtual ~AudioOutputI2S() override;

    bool SetPinout(int bclk, int wclk, int dout);
    bool SetPinout(int bclk, int wclk, int dout, int mclk);

    virtual bool SetRate(int hz) override;
    virtual bool SetBitsPerSample(int bits) override;
    virtual bool SetChannels(int channels) override;
    bool SetOutputModeMono(bool mono);
    bool SetLsbJustified(bool lsbJustified);
    bool SetMclk(bool enabled);
    bool SetBitsPerChan(i2s_slot_bit_width_t bitsPerChan);   // 修正类型
    bool set_ConsumeSample_CB(SampleCB fn);
    bool SwapClocks(bool swap_clocks);
    bool SetTimeout(uint32_t timeout_ms);                    // 改为毫秒

    virtual bool begin() override;
#ifdef CONFIG_DAC_32bit
    virtual bool ConsumeSample(int32_t sample[2]) override;
#else
    virtual bool ConsumeSample(int16_t sample[2]) override;
#endif
    virtual void flush() override;
    virtual bool stop() override;

protected:
    virtual int AdjustI2SRate(int hz) { return hz; }

    i2s_chan_handle_t tx_handle = nullptr;
    SampleCB ConsumeSampleCB = nullptr;

    int portNo;
    bool mono;
    bool lsb_justified;
    bool use_mclk;
    bool swap_clocks;
    int dma_buf_count;
    i2s_slot_bit_width_t bits_per_chan;   // 修正
    uint32_t timeout_ms;                  // 单位：毫秒

    uint8_t bclkPin;
    uint8_t wclkPin;
    uint8_t doutPin;
    uint8_t mclkPin;
};