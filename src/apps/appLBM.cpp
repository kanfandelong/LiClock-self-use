#include "AppManager.h"

extern RTC_DATA_ATTR const char *filename; //保存从文件选择的完整文件名
extern RTC_DATA_ATTR char buf[];

class AppLBM : public AppBase
{
private:
    /* data */
public:
    AppLBM()
    {
        name = "lbmshow";
        title = "图片";
        description = "模板";
        image = NULL;
    }
    void set();
    void setup();
    String _dir;                     // 当前目录
    char *titles[256];              // 图片文件名内存指针数组
    int file_cont = 0;
};
static AppLBM app;
void AppLBM::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), false);
}
void AppLBM::setup(){
    if (hal.btnl.isPressing()) {
        appManager.goBack();
        return;
    }
    if (filename == NULL || hal.btnr.isPressing()) 
        filename = GUI::fileDialog(title, false, "lbm");
    sprintf(buf,"%s",filename);     //将filename指向的数据拷贝到buf
    filename = buf;                 //将filename指向到buf
    String _filename;
    bool is_root = false;
    bool in_littlefs = false;
    if (strncmp(filename, "/sd/", 4) == 0) {
        _filename = remove_path_prefix(filename,"/sd");
        in_littlefs = false;
    } 
    else if (strncmp(filename, "/littlefs/", 10) == 0) {
        _filename = remove_path_prefix(filename,"/littlefs");
        in_littlefs = true;
    }
    int lastSlash = _filename.lastIndexOf('/');
    _dir = _filename.substring(0, lastSlash);
    if (_dir == ""){
        is_root = true;
        _dir = "/";
    }
    uint16_t song_count = 0;
    File root;
    if (is_root){
        if (!in_littlefs)
            root = SD.open("/");
        else
            root = LittleFS.open("/");
    }
    else{
        if (!in_littlefs)
            root = SD.open(_dir);
        else
            root = LittleFS.open(_dir);
    }
    File dir = root.openNextFile();
    while (dir)
    {
        if (!dir.isDirectory() && String(dir.name()).endsWith(".lbm")) {
            song_count++;
        }
        dir.close();
        dir = root.openNextFile();
    }
    dir.close();
    root.close();
    int i = 0;
    while (titles[i] != NULL)
    {
        free(titles[i]);
        titles[i] = NULL;   
        ++i;
    }
    if (is_root){
        if (!in_littlefs)
            root = SD.open("/");
        else
            root = LittleFS.open("/");
    }
    else{
        if (!in_littlefs)
            root = SD.open(_dir);
        else
            root = LittleFS.open(_dir);
    }
    dir = root.openNextFile();
    file_cont = song_count;
    memset(titles, 0, sizeof(titles));
    i = 0;
    while (dir)
    {
        if (!dir.isDirectory() && String(dir.name()).endsWith(".lbm")) {
            titles[i] = (char *)malloc(strlen(dir.name()) + 1);
            strcpy(titles[i], dir.name());
            i++;
            Serial.printf("%s\n", dir.name());
        }
        dir.close();
        dir = root.openNextFile();
    }
    dir.close();
    root.close();
    int index = random(0, file_cont - 1);
/*     if (index == file_cont)
        index = 0;
    if (index < 0)
        index = 0; */
    String ibm_name;
    if (in_littlefs){
        if (_dir == "/")
            ibm_name = "/littlefs" + _dir + titles[index];
        else 
            ibm_name = "/littlefs" + _dir + "/" + titles[index];
    }
    else{
        if (_dir == "/")
            ibm_name = "/sd" + _dir + titles[index];
        else 
            ibm_name = "/sd" + _dir + "/" + titles[index];
    }

    FILE *fp = fopen(ibm_name.c_str(), "rb");
    uint16_t w, h;
    HEADGRAY header;
    /* fread(&w, 2, 1, fp);
    fread(&h, 2, 1, fp);
    fread(&grayLevels, 2, 1, fp); */
    fread(&header, sizeof(header), 1, fp);
    w = header.w;
    h = header.h;
    fclose(fp);
    display.fillScreen(GxEPD_WHITE);
    display.display();
    GUI::drawLBM((296 - w) / 2,(128 - h) / 2, ibm_name.c_str(), GxEPD_BLACK);
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 301 - hal.timeinfo.tm_sec;
}