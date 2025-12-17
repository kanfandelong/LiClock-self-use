#include "AppManager.h"
//#include "WAVRecorder.h"

#define SAMPLE_RATE 16000
#define SAMPLE_LEN 8
#define MIC_PIN 36
#define MIC_PIN_2 32
#define MIC_POWER_PIN 32
#define one_CHANNELS 1
#define two_CHANNELS 2

class WAVGenerator {
public:
    WAVGenerator(File& file, uint32_t rate, uint16_t length, uint16_t channels) : dataFile(file), samplingRate(rate), sampleLength(length), numChannels(channels) {
        writeHeader();
    }

    void appendBuffer(const uint8_t* buffer, size_t size, bool isActive) {
        if (isActive) {
            dataFile.write(buffer, size);
            dataSize += size;
        }
    }

    void create() {
        dataFile.seek(4);
        dataFile.write((uint32_t)(dataSize + 36));
        dataFile.seek(40);
        dataFile.write((uint32_t)dataSize);
        dataFile.close();
    }

private:
    File& dataFile;
    uint32_t samplingRate;
    uint16_t sampleLength;
    uint16_t numChannels;
    uint32_t dataSize = 0;

    void writeHeader() {
        dataFile.write((uint8_t *)"RIFF", 4);
        dataFile.write((uint32_t)0); // 文件大小，稍后填充
        dataFile.write((uint8_t *)"WAVE", 4);
        dataFile.write((uint8_t *)"fmt ", 4);
        dataFile.write((uint32_t)16); // 子块大小
        dataFile.write((uint16_t)1); // 格式代码，PCM=1
        dataFile.write((uint16_t)numChannels);
        dataFile.write((uint32_t)samplingRate);
        dataFile.write((uint32_t)(samplingRate * numChannels * sampleLength / 8)); // 字节率
        dataFile.write((uint16_t)(numChannels * sampleLength / 8)); // 块对齐
        dataFile.write((uint16_t)sampleLength);
        dataFile.write((uint8_t *)"data", 4);
        dataFile.write((uint32_t)0); // 数据大小，稍后填充
    }
};

// WAV生成器实例
WAVGenerator* wg = nullptr;

/* WAVRecorder* wr;
SoundActivityDetector* SoundActivity;
channel_t channels[] = {{MIC_PIN}};
channel_t channels_2[] = {{MIC_PIN},{MIC_PIN_2}}; */
File wavfile; 

static const uint8_t WAVRecorder_bits[] = {
   0x00, 0xc0, 0x03, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x38, 0x1c, 0x00,
   0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x04, 0x20, 0x00,
   0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00,
   0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00,
   0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00,
   0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00,
   0x00, 0x06, 0x60, 0x00, 0x00, 0x04, 0x20, 0x00, 0x80, 0x0c, 0x30, 0x01,
   0x80, 0x19, 0x98, 0x01, 0x80, 0x79, 0x9e, 0x01, 0x00, 0xe3, 0xc7, 0x00,
   0x00, 0x86, 0x61, 0x00, 0x00, 0x9c, 0x39, 0x00, 0x00, 0xf8, 0x1f, 0x00,
   0x00, 0xc0, 0x03, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80, 0x01, 0x00,
   0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00 };

class AppWAVRecorder : public AppBase
{
private:
    /* data */
public:
    AppWAVRecorder()
    {
        name = "WAVRecorder";
        title = "录音";
        description = "基于ESP32的录音机";
        image = WAVRecorder_bits;
    }
    void set();
    void setup();
    void menu();    
    void recorder_mode();
    String file_name();
    void selctfolder();
    std::list<String> folderlist;
};
static AppWAVRecorder app;

const char *foldername;
long start_time, end_time;
bool exit_flag = true;
bool recording = false, isRecording = false, cheak_r = false, cheak_c = false, stop_recording = false;

