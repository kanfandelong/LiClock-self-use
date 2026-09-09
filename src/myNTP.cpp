// 使用ESP32官方NTP库更新时间，手动更新时间方便计算误差
#include "A_Config.h"
// DEBUG
#include <WiFiUdp.h>
static WiFiUDP udp;
static const int NTP_PACKET_SIZE = 48;        // NTP time is in the first 48 bytes of message
static byte NTPpacketBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing packets

// send an NTP request to the time server at the given address
static void sendNTPpacket(IPAddress &address)
{
    // set all bytes in the buffer to 0
    memset(NTPpacketBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    NTPpacketBuffer[0] = 0b11100011; // LI, Version, Mode
    NTPpacketBuffer[1] = 0;          // Stratum, or type of clock
    NTPpacketBuffer[2] = 6;          // Polling Interval
    NTPpacketBuffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    NTPpacketBuffer[12] = 49;
    NTPpacketBuffer[13] = 0x4E;
    NTPpacketBuffer[14] = 49;
    NTPpacketBuffer[15] = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); // NTP requests are to port 123
    udp.write(NTPpacketBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}
static time_t getNtpTime()
{
    IPAddress ntpServerIP;
    while (udp.parsePacket() > 0)
        ;
    WiFi.hostByName("ntp.aliyun.com", ntpServerIP);
    udp.begin(1337);
    sendNTPpacket(ntpServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 2000)
    {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            udp.read(NTPpacketBuffer, NTP_PACKET_SIZE);
            unsigned long secsSince1900;
            secsSince1900 = (unsigned long)NTPpacketBuffer[40] << 24;
            secsSince1900 |= (unsigned long)NTPpacketBuffer[41] << 16;
            secsSince1900 |= (unsigned long)NTPpacketBuffer[42] << 8;
            secsSince1900 |= (unsigned long)NTPpacketBuffer[43];
            udp.stop();
            return secsSince1900 - 2208988800UL;
        }
    }
    udp.stop();
    return 0; // return 0 if unable to get the time
}

void NTPSync()
{
    time_t timenow;
    int count = 0;
    timenow = getNtpTime();
    while (timenow == 0 || timenow == 0xFFFFFFFF)
    {
        delay(500);
        timenow = getNtpTime();
        count++;
        if (count > 5)
        {
            display.clearScreen();
            u8g2Fonts.setCursor(70, 80);
            u8g2Fonts.print("NTP同步失败");
            display.display();
            delay(100);
            // hal.powerOff(false);
            return;
        }
    }
    struct timeval tv;
    time_t now;
    tv.tv_sec = timenow;
    tv.tv_usec = 0;

    time(&now);
    settimeofday(&tv, NULL);
    if ((peripherals.peripherals_current & PERIPHERALS_DS3231_BIT) && !hal.dis_DS3231)
    {
        xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
        DateTime rtc_dt = RTClib::now();
        time_t rtc_unix = rtc_dt.unixtime();
        xSemaphoreGive(peripherals.i2cMutex);

        const time_t RTC_INVALID_THRESHOLD = 1609459200; // 2021-01-01 00:00:00 UTC

        if (rtc_unix < RTC_INVALID_THRESHOLD) // 初始化同步RTC时间
        {
            log_i("RTC 未初始化，正在同步时间...");

            xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
            RTClib::adjust(timenow);
            xSemaphoreGive(peripherals.i2cMutex);

            hal.pref.putLong("rtc_sync", timenow);
        }
        else // 更新RTC时间并更新老化寄存器
        {
            int rtc_offset = (int)(rtc_unix - timenow);
            log_i("rtc_offset:%d", rtc_offset);

            if (abs(rtc_offset) > 2)
            {
                log_i("RTC 误差超过2秒，写入新时间并更新老化寄存器");

                hal.pref.putLong("rtc_sync", timenow);
                hal.pref.putInt("rtc_offset", rtc_offset);

                xSemaphoreTake(peripherals.i2cMutex, portMAX_DELAY);
                RTClib::adjust(timenow);
                xSemaphoreGive(peripherals.i2cMutex);

                hal.rtc_offset();
                log_i("DS3231 已同步，老化寄存器已更新，偏移秒数:%d", rtc_offset);
            }
            else
            {
                log_i("RTC 误差在(<=2秒)，等待误差更大再同步");
            }
        }
    }
    // 用原数据计算误差（ESP32的RTC修正和校准）
    {
        int64_t tmp;
        time_t now1 = now;
        if (hal.delta != 0 && hal.lastsync < now1)
        {
            // 下面修正时钟频率偏移
            tmp = now1 - hal.lastsync;
            tmp *= hal.delta;
            tmp /= hal.every;
            now1 -= tmp;
        }
        hal.last_update_delta = now1 - timenow;
    }
    if (hal.lastsync != 1 && ((now / 31536000 + 1970) > 2020))
    {
        int32_t every = tv.tv_sec - hal.lastsync;
        int delta = now - tv.tv_sec;
        if (delta > 2)
        {
            hal.pref.putInt("every", every);
            hal.pref.putInt("delta", delta);
            hal.every = every;
            hal.delta = delta;
            log_i("误差已更新，经过%d秒误差%d秒,用上次得到的参数修正后的RTC时间,作为当前时间与NTP相比误差为%d秒", every, delta, hal.last_update_delta);
        }
        else
        {
            log_i("误差过小，在误差修正过程中请尽可能使用睡眠模式");
        }
    }
    else if (hal.every != 100)
    {
        log_i("首次同步时间,已加载RTC偏移修正参数");
    }
    else
    {
        log_i("首次同步时间, now=%u", tv.tv_sec);
    }
    hal.pref.putUInt("lastsync", tv.tv_sec);
    hal.lastsync = tv.tv_sec;
}
