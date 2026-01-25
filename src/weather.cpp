#include <A_Config.h>
// 之前写的程序，一次获取，内存不足可尝试分批
Weather weather;
#define WEATHER_TYPE_COUNT 20
const char *weather_codes[WEATHER_TYPE_COUNT] =
    {
        "CLEAR_DAY",
        "CLEAR_NIGHT",
        "PARTLY_CLOUDY_DAY",
        "PARTLY_CLOUDY_NIGHT",
        "CLOUDY",
        "LIGHT_HAZE",
        "MODERATE_HAZE",
        "HEAVY_HAZE",
        "LIGHT_RAIN",
        "MODERATE_RAIN",
        "HEAVY_RAIN",
        "STORM_RAIN",
        "FOG",
        "LIGHT_SNOW",
        "MODERATE_SNOW",
        "HEAVY_SNOW",
        "STORM_SNOW",
        "DUST",
        "SAND",
        "WIND",
};
void Weather::begin()
{
    File file = LittleFS.open("/System/weather.bin", "r");
    if (!file)
    {
        error("无法打开天气文件，或天气不存在");
        return;
    }
    file.readBytes((char *)&hour24, sizeof(hour24));
    file.readBytes((char *)&rain, sizeof(rain));
    file.readBytes((char *)&five_days, sizeof(five_days));
    file.readBytes((char *)&realtime, sizeof(realtime));
    file.readBytes((char *)&desc1, sizeof(desc1));
    file.readBytes((char *)&desc2, sizeof(desc2));
    file.readBytes((char *)&hasAlert, sizeof(hasAlert));
    file.readBytes((char *)alert, sizeof(alert));
    file.readBytes((char *)alertTitle, sizeof(alertTitle));
    file.readBytes((char *)&alertPubTime, sizeof(alertPubTime));
    file.readBytes((char *)&lastupdate, sizeof(lastupdate));

    file.close();
}
void Weather::save()
{
    File file = LittleFS.open("/System/weather.bin", "w");
    if (!file)
    {
        error("天气文件打开失败");
        return;
    }
    file.write((uint8_t *)&hour24, sizeof(hour24));
    file.write((uint8_t *)&rain, sizeof(rain));
    file.write((uint8_t *)&five_days, sizeof(five_days));
    file.write((uint8_t *)&realtime, sizeof(realtime));
    file.write((uint8_t *)&desc1, sizeof(desc1));
    file.write((uint8_t *)&desc2, sizeof(desc2));
    file.write((uint8_t *)&hasAlert, sizeof(hasAlert));
    file.write((uint8_t *)alert, sizeof(alert));
    file.write((uint8_t *)alertTitle, sizeof(alertTitle));
    file.write((uint8_t *)&alertPubTime, sizeof(alertPubTime));
    file.write((uint8_t *)&lastupdate, sizeof(lastupdate));
    file.close();
}

