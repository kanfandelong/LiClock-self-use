#include <A_Config.h>
#include <peripherals.h>
// SPIClass SDSPI(HSPI);
Peripherals peripherals;
void Peripherals::check()
{
    display.clearScreen();
    GUI::drawWindowsWithTitle("重新检测外设", 0, 0, 296, 128);
    u8g2Fonts.drawUTF8(20, 30, "检测到硬件更改，正在重新检测外设");
    display.display();
    String msg = "";
    // 先检测I2C设备
    uint16_t i2cbitmask = 0;
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    Wire.beginTransmission(AHTX0_I2CADDR_DEFAULT);
    if (Wire.endTransmission() == 0)
    {
        i2cbitmask |= PERIPHERALS_AHT20_BIT;
        msg += "AHT20温湿度传感器\n";
    }
    Wire.beginTransmission(BMP280_ADDRESS);
    if (Wire.endTransmission() == 0)
    {
        i2cbitmask |= PERIPHERALS_BMP280_BIT;
        msg += "BMP280气压传感器\n";
    }
    Wire.beginTransmission(SGP30_I2CADDR_DEFAULT);
    if (Wire.endTransmission() == 0)
    {
        i2cbitmask |= PERIPHERALS_SGP30_BIT;
        msg += "SGP30空气质量传感器\n";
    }
    Wire.beginTransmission(0x68);
    if (Wire.endTransmission() == 0)
    {
        i2cbitmask |= PERIPHERALS_DS3231_BIT;
        msg += "DS3231高精度RTC\n";
    }
    Wire.beginTransmission(0x44);
    if (Wire.endTransmission() == 0)
    {
        i2cbitmask |= PERIPHERALS_SHT30_BIT;
        msg += "SHT30温湿度传感器\n";
    }
    Wire.beginTransmission(0x55);
    if (Wire.endTransmission() == 0)
    {
        i2cbitmask |= PERIPHERALS_BQ27441_BIT;
        msg += "BQ27441电量计\n";
    }
    xSemaphoreGive(i2cMutex);
    info("Peripherals check OK: 0x%02x", i2cbitmask);
    Serial0.println(msg);
    GUI::msgbox("检测到的外设", msg.c_str(), 5);
    i2cbitmask |= PERIPHERALS_SD_BIT;
    hal.pref.putUShort(SETTINGS_PARAM_PHERIPHERAL_BITMASK, i2cbitmask);
    peripherals_current = i2cbitmask;
    ESP.restart();
}

void Peripherals::init()
{
    Wire.begin(PIN_SDA, PIN_SCL, hal.pref.getInt("I2C_freq", 100000));
    i2cMutex = xSemaphoreCreateMutex();
    xSemaphoreGive(i2cMutex);
//     SDSPI.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, -1);
    peripherals_current = hal.pref.getUShort(SETTINGS_PARAM_PHERIPHERAL_BITMASK, 0xffff);
    if (peripherals_current == 0xffff)
    {
        check();
    }
}
void Peripherals::initSGP()
{
    if (peripherals_current & PERIPHERALS_SGP30_BIT && sgpInited == false)
    {
        if (!sgp.begin(&Wire, false))
        {
            Serial0.println("Sensor not found :(");
            xSemaphoreGive(i2cMutex);
            check();
            return;
        }
        Serial0.print("Found SGP30 serial #");
        Serial0.print(sgp.serialnumber[0], HEX);
        Serial0.print(sgp.serialnumber[1], HEX);
        Serial0.println(sgp.serialnumber[2], HEX);
        sgpInited = true;
    }
}
uint16_t Peripherals::checkAvailable(uint16_t bitmask)
{
    return (peripherals_current & bitmask) ^ bitmask;
}