void IRAM_ATTR buttonr_interrupt(){
    stop_recording = true;
    cheak_r = true;
}
void IRAM_ATTR buttonc_interrupt(){
    stop_recording = true;
    cheak_c = true;
}
void task_recording(void *){
    static uint16_t buffer[1024]; // 缓冲区大小
    static size_t bufferIndex = 0;
    while(1){
        if (isRecording) {
            while(1){

                if (bufferIndex >= sizeof(buffer) / 2) {
                    wg->appendBuffer((uint8_t*)buffer, bufferIndex * 2, true);
                    bufferIndex = 0;
                }

                uint16_t result16 = analogRead(MIC_PIN);
                buffer[bufferIndex++] = result16;

                // 控制采样频率
                delayMicroseconds(1000000 / SAMPLE_RATE);
                if (millis () - start_time > hal.pref.getInt("recorder_time", 3000))
                    isRecording = false;}
                while (stop_recording)
                    delay(100);
        }else
            delay(100);
    }
}
void stopRecording() {
    if (isRecording) {
        isRecording = false;
        wg->create(); // 结束WAV文件
        Serial.println("录音结束！");
    }
}
void startRecording() {
    isRecording = true;

/*     // 启动定时器进行采样
    timerBegin(0, 80, true); // 分频系数为80，计数器时钟为1MHz
    timerAttachInterrupt(0, &sampleAndBuffer, true);
    timerAlarmWrite(0, 1000000 / SAMPLE_RATE, true); // 设置中断周期
    timerAlarmEnable(0); */
    log_i("task_recording start");
    xTaskCreatePinnedToCore(task_recording, "task_recording", 8192, NULL, 5, NULL, 0);
}
static void app_exit(){
    if (recording)
        //wr->stop();
    digitalWrite(MIC_POWER_PIN, LOW);
    gpio_set_drive_capability(GPIO_NUM_32, GPIO_DRIVE_CAP_DEFAULT);
    pinMode(MIC_POWER_PIN, INPUT);
}

void AppWAVRecorder::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}

void AppWAVRecorder::setup(){
    exit = app_exit;
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTONR), buttonr_interrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTONC), buttonc_interrupt, FALLING);
    if (hal.pref.getBool(hal.get_char_sha_key("存储在littleFS"), false))
        wavfile = LittleFS.open(file_name(), "w");
    else{
        peripherals.load(PERIPHERALS_SD_BIT);
        wavfile = SD.open(file_name(), "w");
    }
    if (hal.pref.getBool(hal.get_char_sha_key("立体声"), false)){
        pinMode(MIC_PIN, INPUT);
        pinMode(MIC_PIN_2, INPUT);
        //wr = new WAVRecorder(12, channels, one_CHANNELS, SAMPLE_RATE, SAMPLE_LEN, &Serial);
    }else{
        pinMode(MIC_PIN, INPUT);
        pinMode(MIC_POWER_PIN, OUTPUT);
        gpio_set_drive_capability(GPIO_NUM_32, GPIO_DRIVE_CAP_3);
        digitalWrite(MIC_POWER_PIN, HIGH);
        analogSetPinAttenuation(MIC_PIN, ADC_0db);
        //wr = new WAVRecorder(12, channels_2, two_CHANNELS, SAMPLE_RATE, SAMPLE_LEN, &Serial);
        wg = new WAVGenerator(wavfile, SAMPLE_RATE, SAMPLE_LEN, one_CHANNELS);
    }
    analogReadResolution(12);
    //wr->setFile(&wavfile);
    if(hal.pref.getBool(hal.get_char_sha_key("自动检测音量录音"), false)){
        //SoundActivity = new SoundActivityDetector(MIC_PIN, hal.pref.getInt("recorder-c", 10), hal.pref.getInt("recorder-v", 100), hal.pref.getInt("recorder-^", 150), &Serial);
    }
    while(exit_flag){
        if (!GUI::waitLongPress(PIN_BUTTONR) && recording){
            
            if (GUI::msgbox_yn("提示", "是否停止录音？")){
                //wr->stop();
                stopRecording();
                end_time = millis();
                recording = false;
                char buf[64];
                sprintf(buf, "录音结束，共录制 %.02f 秒", (float)(end_time - start_time) / 1000.00);
                GUI::msgbox("提示", buf);
            }else
                stop_recording = false;
        }else if (hal.btnr.isPressing()){
            if (GUI::waitLongPress(PIN_BUTTONR)){
                GUI::info_msgbox("录音", "录音中，请勿按下复位键。");
                recording = true;
                if (!hal.pref.getBool(hal.get_char_sha_key("定时录音"), false) && !hal.pref.getBool(hal.get_char_sha_key("自动检测音量录音"), false)){
                    startRecording();
                    stop_recording = false;
                    //wr->start();
                }else if(hal.pref.getBool(hal.get_char_sha_key("定时录音"), false)){
                    //wr->startBlocking(hal.pref.getInt("recorder_time", 3000));
                    GUI::info_msgbox("录音完成", "录音完成，请查看录音文件。");
                }else if(hal.pref.getBool(hal.get_char_sha_key("自动检测音量录音"), false)){
                    //wr->start(SoundActivity);
                }
                start_time = millis();
            }
        }
        if (hal.btnc.isPressing() || cheak_c){
            cheak_c = false;
            menu();
        }
        if (start_time - millis() > 2000 && recording){
            float use_time = (float)(millis() - start_time) / 1000;
            char buf[32];
            sprintf(buf, "已录制%.2f秒", use_time);
            GUI::info_msgbox("录音时间", buf);
        }
    }
}