int8_t Weather::refresh()
{
    HTTPClient http;
    String url = String("http://api.caiyunapp.com/v2.5/96Ly7wgKGq6FhllM/") + config[PARAM_GPS].as<String>() + String("/weather.jsonp?hourlysteps=20&unit=metric%3Av2&dailysteps=4&alert=true");
    if (!http.begin(url))
    {
        warn("HTTP连接失败");
        return -5;
    }
    http.addHeader("Accept", "*/*");
    http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.131 Safari/537.36");

    hal.autoConnectWiFi();
    Serial.println("开始更新天气");
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
        DynamicJsonDocument doc(50000);
        auto s = http.getStream();
        DeserializationError _error = deserializeJson(doc, s);
        if (_error)
        {
            log_w("JSON解析失败: %s\n", _error.c_str());
            http.end();
            return -4;
        }

        if (!doc.containsKey("status") || doc["status"] != "ok")
        { // API失效
            http.end();
            doc.clear();
            warn("天气API已失效");
            return -3;
        }

        if (doc.containsKey("result") && doc["result"].containsKey("alert") && doc["result"]["alert"].containsKey("status") && doc["result"]["alert"]["status"] == "ok")
        {
            if (doc["result"]["alert"].containsKey("content") && doc["result"]["alert"]["content"].size() != 0)
            {
                hasAlert = true;
                if (doc["result"]["alert"]["content"][0].containsKey("description"))
                    strcpy(alert, doc["result"]["alert"]["content"][0]["description"].as<const char *>());
                if (doc["result"]["alert"]["content"][0].containsKey("title"))
                    strcpy(alertTitle, doc["result"]["alert"]["content"][0]["title"].as<const char *>());
                if (doc["result"]["alert"]["content"][0].containsKey("pubtimestamp"))
                    alertPubTime = doc["result"]["alert"]["content"][0]["pubtimestamp"].as<uint32_t>();
            }
        }
        else
        {
            hasAlert = false;
        }

        if (doc["result"].containsKey("hourly") && doc["result"]["hourly"].containsKey("description"))
            strcpy(desc1, doc["result"]["hourly"]["description"].as<const char *>());
        if (doc["result"].containsKey("minutely") && doc["result"]["minutely"].containsKey("description"))
            strcpy(desc2, doc["result"]["minutely"]["description"].as<const char *>());

        for (uint8_t i = 0; i < 20; ++i)
        {
            if (doc["result"].containsKey("hourly") && doc["result"]["hourly"].containsKey("temperature") && doc["result"]["hourly"]["temperature"].size() > i &&
                doc["result"]["hourly"].containsKey("skycon") && doc["result"]["hourly"]["skycon"].size() > i &&
                doc["result"]["hourly"].containsKey("wind") && doc["result"]["hourly"]["wind"].size() > i &&
                doc["result"]["hourly"].containsKey("precipitation") && doc["result"]["hourly"]["precipitation"].size() > i &&
                doc["result"]["hourly"].containsKey("pressure") && doc["result"]["hourly"]["pressure"].size() > i)
            {
                String timestr = doc["result"]["hourly"]["temperature"][i]["datetime"].as<String>();
                timestr = timestr.substring(5, 13);
                strcpy(hour24[i].date, timestr.c_str());

                String s = doc["result"]["hourly"]["skycon"][i]["value"].as<String>();
                hour24[i].weathernum = codeToNum(s.c_str());
                hour24[i].temperature = int16_t(doc["result"]["hourly"]["temperature"][i]["value"].as<float>() * 10);
                hour24[i].winddirection = uint16_t(doc["result"]["hourly"]["wind"][i]["direction"].as<float>());
                hour24[i].windspeed = uint16_t(doc["result"]["hourly"]["wind"][i]["speed"].as<float>() * 10);
                hour24[i].rain = uint16_t(doc["result"]["hourly"]["precipitation"][i]["value"].as<float>() * 100);
                hour24[i].pressure = doc["result"]["hourly"]["pressure"][i]["value"];
            }
            else
            {
                warn("hourly数据不完整，索引 %d", i);
                http.end();
                return -6;
            }
        }

        for (uint8_t i = 0; i < 120; ++i)
        {
            if (doc["result"].containsKey("minutely") && doc["result"]["minutely"].containsKey("precipitation_2h") && doc["result"]["minutely"]["precipitation_2h"].size() > i)
            {
                rain[i] = doc["result"]["minutely"]["precipitation_2h"][i].as<float>() * 100;
            }
            else
            {
                warn("minutely降水数据不完整，索引 %d", i);
                http.end();
                return -7;
            }
        }

        const char *dateStr;
        for (uint8_t i = 0; i < 4; ++i)
        {
            if (doc["result"].containsKey("daily") && doc["result"]["daily"].containsKey("temperature") && doc["result"]["daily"]["temperature"].size() > i &&
                doc["result"]["daily"].containsKey("skycon") && doc["result"]["daily"]["skycon"].size() > i)
            {
                dateStr = doc["result"]["daily"]["temperature"][i]["date"].as<const char *>();
                Serial.println(dateStr ? dateStr : "error");
                five_days[i].max = int16_t(doc["result"]["daily"]["temperature"][i]["max"].as<float>() * 10);
                five_days[i].min = int16_t(doc["result"]["daily"]["temperature"][i]["min"].as<float>() * 10);
                five_days[i].weathernum = codeToNum(doc["result"]["daily"]["skycon"][i]["value"].as<const char *>());
            }
            else
            {
                warn("daily数据不完整，索引 %d", i);
                http.end();
                return -8;
            }
        }

        if (doc["result"].containsKey("realtime") && doc["result"]["realtime"].containsKey("skycon") && doc["result"]["realtime"].containsKey("temperature") &&
            doc["result"]["realtime"].containsKey("humidity") && doc["result"]["realtime"].containsKey("pressure"))
        {
            realtime.weathernum = codeToNum(doc["result"]["realtime"]["skycon"].as<const char *>());
            realtime.temperature = int16_t(doc["result"]["realtime"]["temperature"].as<float>() * 10);
            realtime.humidity = uint16_t(doc["result"]["realtime"]["humidity"].as<float>() * 100);
            realtime.pressure = doc["result"]["realtime"]["pressure"];
        }
        else
        {
            warn("realtime数据不完整");
            http.end();
            return -9;
        }

        doc.clear();
        lastupdate = hal.now;
        save();
        info("天气更新成功");
    }
    else
    {
        http.end();
        warn("天气更新时出现HTTP错误");
        return -2;
    }
    http.end();
    return 0;
}

weatherInfo24H *Weather::getWeather(uint8_t month, uint8_t date, uint8_t hour)
{
    char strdate[9];
    sprintf(strdate, "%02d-%02dT%02d", month, date, hour);
    for (uint8_t i = 0; i < 48; ++i)
    {
        if (strcmp(strdate, hour24[i].date) == 0)
        {
            return &hour24[i];
        }
    }
    return NULL;
}

uint16_t Weather::codeToNum(const char *code)
{
    for (uint8_t i = 0; i < WEATHER_TYPE_COUNT; ++i)
    {
        if (strcmp(weather_codes[i], code) == 0)
        {
            return i;
        }
    }
    return 0;
}