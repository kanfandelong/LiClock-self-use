#include "AppManager.h"

static const uint8_t wenjianimg[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x3f, 0x00, 0x00,
   0x08, 0x40, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0xf8, 0xff, 0xff, 0x3f,
   0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20,
   0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20,
   0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20,
   0x08, 0x00, 0x00, 0x20, 0x08, 0x00, 0x00, 0x20, 0x88, 0xff, 0xff, 0x20,
   0x88, 0x00, 0x80, 0x20, 0x88, 0xfc, 0x9f, 0x20, 0x88, 0x04, 0x90, 0x20,
   0x88, 0x04, 0x90, 0x20, 0xf8, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

//#define wprintf(fmt, ...) printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG(fmt, ...) \
  do { \
    Serial.printf("[%s:%d] ", __FILE__, __LINE__); \
    Serial.printf(fmt, ##__VA_ARGS__); \
  } while (0)


// 定义bin 文件分页参数
#define LINES_PER_PAGE   9       // 每页n行
#define BYTES_PER_LINE   8       // 每行n个字节
#define BYTES_PER_PAGE  (LINES_PER_PAGE * BYTES_PER_LINE)  // 每页显示n行，每行n个字节


const char *filename;

class Appwenjian : public AppBase
{
private:
    /* data */
    void displayPage(File &file, int page) {
        // 计算当前页面的起始字节
        uint32_t startByte = page * BYTES_PER_PAGE;
        uint32_t datOffset = startByte;
        int a = 0;
        bool end = false;
        // 清屏
        display.clearScreen();

        // 设置字体
        //u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2Fonts.setCursor(0, 10);
  
        file.seek(startByte);
        for (int line = 0; line < LINES_PER_PAGE; line++) {
            
            if (end){
                break;
            }

            u8g2Fonts.print(datOffset, HEX);
            datOffset = datOffset + 8;
            u8g2Fonts.setCursor( 62, u8g2Fonts.getCursorY());

            for (int byte = 0; byte < BYTES_PER_LINE; byte++) {
                if (file.available()) {
                    uint8_t data = file.read();
                    //u8g2.print("0x");
                    if (data == 0)
                    {
                        u8g2Fonts.print("00");
                    }else{
                        if (data < 16)
                        {
                            u8g2Fonts.print("0");
                            u8g2Fonts.print(data, HEX);
                        }else{
                            u8g2Fonts.print(data, HEX);
                        }
                    }
                    a++;
                    u8g2Fonts.setCursor(62 + (17 * a) , u8g2Fonts.getCursorY());
                }else {
                    display.display(true);
                    end = true;
                    break;
                }
            }
            a = 0;
            u8g2Fonts.setCursor(0, u8g2Fonts.getCursorY() + 14);
        }

        display.display(true);
    }
    void openbin(){
        File file;
        if (strncmp(filename, "/sd/", 4) == 0) {
            file = SD.open(remove_path_prefix(filename,"/sd"));
        } 
        else if (strncmp(filename, "/littlefs/", 10) == 0) {
            file = LittleFS.open(remove_path_prefix(filename,"/littlefs"));
        }
        int currentPage = 0;
        int totalPages = 0;
        file.seek(0, SeekEnd);
        totalPages = (file.size() + BYTES_PER_PAGE - 1) / BYTES_PER_PAGE;
        file.seek(0, SeekSet);
        displayPage(file, currentPage);
        bool end = false;
        while (1)
        {
            if (hal.btnr.isPressing()) {
                currentPage++;
                if (currentPage >= totalPages) {
                    currentPage = totalPages - 1;
                }
                displayPage(file, currentPage);
            }
            if (hal.btnl.isPressing()) {
                currentPage--;
                if (currentPage < 0) {
                    currentPage = 0;
                }
                displayPage(file, currentPage);
            }if (hal.btnc.isPressing()) {
                static const menu_item appMenu_main[] = {
                    {NULL, "返回"},
                    {NULL, "退出"},
                    {NULL, "跳转"},
                    {NULL, NULL},
                };
                char *buf = (char *)malloc(128);
                sprintf(buf, "当前页：%d/%d", currentPage + 1, totalPages);
                int res = GUI::menu(buf,appMenu_main);
                switch (res)
                {
                    case 0:
                        break;
                    case 1:
                        end = true;
                        break;
                    case 2:
                    {
                        int digits = 0;
                        int a = totalPages;
                        while (a > 0)
                        {
                            a /= 10;
                            digits++;
                        }
                        currentPage = GUI::msgbox_number("跳转页", digits,currentPage + 1) - 1;
                        if (currentPage < 0) {
                            currentPage = 0;
                        }if (currentPage >= totalPages) {
                            currentPage = totalPages - 1;
                        }
                        displayPage(file, currentPage);
                    }
                    default:
                        break;
                }
                //break;
            }if (end){
                break;
            }
            delay(100);  // 避免按钮抖动
        }
        file.close();
    }
public:
    Appwenjian()
    {
        name = "wenjian";
        title = "文件管理";
        description = "文件管理器";
        image = wenjianimg;
        peripherals_requested = PERIPHERALS_SD_BIT;
        _showInList = true;
    }
    std::list<String> directorylist;
    int getFileSize(const char *filepath, bool fromTF = false);
   // void loadwenjian(const String path);
    const char* getFileName(const char* filePath);
    const char* combinePath(const char* directory, const char* fileName);
    const char* remove_path_prefix(const char* path, const char* prefix);
    void setup();
    const char* get_houzhui(const char* filename);
    void openfile();
    void selctwenjianjia();
    void uint8tobuf(uint8_t *input,int inputSize,char *output);
    //const char* combineFilePath(const char* path, const char* filename, const char* extension);
    bool wenjianend = false;
};
static Appwenjian wenjian;

const char *directoryname;
time_t LastWrite_time = 0;
String toApp = "";
bool hasToApp = false;

int Appwenjian::getFileSize(const char* filePath, bool fromTF)
{
    File file;
    int fileSize = 0;
    
    if (fromTF == false)
        file = LittleFS.open(remove_path_prefix(filePath,"/littlefs"));
    else
        file = SD.open(remove_path_prefix(filePath,"/sd"));
    
    if (!file)
    {
        //Serial.println("[文件管理] 无法打开文件");
        LOG("\033[31m无法打开文件%s\033[32m\n",filePath);
        F_LOG("无法打开文件%s\n",filePath);
        return 0;
    }
    LastWrite_time = file.getLastWrite();
    fileSize = file.size();
    
    file.close();
    LOG("filename:%s\n",filePath);
    LOG("size:%dBytes\n",fileSize);
    return fileSize;
}

/*
void AppInstaller::loadApp(const String path) // 加载TF卡App
{
    String filename = path.substring(path.lastIndexOf('/') + 1);
    app_lua.initialize(filename, path);
    app_lua.init();
    app_lua.peripherals_requested = PERIPHERALS_SD_BIT;
    app_lua._showInList = false;
    app_lua._reentrant = false;
    appManager.gotoApp(&app_lua);
}*/

void Appwenjian::setup() 
{
    fanhui:
    filename = GUI::fileDialog("文件管理", false, NULL, NULL);
    float a;
    if (strncmp(filename, "/sd/", 4) == 0) {
        a = (float)getFileSize(filename,true);
    } 
    else if (strncmp(filename, "/littlefs/", 10) == 0) {
        a = getFileSize(filename,false);
    }
    struct tm *filetimeinfo; 
    filetimeinfo = localtime(&LastWrite_time);
    char Str[128];
    if (a <= 1024){
        sprintf(Str, "大小 %dBytes %d.%d.%d %d:%d", (int)a, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min); 
    }else if (a <= 1024 * 1024){
        sprintf(Str, "大小 %.2fKB %d.%d.%d %d:%d", a / 1024.0, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min);
    }else if (a <= 1024 * 1024 * 1024){
        sprintf(Str, "大小 %.2fMB %d.%d.%d %d:%d", a / 1024.0 / 1024.0, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min);
    }else{
        sprintf(Str, "大小 %.2fGB %d.%d.%d %d:%d", a / 1024.0 / 1024.0 / 1024.0, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min);
    }
    char buf[64];
    static const menu_item appMenu_main[] = {
    {NULL, "返回"},
    {NULL, "新建"},
    {NULL, "复制文件"},
    {NULL, "重命名"},
    {NULL, "删除"},
    {NULL, Str},
    {NULL, "打开"},
    {NULL, "退出"},
    {NULL, buf},
    {NULL, NULL},
    };
    int res = 0;
    while (hasToApp == false)
    {
        sprintf(buf,"文件系统:%d/%d|%dkB",LittleFS.usedBytes()/1024, LittleFS.totalBytes() / 1024,(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
        int res = GUI::menu(filename, appMenu_main);
        switch (res)
        {
        case 0:
            goto fanhui;
            break;
        case 1:
            {
                const char* newfile;
                bool ok = false;
                if(GUI::msgbox_yn("文件管理","新建","文件夹","文件"))
                {
                    newfile = GUI::englishInput("输入路径，例如/testing/");
                    if(GUI::msgbox_yn("文件管理","新建文件夹到","littlefs","sd"))
                    {
                        ok = LittleFS.mkdir(newfile);
                    }
                    else{
                        ok = SD.mkdir(newfile);
                    }
                    if (!ok)
                    {GUI::msgbox("文件管理器","无法创建文件夹");
                    F_LOG("无法创建文件夹");
                    }
                }else{
                    newfile = GUI::englishInput("输入路径，例如/testing.txt");
                    File f;
                    if(GUI::msgbox_yn("文件管理","新建文件到","littlefs","sd"))
                    {
                        f = LittleFS.open(newfile,"w");
                        f.close();
                    }
                    else{
                        f = SD.open(newfile,"w");
                        f.close();
                    }
                }
                delete[] newfile;
            }
            break;
        case 2:
            {  
            if(LittleFS.exists("/userdat") == false){LittleFS.mkdir("/userdat");}
            if(SD.exists("/userdat") == false){SD.mkdir("/userdat");}
            selctwenjianjia();
            File newfile,file;
            if (strncmp(filename, "/sd/", 4) == 0) {
                newfile = LittleFS.open(combinePath(directoryname,getFileName(filename)),"w");
                file = SD.open(remove_path_prefix(filename,"/sd"));
                if (!file)
                {
                   //Serial.println("[文件管理]file无法打开文件");
                   LOG("\033[31m无法打开文件%s\033[32m\n",filename);
                   F_LOG("无法打开文件%s",filename);
                   break;
                }
                if (!newfile)
                {
                   //Serial.println("[文件管理]newfile 无法打开文件");
                   LOG("\033[31m无法打开文件%s\033[32m\n",combinePath(directoryname,getFileName(filename)));
                   F_LOG("无法打开文件%s",combinePath(directoryname,getFileName(filename)));
                   break;
                }
                if(file.size() > LittleFS.totalBytes() - LittleFS.usedBytes())
                {
                    GUI::msgbox("警告","littlefs剩余的空间不足以复制当前的文件,自动取消当前复制!");
                    newfile.close();
                    file.close();
                    LittleFS.remove(combinePath(directoryname,getFileName(filename)));
                    break;
                }
                display.clearScreen();
                u8g2Fonts.setCursor(1,35);
                unsigned int a = millis();
                hal.copy(newfile,file);
                u8g2Fonts.printf("耗时:%dms",millis()-a);
                display.display(true);
            } 
            else if (strncmp(filename, "/littlefs/", 10) == 0) {
                newfile = SD.open(combinePath(directoryname,getFileName(filename)),"w");
                file = LittleFS.open(remove_path_prefix(filename,"/littlefs"));
                if (!file)
                {
                   //Serial.println("[文件管理]file无法打开文件");
                   LOG("\033[31m无法打开文件%s\033[32m\n",filename);
                   F_LOG("无法打开文件%s",filename);
                }
                if (!newfile)
                {
                   //Serial.println("[文件管理]newfile 无法打开文件");
                   LOG("\033[31m无法打开文件%s\033[32m\n",combinePath(directoryname,getFileName(filename)));
                   F_LOG("无法打开文件%s",combinePath(directoryname,getFileName(filename)));
                }
                display.clearScreen();
                u8g2Fonts.setCursor(1,35);
                unsigned int a = millis();
                hal.copy(newfile,file);
                u8g2Fonts.printf("耗时:%dms",(millis()-a));
                display.display(true);
            }
            newfile.close();
            file.close();
            }
            break;
        case 3:
            {
                const char *newname;
                bool ok = false;
                if (strncmp(filename, "/sd/", 4) == 0)
                {
                    newname = GUI::englishInput(remove_path_prefix(filename,"/sd"));
                    ok = SD.rename(remove_path_prefix(filename,"/sd"),newname);
                }
                else if (strncmp(filename, "/littlefs/", 10) == 0) 
                {
                    newname = GUI::englishInput(remove_path_prefix(filename,"/littlefs"));
                    ok = LittleFS.rename(remove_path_prefix(filename,"/littlefs"),newname);
                }
                delete[] newname;
            }
            break;
        case 4:
            {  
                bool OK = false;
            if(GUI::msgbox_yn("提示","删除的为","文件夹","文件") == false)
            {
                if(GUI::msgbox_yn("提示","确定删除") == false)
                {
                    break;
                }else{
                    if (strncmp(filename, "/sd/", 4) == 0) {
                        OK = SD.remove(remove_path_prefix(filename,"/sd"));
                    } 
                    else if (strncmp(filename, "/littlefs/", 10) == 0) {
                        OK = LittleFS.remove(remove_path_prefix(filename,"/littlefs"));
                    }
                }
            }else{
                selctwenjianjia();
                if(GUI::msgbox_yn("提示","确定删除") == false)
                {
                    break;
                }else{
                    if (strncmp(filename, "/sd/", 4) == 0) {
                        OK = SD.rmdir(directoryname);
                    } 
                    else if (strncmp(filename, "/littlefs/", 10) == 0) {
                        OK = LittleFS.rmdir(directoryname);
                    }
                }
            }
            if(!OK)
            {
                GUI::msgbox("错误","删除失败!");
                LOG("\033[33mremove %s error\033[32m\n",filename);
                F_LOG("file remove %s error",filename);
            }else{
                LOG("\033[33mremove %s\033[32m\n",filename);}
            }
            break;
        case 5:
            { 
            int a;
            if (strncmp(filename, "/sd/", 4) == 0) {
                a = getFileSize(filename,true);
            } 
            else if (strncmp(filename, "/littlefs/", 10) == 0) {
                a = getFileSize(filename,false);
            } 
            char Str[20];
            sprintf(Str, "%d Bytes(%dKB)", a, a/1024);  
            GUI::msgbox("文件大小",Str);
            }
            break;
        case 6:
            openfile();
            break;
        case 7:
            appManager.goBack(); 
            return;
            break;
        case 8:
            break;
        default:
            break;
        }
    }
    if (hasToApp == true)
    {
        hasToApp = false;
        if (toApp != "")
        {
            appManager.gotoApp(toApp.c_str());
        }
        return;
    }
}


char fullPath[300];

const char* Appwenjian::getFileName(const char* filePath) {
  // 找到最后一个斜杠的位置
  const char* lastSlash = strrchr(filePath, '/');

  // 如果找到了斜杠
  if (lastSlash != NULL) {
    // 返回斜杠之后的字符串作为文件名
    return lastSlash + 1;
  } else {
    // 如果没有找到斜杠，则整个路径都是文件名
    return filePath;
  }
}

const char* Appwenjian::combinePath(const char* directory, const char* fileName) {
// 计算所需的缓冲区大小（目录长度 + 文件名长度 + 斜杠 + 结束符）
  size_t directoryLen = strlen(directory);
  /*size_t fileNameLen = strlen(fileName);
  size_t bufferSize = directoryLen + fileNameLen + 2; // 2 是斜杠和结束符

  // 创建缓冲区
  char fullPath[bufferSize];*/
  // 复制目录到缓冲区
  strcpy(fullPath, directory);

  // 如果目录不以斜杠结尾，则添加斜杠
  if (directory[directoryLen - 1] != '/') {
    strcat(fullPath, "/");
  }

  // 添加文件名到缓冲区
  strcat(fullPath, fileName);

  // 返回完整路径
  return fullPath;
}

const char* Appwenjian::remove_path_prefix(const char* path, const char* prefix) {
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

const char* Appwenjian::get_houzhui(const char* filename) {
    const char* dot = strrchr(filename, '.'); // 找到最后一个 '.' 的位置
    if (!dot || dot == filename) { // 如果找不到 '.' 或者 '.' 是第一个字符
        return nullptr; // 没有后缀
    }
    return dot + 1; // 返回 '.' 后面的字符串，即后缀
}

void Appwenjian::openfile()
{
    LOG("openfile,filename:%s\n",filename);
    const char* houzhui = get_houzhui(filename);
    if(strcmp(houzhui, "txt") == 0 || strcmp(houzhui, "TXT") == 0)
    {
        if(GUI::msgbox_yn("提示","使用默认的文本查看器打开？否则使用bin文件查看器打开")){
            if(GUI::msgbox_yn("提示","将会覆盖原有记录的文件名"))
            {
                hal.pref.putBytes(SETTINGS_PARAM_LAST_EBOOK, filename, strlen(filename));
                hasToApp = true;
                toApp = "ebook";
                appManager.gotoApp(toApp.c_str());
            }
        }else{
            openbin();
        }    
    }
    else if(strcmp(houzhui, "buz") == 0)
    {
        buzzer.playFile(filename);
        display.display(false); // 全局刷新一次
        while (!hal.btnl.isPressing() && !hal.btnr.isPressing() && !hal.btnc.isPressing() && buzzer.hasNote())
        {
            delay(100);
        }
        if (buzzer.hasNote())
        {
            buzzer.forceStop();
        }
        GUI::msgbox("提示","播放已停止");
    }
    else if(strcmp(houzhui, "bin") == 0)
    {
        openbin();
    }
    else if(strcmp(houzhui, "lbm") == 0)
    {
        /*
        File file;
        if (strncmp(filename, "/sd/", 4) == 0) {
            file = SD.open(remove_path_prefix(filename,"/sd"),"r");
        } 
        else if (strncmp(filename, "/littlefs/", 10) == 0) {
            file = LittleFS.open(remove_path_prefix(filename,"/littlefs"),"r");
        }*/
        FILE *fp = fopen(getRealPath(filename), "rb");
        uint16_t w, h;
        fread(&w, 2, 1, fp);
        fread(&h, 2, 1, fp);
        fclose(fp);
        display.clearScreen();
        GUI::drawLBM((296 - w) / 2,(128 - h) / 2,filename, GxEPD_BLACK);
        display.display();
        while (1)
        {
            if(digitalRead(PIN_BUTTONR) == 1)
            {
                appManager.noDeepSleep = false;
                hal.powerOff(false);
                esp_deep_sleep_start();
            }
            if(digitalRead(PIN_BUTTONC) == 1)
            {
                break;
            }
            delay(100);
        }
    }
    else if(strcmp(houzhui, "bmp") == 0 || strcmp(houzhui, "BMP") == 0)
    {
        display.clearScreen();
        if (strncmp(filename, "/sd/", 4) == 0) {
            GUI::drawBMP(&SD,remove_path_prefix(filename,"/sd"),false);
        } 
        else if (strncmp(filename, "/littlefs/", 10) == 0) {
            GUI::drawBMP(&LittleFS,remove_path_prefix(filename,"/littlefs"),false);
        }
        while (1)
        {
            if(hal.btnr.isPressing())
            {
                appManager.noDeepSleep = false;
                hal.powerOff(false);
                esp_deep_sleep_start();
            }
            if(hal.btnc.isPressing())
            {
                break;
            }
            delay(100);
        }
    }else if(strcmp(houzhui, "JPG") == 0 || strcmp(houzhui, "jpg") == 0){
        display.clearScreen();
        if (strncmp(filename, "/sd/", 4) == 0) {
            GUI::drawJPG(remove_path_prefix(filename,"/sd"), SD);
        } 
        else if (strncmp(filename, "/littlefs/", 10) == 0) {
            GUI::drawJPG(remove_path_prefix(filename,"/littlefs"), LittleFS);
        }
        while (1)
        {
            if(hal.btnr.isPressing())
            {
                appManager.noDeepSleep = false;
                hal.powerOff(false);
                esp_deep_sleep_start();
            }
            if(hal.btnc.isPressing())
            {
                break;
            }
            delay(100);
        }
    }else {
        GUI::msgbox("提示","文件格式没有支持的显示或处理方式，使用16进制(bin)模式打开");
        openbin();
    }
}

void Appwenjian::selctwenjianjia()
{

    directorylist.clear();
    File root, file;
    if (strncmp(filename, "/sd/", 4) == 0) {
        root = LittleFS.open("/");
        if (!root)
        {
            LOG("\033[33mroot未打开\033[32m\n");
        }
        file = root.openNextFile();
    } 
    else if (strncmp(filename, "/littlefs/", 10) == 0) {
        root = SD.open("/");
        if (!root)
        {
            LOG("\033[33mroot未打开\033[32m\n");
        }
        file = root.openNextFile();
    }

    while (file)
    {
        String name = file.name();
        if (file.isDirectory())
        {
            directorylist.push_back(file.name());
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    menu_item *fileList = new menu_item[directorylist.size() + 2];
    fileList[0].title = "使用默认";
    fileList[0].icon = NULL;
    int i = 1;
    std::list<String>::iterator it;
    for (it = directorylist.begin(); it != directorylist.end(); ++it)
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
        directoryname = "/userdat/";
    }
    else
    {
        /*static char result[256]; 
        strcat(result, "/");
        strcpy(result, fileList[appIdx].title); 
        strcat(result, "/");
        directoryname = result;
        Serial.print(directoryname);*/



        /*std::string original(fileList[appIdx].title);
        std::string modified = "/" + original + "/";
        char* result = new char[modified.length() + 1];
        std::strcpy_s(result, modified.c_str());
        directoryname = result;
        delete[] result;*/

        size_t originalLength = strlen(fileList[appIdx].title);
        size_t newLength = originalLength + 2; // 为两边的 '/' 预留空间
        char* newString = new char[newLength + 1]; // +1 为结尾的 '\0'

        newString[0] = '/'; // 开始处添加 '/'
        strcpy(newString + 1, fileList[appIdx].title); // 复制原字符串
        newString[newLength - 1] = '/'; // 结束处添加 '/'
        newString[newLength] = '\0'; // 终止符
        directoryname = newString;
        delete[] newString;
    }
}

void Appwenjian::uint8tobuf(uint8_t *input,int inputSize,char *output)
{
    for(int i = 0;i < inputSize;i++)
    {
        sprintf(output + (i * 3),"%02x ",input[i]);
    }
}