bool Peripherals::load(uint16_t bitmask)
{
    bool staitus = true;
    info("外设加载:0x%x -> 0x%x,当前安装,0x%x", peripherals_load, bitmask, peripherals_current);
    if (bitmask & PERIPHERALS_SD_BIT && (peripherals_load & PERIPHERALS_SD_BIT) == 0)
    {
        // 需要加载TF卡
        // 首先测试TF卡是否存在
        if (digitalRead(PIN_SD_CARDDETECT) != 1)
        {
            Serial0.println("[外设] 加载TF卡");
            gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
            digitalWrite(PIN_SDVDD_CTRL, 0);
            gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
            delay(50);
            SD_MMC.setPins(PIN_SD_SCLK, PIN_SD_CMD, PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3);
            uint32_t freq = (uint32_t)hal.pref.getInt("sd_clk_freq" , 3500000);
            Serial0.printf("[外设] 设置TF卡频率:%d HZ\n", freq); 
            if (SD_MMC.begin("/sd", false, false, freq) == false)
            {
                SD_MMC.end();
                delay(100);
                info("TF卡挂载失败,尝试重新挂载");
                if (SD_MMC.begin("/sd", false, false, freq) == false)
                {
                    GUI::msgbox("错误", "存在TF卡，但无法挂载", 5);
                    SD_MMC.end();
                    gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
                    digitalWrite(PIN_SDVDD_CTRL, 1);
                    gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
                    bitmask &= ~PERIPHERALS_SD_BIT; // 加载失败，清加载标志
                    staitus = false;
                }
            }
        }else{
            log_w("[外设] 未插入TF卡");
            bitmask &= ~PERIPHERALS_SD_BIT;// 加载失败，清加载标志
        }
    }
    else if ((bitmask & PERIPHERALS_SD_BIT) == 0 && peripherals_load & PERIPHERALS_SD_BIT)
    {
        // 卸载TF卡
        Serial0.println("[外设] 卸载TF卡");
        SD_MMC.end();
        delay(50);
        gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
        digitalWrite(PIN_SDVDD_CTRL, 1);
        gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
    }
    // 只有sgp和SD卡需要重新加载
    if (bitmask & PERIPHERALS_SGP30_BIT && (peripherals_current & PERIPHERALS_SGP30_BIT) && (peripherals_load & PERIPHERALS_SGP30_BIT == 0))
    {
        Serial0.println("[外设] 加载SGP30");
        // 需要加载sgp
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        if (!sgpInited)
            initSGP();
        sgp.IAQinit();
        xSemaphoreGive(i2cMutex);
    }
    else if ((bitmask & PERIPHERALS_SGP30_BIT) == 0 && (peripherals_current & PERIPHERALS_SGP30_BIT) && (peripherals_load & PERIPHERALS_SGP30_BIT))
    {
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        if (!sgpInited)
            initSGP();
        sgp.softReset();
        xSemaphoreGive(i2cMutex);
    }
    // 之后检查有些可选外设
    //  尝试按需初始化外设
    if (bitmask & PERIPHERALS_AHT20_BIT && (peripherals_current & PERIPHERALS_AHT20_BIT) && ahtInited == false)
    {
        Serial0.println("[外设] 首次加载AHT20");
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        if (!aht.begin())
        {
            xSemaphoreGive(i2cMutex);
            warn("Could not find AHT? Check wiring");
            check();
        }
        else
            xSemaphoreGive(i2cMutex);
        ahtInited = true;
    }
    if ((bitmask & PERIPHERALS_SHT30_BIT) && (peripherals_current & PERIPHERALS_SHT30_BIT == 0))
    {
        Serial0.println("[外设] 首次加载SHT30");
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        if (!sht.begin())
        {
            xSemaphoreGive(i2cMutex);
            warn("Could not find AHT? Check wiring");
            check();
        }
        else{
            uint16_t sht_stautus = sht.readStatus();
            xSemaphoreGive(i2cMutex);
        }
    }
    // if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
    if (bitmask & PERIPHERALS_BMP280_BIT && peripherals_current & PERIPHERALS_BMP280_BIT && bmpInited == false)
    {
        Serial0.println("[外设] 首次加载BMP280");
        xSemaphoreTake(i2cMutex, portMAX_DELAY);
        if (!bmp.begin())
        {
            xSemaphoreGive(i2cMutex);
            warn("Could not find a valid BMP280 sensor, check wiring or "
                  "try a different address!");
            check();
        }
        else
            xSemaphoreGive(i2cMutex);
        /* Default settings from datasheet. */
        bmp.setSampling(Adafruit_BMP280::MODE_FORCED,    /* Operating Mode. */
                        Adafruit_BMP280::SAMPLING_X2,    /* Temp. oversampling */
                        Adafruit_BMP280::SAMPLING_X16,   /* Pressure oversampling */
                        Adafruit_BMP280::FILTER_X16,     /* Filtering. */
                        Adafruit_BMP280::STANDBY_MS_63); /* Standby time. */
        bmpInited = true;
    }
    // DS3231无需初始化
    peripherals_load = bitmask;
    return staitus;
}
void Peripherals::load_append(uint16_t bitmask)
{
    int tmp = peripherals_load;
    if (tmp | bitmask == tmp)
        return;
    peripherals.load(bitmask | tmp);
}

