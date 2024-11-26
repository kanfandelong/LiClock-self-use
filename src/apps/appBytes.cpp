#include "AppManager.h"
#include <LittleFS.h>
#include"A_Config.h"

#define LOG(fmt, ...) \
  do { \
    Serial.printf("[%s:%d] ", __FILE__, __LINE__); \
    Serial.printf(fmt, ##__VA_ARGS__); \
  } while (0)

static const menu_item Bytes_menu[] =
{
    {NULL,"返回"},
    {NULL,"退出"},
    {NULL,"重新初始化"},
    {NULL,"显示数据"},
    {NULL,"测试WIFI"},
    {NULL,"选择文件"},
    {NULL,"写入配置"},
    {NULL,"数据操作"},
    {NULL,"空间占用"},
    {NULL,"电池日志"},
    {NULL,"电池使用历史"},
    {NULL,NULL},
};

class AppBytes : public AppBase
{
private:
    /* data */
public:
    AppBytes()
    {
        name = "bytes";
        title = "数据";
        description = "数据读写入";
        image = NULL;
        _showInList = true;
        noDefaultEvent = true;
    }
    void set();
    void setup();
    const char* remove_path_prefix(const char* path, const char* prefix);
    void init();
    void xianshi();
    void ceshi();
    void chaxun();
    void menu();
    void shezhi();
    void shuzi();
    void getVbat(File f);
    void batlog();
};
static AppBytes app;

float offset = 0;
float o = 1.0;
uint16_t a;
//String byteconut[4];
uint64_t shuliang;
char *wen = (char *)"/appdat/Bytes.bin";
const char *textname;

void AppBytes::set() {
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
//初始化数据文件    
void AppBytes::getVbat(File f)
{
    uint8_t date;
    
    
    //int s = f.size();
    
    int b = 0;
    f.seek(ceil(offset),SeekSet);
    f.read(&date, sizeof(date));
    offset = offset - o;
    a = date;
    LOG("%d\n",date);
}

void AppBytes::batlog()
{
    /*
    File f = LittleFS.open("/Batrecording.bin", "r");
    int allsize = f.size();
    if(allsize > 23040)
    {
        offset = allsize - 23040 - 320;
    }*/
    display.clearScreen();
    //u8g2Fonts.drawUTF8(1,14,"电池使用历史");
    int x1 = 295;
    File f = LittleFS.open("/dat/Batlog.bin", "r");
    if (!f)
    {
        LOG("文件未打开");
    }
    int c = f.size();
    int minbat = c / 2;
    u8g2Fonts.setCursor(1,14);
    u8g2Fonts.printf("%d分钟前到现在的电池使用情况\n", minbat);
    offset = (float)c;
    if(c > 296){
        o = (float)c / 296.0;
        c = 296;}
    f.seek(offset,SeekSet);
    for(int i = 0;i < c;i = i + 1)
    {
    getVbat(f);
    //offset = offset + 2;
    display.drawLine(x1,127,x1,127-a,0);
    x1 = x1 - 1;
    }
    display.drawLine(0,27,295,27,0);
    display.display(true);
    f.close();
}

void AppBytes::init()
{
    String byteconut[6];
    byteconut[0] = "ChinaNet-GFcU";
    byteconut[1] = "376kctu9";
    byteconut[2] = "ChinaNet-666";
    byteconut[3] = "822628aa";
    byteconut[4] = "ESP8266";
    byteconut[5] = "822628aa";
    /*for(int i=5;i<47;i++)
    {
        byteconut[i] = i+1;
    }*/
    //LittleFS.mkdir("/dat");
    if(LittleFS.exists("/dat") == false){LittleFS.mkdir("/dat");}
    File f = LittleFS.open("/dat/Bytes.bin", "w");
    if (!f)
    {
        LOG("文件未打开");
    }
    f.seek(0,SeekSet);
    f.write((uint8_t *)byteconut, sizeof(byteconut));
    //f.seek(100,SeekSet);
    f.close();
}
//依次显示所有文件中的数据
void AppBytes::xianshi()
{

    int a = GUI::msgbox_number("需要查询的个数",3,1);
    String bytecon[a];
    File r;
    if (strncmp(textname, "/sd/", 4) == 0)
    {
        r = SD.open(remove_path_prefix(textname,"/sd"), "r");
    }
    else if (strncmp(textname, "/littlefs/", 10) == 0)
    {
        r = LittleFS.open(remove_path_prefix(textname,"/littlefs"), "r"); 
    }
    if (!r)
    {
        LOG("文件未打开");
    }
    r.seek(0,SeekSet);
    r.readBytes((char *)bytecon, sizeof(bytecon));
    r.close();
    const char *ch;
    u8g2Fonts.setBackgroundColor(1);
    u8g2Fonts.setForegroundColor(0);
    int x = 5;
    int y = 28;
    for(int i=0;i<a;i++)
    {
        ch=bytecon[i].c_str();
        u8g2Fonts.drawUTF8(x,y,ch);
        display.display(true);
        y = y+14;
        if(y>128){y = 28;x = x+97;}
        if(x>296){x = 5;}
        if((i+1) % 24 == 0){display.clearScreen();
        GUI::drawWindowsWithTitle("数据", 0, 0, 296, 128);
        display.display(false);}
    }
} 
//测试选中的WIFI数据是否有效
void AppBytes::ceshi()
{   
    String byteconut[2];
    //#include <DNSServer.h>
    int h = GUI::msgbox_number("需要检查的角标",3,1);
    File r;
    if (strncmp(textname, "/sd/", 4) == 0)
    {
        r = SD.open(remove_path_prefix(textname,"/sd"), "r");
    }
    else if (strncmp(textname, "/littlefs/", 10) == 0)
    {
        r = LittleFS.open(remove_path_prefix(textname,"/littlefs"), "r"); 
    }
    if (!r)
    {
        LOG("文件未打开");
    }    
    r.seek(h*16,SeekSet);
    r.readBytes((char *)byteconut, sizeof(byteconut));
    r.close();
    WiFi.setHostname("weatherclock");
    WiFi.mode(WIFI_STA);
    const char *ss = byteconut[0].c_str();
    const char *pa = byteconut[1].c_str();
    WiFi.begin(ss,pa);
    if (WiFi.waitForConnectResult(20000) != WL_CONNECTED)
    {
        GUI::msgbox("提示","当前角标的数据无效");
        LOG("当前角标的数据无效\n");
    }
    else{
        char buf[50];
        sprintf(buf,"WIFI有效！\nIP地址是：%s\n",WiFi.localIP().toString().c_str());
        LOG(buf);
        GUI::msgbox("提示",buf);
    }
}
//选择文件
void AppBytes::chaxun()
{
    textname = GUI::fileDialog("选择文件");
    config[Text] = textname;
    hal.saveConfig();
}
//菜单
void AppBytes::menu()
{
    int res = 0;
    res = GUI::menu("菜单", Bytes_menu);
    switch (res)
    {
    case 0:
        break;
    case 1:
        appManager.goBack();
        return;
        break;
    case 2:
        init();
        break;
    case 3:
        xianshi();
        break;
    case 4:
        ceshi();
        break;
    case 5:
        chaxun();
        break;
    case 6:
        shezhi();
        break;
    case 7:
        shuzi();
        break;
    case 8:
    {
        File r;
        if (strncmp(textname, "/sd/", 4) == 0)
        {
            r = SD.open(remove_path_prefix(textname,"/sd"), "r");
        }
        else if (strncmp(textname, "/littlefs/", 10) == 0)
        {
            r = LittleFS.open(remove_path_prefix(textname,"/littlefs"), "r"); 
        }
        if (!r)
        {
            LOG("文件未打开");
        }
        int needdat=r.size();
        r.close();
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);
        u8g2Fonts.setCursor(5,42);
        u8g2Fonts.printf("当前文件占据的空间：%dbytes(%dKB)",needdat,needdat/1024);
    }
        break;
    case 9:
    {
        File r = LittleFS.open("/dat/Batlog.bin", "r"); 
        if (!r)
        {
            LOG("文件未打开");
        }
        int needdat=r.size();
        r.close();
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);
        u8g2Fonts.setCursor(5,42);
        u8g2Fonts.printf("电池日志占据的空间：%dBytes(%dKB)",needdat,needdat/1024); 
        display.display(true);
        while(1)
        { 
            if(digitalRead(PIN_BUTTONC) == 0)
            {
                if(GUI::msgbox_yn("提示","是否删除电池日志","删除","取消"))
                {
                    LittleFS.remove("/dat/Batlog.bin");
                }
                break;
            }
            else if(digitalRead(PIN_BUTTONL) == 0)
            {
                setup();
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        //batlog();   
        break;
    }
    case 10:
        batlog();
        break;
    default:
        break;
    }
}
//将选定角标的WIFI数据写入设置文件
void AppBytes::shezhi()
{
    String bytedat[2];
    int s = GUI::msgbox_number("设置的数据的角标",3,1);
    File r;
    if (strncmp(textname, "/sd/", 4) == 0)
    {
        r = SD.open(remove_path_prefix(textname,"/sd"), "r");
    }
    else if (strncmp(textname, "/littlefs/", 10) == 0)
    {
        r = LittleFS.open(remove_path_prefix(textname,"/littlefs"), "r"); 
    }
    if (!r)
    {
        LOG("文件未打开");
    }
    r.seek(s*16,SeekSet);
    r.readBytes((char *)bytedat, sizeof(bytedat));
    r.close();
    config[PARAM_SSID] = bytedat[0].c_str();
    config[PARAM_PASS] = bytedat[1].c_str();
    hal.saveConfig();
    WiFi.setHostname("weatherclock");
    WiFi.mode(WIFI_STA);
    WiFi.begin(config[PARAM_SSID].as<const char *>(), config[PARAM_PASS].as<const char *>());
    if (WiFi.waitForConnectResult(20000) != WL_CONNECTED)
    {
        GUI::msgbox("提示","已写入配置文件，\n但WIFI连接失败。");
    }
    char buf[70];
    sprintf(buf,"已写入配置文件，\nWIFI连接成功。\nIP地址是：%s\n",WiFi.localIP().toString().c_str());
    LOG(buf);
    GUI::msgbox("提示",buf);
}
void AppBytes::shuzi()
{
    int m = GUI::msgbox_number("a w r文件操作标志",3,1);
    if(m == 100){
        String wdat[1];
        wdat[0] = String(GUI::englishInput("增加的数据"));
        File f;
        if (strncmp(textname, "/sd/", 4) == 0)
        {
            f = SD.open(remove_path_prefix(textname,"/sd"), "r");
        }
        else if (strncmp(textname, "/littlefs/", 10) == 0)
        {
            f = LittleFS.open(remove_path_prefix(textname,"/littlefs"), "r"); 
        }
        if (!f)
        {
            LOG("文件未打开");
        }
        f.write((uint8_t *)wdat, sizeof(wdat));
        int j = f.position();
        //f.seek(100,SeekSet);
        f.close();
        GUI::drawWindowsWithTitle("数据", 0, 0, 296, 128);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);
        u8g2Fonts.setCursor(5,30);
        u8g2Fonts.printf("新增数据%s在偏移%d(%d)",wdat[0].c_str(),j-16,(j-16)/16);
        display.display(true);
    }
    if(m == 10)
    {
        int s = GUI::msgbox_number("偏移位置",5,0);
        String wdat[1];
        File f;
        if (strncmp(textname, "/sd/", 4) == 0)
        {
            f = SD.open(remove_path_prefix(textname,"/sd"), GUI::englishInput("文件打开模式:a,r+,w+,a+"));
        }
        else if (strncmp(textname, "/littlefs/", 10) == 0)
        {
            f = LittleFS.open(remove_path_prefix(textname,"/littlefs"), GUI::englishInput("文件打开模式:a,r+,w+,a+")); 
        }
        if (!f)
        {
            LOG("文件未打开");
        }
        f.seek(s*16,SeekSet);
        f.readBytes((char *)wdat,sizeof(wdat));
        wdat[0] = String(GUI::englishInput(wdat[0].c_str()));
        f.write((uint8_t *)wdat, sizeof(wdat));
        int j = f.position();
        //f.seek(100,SeekSet);
        f.close();
        GUI::drawWindowsWithTitle("写入数据", 0, 0, 296, 128);
        u8g2Fonts.setBackgroundColor(1);
        u8g2Fonts.setForegroundColor(0);
        u8g2Fonts.setCursor(5,30);
        u8g2Fonts.printf("写入数据%s在偏移%d",wdat[0].c_str(),j - 16);
        display.display(true);
    }
    if(m == 1)
    {
        String bytedat[1];
        int s = GUI::msgbox_number("需要查询角标",3,1);
        File r;
        if (strncmp(textname, "/sd/", 4) == 0)
        {
            r = SD.open(remove_path_prefix(textname,"/sd"), "r");
        }
        else if (strncmp(textname, "/littlefs/", 10) == 0)
        {
            r = LittleFS.open(remove_path_prefix(textname,"/littlefs"), "r"); 
        }
        if (!r)
        {
            LOG("文件未打开");
        }
        r.seek(s*16,SeekSet);
        r.readBytes((char *)bytedat, sizeof(bytedat));
        r.close();
        char buf[35];
        sprintf(buf,"数据:%s",bytedat[0].c_str());
        GUI::msgbox("数据",buf);
        u8g2Fonts.setCursor(5,42);
        u8g2Fonts.printf(buf);
        display.display(true);
    }
}

/**
 * @brief  删除路径的前缀  
 * @param path 输入的路径
 * @param prefix 需要删除的前缀
 * @return 去除前缀后的路径
 */
const char* AppBytes::remove_path_prefix(const char* path, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t path_len = strlen(path);

    // 检查路径是否以指定前缀开头
    if (strncmp(path, prefix, prefix_len) == 0) {
        // 返回去除前缀后的路径
        return path + prefix_len;
    }
    // 如果路径不以指定前缀开头，则返回原始路径
    return path;
}

void AppBytes::setup()
{
    textname = config[Text];
    display.clearScreen();
    GUI::drawWindowsWithTitle("数据", 0, 0, 296, 128);
    u8g2Fonts.setCursor(5,30);
    u8g2Fonts.printf("提示，角标以0为起始");
    display.display(true);
    //delay(100);
    if(digitalRead(PIN_BUTTONC) == 0)
    {
        menu();
        display.display(true);
    }
    else if(digitalRead(PIN_BUTTONL) == 0)
    {
        appManager.goBack();
    }
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 61 - hal.timeinfo.tm_sec;
}