static const menu_select menu_WAVRecorder[] =
{
    {false,"返回"},
    {false,"退出"},
    {true, "存储在littleFS"},
    {false,"选择文件夹"},
    {true, "立体声"},
    {false,"录音模式"},
    {false,NULL},
};

void AppWAVRecorder::menu(){
    int res = 0;
    bool end = false;
    while (!end){
        res = GUI::select_menu("录音设置", menu_WAVRecorder);
        switch (res)
        {
            case 0:
                stop_recording = false;
                end = true;
                break;
            case 1:
                end = true;
                exit_flag = false;
                appManager.goBack();
                break;
            case 2:
                break;
            case 3:
                selctfolder();
                break;
            case 4:
                break;
            case 5:
                recorder_mode();
                break;
            default:
                break;
        }
    }
}

static const menu_select menu_recorder_mode[] =
{
    {false,"返回"},
    {true, "定时录音"},
    {false,"录音时长"},
    {true, "自动检测音量录音"},
    {false,"检测上限"},
    {false,"检测下限"},
    {false,"触发次数"},
    {false,NULL},
};

void AppWAVRecorder::recorder_mode(){
    int res = 0;
    bool end = false;
    while (!end){
        res = GUI::select_menu("录音模式", menu_recorder_mode);
        switch (res)
        {
            case 0:
                end = true;
                break;
            case 1:
                break;
            case 2:
                hal.pref.putInt("recorder_time", GUI::msgbox_number("输入时长(ms)", 5, hal.pref.getInt("recorder_time", 3000)));
                break;
            case 3:
                break;
            case 4:
                hal.pref.putInt("recorder-^", GUI::msgbox_number("输入检测上限", 5, hal.pref.getInt("recorder-^", 150)));
                break;
            case 5:
                hal.pref.putInt("recorder-v", GUI::msgbox_number("输入检测下限", 5, hal.pref.getInt("recorder-v", 100)));
                break;
            case 6:
                hal.pref.putInt("recorder-c", GUI::msgbox_number("输入触发次数", 6, hal.pref.getInt("recorder-c", 10)));
                break;
            default:
                break;
        }
    }
}