void Peripherals::tf_unload(bool save_power){
    peripherals_load &= ~PERIPHERALS_SD_BIT;
    if (digitalRead(PIN_SD_CARDDETECT) == HIGH){
        log_w("[外设] TF卡不存在，无需卸载");
        return;
    }else if((peripherals_load & PERIPHERALS_SD_BIT) == 0){
        log_w("[外设] 未加载TF卡，无需卸载");
    }
    SD_MMC.end();
    delay(100);
    gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
    if (save_power)
        digitalWrite(PIN_SDVDD_CTRL, 0);
    else
        digitalWrite(PIN_SDVDD_CTRL, 1);
    gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
    if (save_power){
        log_i("[外设] 卸载并保持TF卡供电\n");
    }else{
        log_i("[外设] 卸载并关闭TF卡供电\n");
    }
}

void Peripherals::sleep()
{
    if(config[TFmode] == "0")
    {
        if ((peripherals_load & PERIPHERALS_SD_BIT) && digitalRead(PIN_SD_CARDDETECT) != HIGH)
        {
            SD_MMC.end();
            delay(50);
            gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
            digitalWrite(PIN_SDVDD_CTRL, 1);
            gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
            Serial0.printf("[外设] 卸载并关闭TF卡供电\n");
            // F_LOG("卸载并关闭TF卡供电");
        }else if((peripherals_load & PERIPHERALS_SD_BIT) && digitalRead(PIN_SD_CARDDETECT) != LOW){
            log_w("[外设] TF卡不存在，无需卸载");
        }
    }
    else
    {
        if ((peripherals_load & PERIPHERALS_SD_BIT) && digitalRead(PIN_SD_CARDDETECT) != HIGH)
        {
            SD_MMC.end();
            delay(500);
            gpio_hold_dis((gpio_num_t)PIN_SDVDD_CTRL);
            digitalWrite(PIN_SDVDD_CTRL, 0);
            gpio_hold_en((gpio_num_t)PIN_SDVDD_CTRL);
            //gpio_deep_sleep_hold_en();
            Serial0.printf("[外设] 卸载并保持TF卡供电\n");
            // F_LOG("卸载并保持TF卡供电");
        }else if((peripherals_load & PERIPHERALS_SD_BIT) && digitalRead(PIN_SD_CARDDETECT) != LOW){
            log_w("[外设] TF卡不存在，无需卸载");
        }
    }
    gpio_deep_sleep_hold_en();    
    if (i2cMutex == NULL)
        return;
    xSemaphoreTake(i2cMutex, portMAX_DELAY);
    if (peripherals_load & PERIPHERALS_SGP30_BIT)
    {
        xSemaphoreGive(i2cMutex);
        sgp.softReset();
        delay(50);
    }
    else
        xSemaphoreGive(i2cMutex);
    // 这里不清除加载的设备，唤醒后自动重新加载
}

void Peripherals::wakeup()
{
    if (peripherals_load == 0)
        return;
    uint16_t tmp = peripherals_load;
    peripherals_load = 0;
    load(tmp);
}