// 用于获取文件名
String AppWAVRecorder::file_name() {
    // 文件夹路径
    const char* folder;
    char currentfolder[256];
    size_t s = hal.pref.getBytes("record_folder", currentfolder, 256);
    if (s == 0)
        folder = "/";
    else
        folder = currentfolder;

    // 格式化为 月_日
    char buffer[256], filenamePrefix[256];
    sprintf(buffer, "%s%02d_%02d", folder, peripherals.rtc.getMonth(), peripherals.rtc.getDate());
    sprintf(filenamePrefix, "%02d_%02d", peripherals.rtc.getMonth(), peripherals.rtc.getDate());
    String monthDay = buffer;
    String fileNamePrefix = filenamePrefix;

    // 检查当前日期下已有的文件数量
    int fileCount = 0;
    File root;
    if (hal.pref.getBool(hal.get_char_sha_key("存储在littleFS"), false))
        root = LittleFS.open("/");
    else
        root = SD.open("/");

    while (true) {
        File entry = root.openNextFile();
        if (!entry) 
            break;  // 没有更多文件了
        String fileName = entry.name();
        entry.close();
        if (fileName.startsWith(fileNamePrefix) && fileName.endsWith(".wav")) {
            fileCount++;
        }
    }
    root.close();

    // 返回新的文件名
    return monthDay + "_" + String(fileCount + 1) + ".wav";
}

void AppWAVRecorder::selctfolder()
{
    folderlist.clear();
    File root, file;
    if (hal.pref.getBool(hal.get_char_sha_key("存储在littleFS"), false)){
        root = LittleFS.open("/");
        if (!root)
            F_LOG("root未打开");
        file = root.openNextFile();
    }else{
        root = SD.open("/");
        if (!root)
            F_LOG("root未打开");
        file = root.openNextFile();
    }
    GUI::info_msgbox("提示", "正在创建文件夹列表...");
    while (file)
    {
        String name = file.name();
        if (file.isDirectory())
        {
            folderlist.push_back(file.name());
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    menu_item *fileList = new menu_item[folderlist.size() + 3];
    fileList[0].title = "使用默认";
    fileList[0].icon = NULL;
    fileList[1].title = "根目录";
    fileList[1].icon = NULL;
    int i = 2;
    std::list<String>::iterator it;
    for (it = folderlist.begin(); it != folderlist.end(); ++it)
    {
        fileList[i].title = (*it).c_str();
        fileList[i].icon = NULL;
        ++i;
    }
    fileList[i].title = NULL;
    fileList[i].icon = NULL;
    int appIdx = GUI::menu("请选择文件夹", fileList);
    if (appIdx == 0)
    {
        delete fileList;
        foldername = "/userdat";
    }else if (appIdx == 1){
        delete fileList;
        foldername = "/";
    }else
    {
        /*static char result[256]; 
        strcat(result, "/");
        strcpy(result, fileList[appIdx].title); 
        strcat(result, "/");
        foldername = result;
        Serial.print(foldername);*/



        /*std::string original(fileList[appIdx].title);
        std::string modified = "/" + original + "/";
        char* result = new char[modified.length() + 1];
        std::strcpy_s(result, modified.c_str());
        foldername = result;
        delete[] result;*/

        size_t originalLength = strlen(fileList[appIdx].title);
        size_t newLength = originalLength + 2; // 为两边的 '/' 预留空间
        char* newString = new char[newLength + 1]; // +1 为结尾的 '\0'

        newString[0] = '/'; // 开始处添加 '/'
        strcpy(newString + 1, fileList[appIdx].title); // 复制原字符串
        newString[newLength - 1] = '/'; // 结束处添加 '/'
        newString[newLength] = '\0'; // 终止符
        foldername = newString;
        delete[] newString;
    }
    char currentfolder[256];
    sprintf(currentfolder, "%s", foldername);
    hal.pref.putBytes("record_folder", currentfolder, strlen(currentfolder));
}
