#include "AppManager.h"

static const uint8_t wenjian_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01, 0x00,
   0xfe, 0xff, 0x03, 0x00, 0x06, 0x00, 0xff, 0x3f, 0xfe, 0xff, 0xff, 0x7f,
   0xfe, 0xff, 0x01, 0x60, 0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60,
   0x06, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x60, 0x06, 0xff, 0xff, 0x60,
   0x86, 0xff, 0xff, 0x61, 0x86, 0x01, 0x80, 0x61, 0x86, 0x01, 0x80, 0x61,
   0x86, 0x01, 0x80, 0x61, 0x86, 0xf9, 0x8f, 0x61, 0x86, 0xf9, 0x9f, 0x61,
   0x86, 0x19, 0x98, 0x61, 0x86, 0x19, 0x98, 0x61, 0x86, 0x19, 0x98, 0x61,
   0x86, 0x19, 0x98, 0x61, 0xfe, 0xf9, 0x9f, 0x7f, 0xfc, 0xfd, 0xdf, 0x3f,
   0x80, 0x0f, 0xf8, 0x00, 0x00, 0x07, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00,
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


const char *filepath; //保存文件夹
RTC_DATA_ATTR const char *filename = NULL; //保存从文件选择的完整文件名
extern RTC_DATA_ATTR char buf[];


class Appwenjian : public AppBase
{
private:
    /**
     * 16进制文件查看器，页绘制函数
     * @param file 文件对象
     * @param page 页码
     */
    void displayPage(File &file, int page) {
        // UTF-8 解析相关变量
        uint8_t utf8_remaining = 0;       // 剩余待读取字节数
        uint16_t utf8_start_x = 0;        // UTF-8字符起始X坐标
        uint16_t utf8_start_y = 0;        // UTF-8字符起始Y坐标
        uint8_t utf8_buffer[5] = {0};     // UTF-8缓冲区（4字节+终止符）
        uint8_t utf8_index = 0;
        bool is_utf8 = false;
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
                    //HEX值打印
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
                    // ASCII/UTF-8处理
                    if (!is_utf8) {
                        if ((data & 0x80) == 0x00) { // ASCII
                            u8g2Fonts.setCursor(225 + 8 * a, u8g2Fonts.getCursorY());
                            if (data != '\n')
                                u8g2Fonts.printf("%c", data);
                        } else { // UTF-8头字节
                            // UTF-8头字节
                            is_utf8 = true;
                            utf8_remaining = 
                                (data & 0xF0) == 0xE0 ? 2 :  // 3字节需要2后续
                                (data & 0xE0) == 0xC0 ? 1 :  // 2字节需要1后续
                                (data & 0xF8) == 0xF0 ? 3 : 0; // 4字节需要3后续
                            utf8_buffer[0] = data;
                            utf8_index = 1;  // 关键修复：从索引1开始存储后续字节
                            utf8_start_x = 225 + 8 * a;
                            utf8_start_y = u8g2Fonts.getCursorY();
                            // 非法头字节立即恢复
                            if (utf8_remaining == 0) 
                                is_utf8 = false;
                        }
                    } else { // 收集UTF-8后续字节
                        // 验证后续字节格式
                        if ((data & 0xC0) != 0x80) { 
                            // 非法字节，终止解析并显示错误符号
                            is_utf8 = false;
                            utf8_remaining = 0;
                            utf8_index = 0;
                        }
                        else {
                            if (utf8_index < sizeof(utf8_buffer)-1) { // 防止溢出
                                utf8_buffer[utf8_index++] = data;
                            }
                            utf8_remaining--;
                            
                            if (utf8_remaining <= 0) {
                                utf8_buffer[utf8_index] = '\0'; // 正确终止符位置
                                
                                // 保存当前光标
                                int savedX = u8g2Fonts.getCursorX();
                                int savedY = u8g2Fonts.getCursorY();
                                
                                // 显示UTF-8字符
                                u8g2Fonts.setCursor(utf8_start_x, utf8_start_y);
                                u8g2Fonts.print((char*)utf8_buffer);
                                
                                // 恢复原始光标
                                u8g2Fonts.setCursor(savedX, savedY);
                                
                                // 重置状态
                                is_utf8 = false;
                                utf8_index = 0;
                            }
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
    /**
     * 16进制文件查看器
     * @note 函数使用全局变量 filename 获取处理的文件
     */
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
        unsigned long wait_time = millis();
        while (!end)
        {
            if (hal.btnr.isPressing()) {
                currentPage++;
                if (currentPage >= totalPages) {
                    currentPage = totalPages - 1;
                }
                displayPage(file, currentPage);
                wait_time = millis();
            }
            if (hal.btnl.isPressing()) {
                currentPage--;
                if (currentPage < 0) {
                    currentPage = 0;
                }
                displayPage(file, currentPage);
                wait_time = millis();
            }if (hal.btnc.isPressing()) {
                static const menu_item appMenu_main[] = {
                    {NULL, "返回"},
                    {NULL, "退出"},
                    {NULL, "跳转页"},
                    {NULL, "跳转至偏移量"},
                    {NULL, "跳转至HEX偏移量"},
                    {NULL, NULL},
                };
                char *buf = (char *)malloc(128);
                sprintf(buf, "当前页：%d/%d", currentPage + 1, totalPages);
                int res = GUI::menu(buf,appMenu_main);
                free(buf);
                switch (res)
                {
                    case 0:
                        display.display(true);
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
                        break;
                    case 3:
                        {
                            int offset = GUI::msgbox_number("跳转至偏移量", 8, (currentPage * BYTES_PER_PAGE));
                            currentPage = offset / BYTES_PER_PAGE;
                            if (offset % BYTES_PER_PAGE)
                                currentPage++;
                            if (currentPage < 0) {
                                currentPage = 0;
                            }if (currentPage >= totalPages) {
                                currentPage = totalPages - 1;
                            }
                            displayPage(file, currentPage);
                        }
                        break;
                    case 4:
                        {
                            uint32_t offset = GUI::msgbox_hex("跳转至偏移量", 8, currentPage * BYTES_PER_PAGE);
                            currentPage = offset / BYTES_PER_PAGE;
                            if (offset % BYTES_PER_PAGE)
                                currentPage++;
                            if (currentPage < 0) {
                                currentPage = 0;
                            }if (currentPage >= totalPages) {
                                currentPage = totalPages - 1;
                            }
                            displayPage(file, currentPage);
                        }
                        break;
                    default:    
                        GUI::msgbox("提示", "无效操作");
                        break;
                }
                wait_time = millis();
            }if (end){
                break;
            }
            delay(100);  // 避免按钮抖动
            if (millis() - wait_time > 30000)
                hal.wait_input();
        }
        file.close();
    }
public:
    Appwenjian()
    {
        name = "wenjian";
        title = "文件管理";
        description = "文件管理器";
        image = wenjian_bits;
        peripherals_requested = PERIPHERALS_SD_BIT;
        _showInList = true;
    }
    void set();
    std::list<String> directorylist;
    int getFileSize(const char *filepath, bool fromTF = false);
   // void loadwenjian(const String path);
    const char* getFileName(const char* filePath);
    const char* combinePath(const char* directory, const char* fileName);
    const char* remove_path_prefix(const char* path, const char* prefix);
    const char* getDirectoryPath(const char* filePath);
    void setup();
    const char* get_houzhui(const char* filename);
    void openfile();
    void selctwenjianjia(bool _file = false);
    void uint8tobuf(uint8_t *input,int inputSize,char *output);
    // const char* combineFilePath(const char* path, const char* filename, const char* extension);
    const char *directoryname;
    time_t LastWrite_time = 0;
    String toApp = "";
    bool hasToApp = false;
};
static Appwenjian wenjian;


void Appwenjian::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
/**
 * 获取指定文件大小
 * @param filePath 文件路径
 * @param fromTF 文件是否在TF卡
 * @return 文件大小（字节）
 */
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
    filepath = getDirectoryPath(filePath);
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
    char char_buf[64];
    int used = 0, total = 0, free = 0;
    String _filename, dir;
    u32_t a;
    const char *file_system;
    bool run_first = true;
    if (filename != NULL)
        run_first = false;
    if (run_first){
fanhui:
        filename = GUI::fileDialog("文件管理", false, NULL, NULL);
        sprintf(buf,"%s",filename);     //将filename指向的数据拷贝到buf
        filename = buf;                 //将filename指向到buf
    }
    GUI::info_msgbox("提示", "获取文件系统信息...");
    used = LittleFS.usedBytes()/1024;
    total = LittleFS.totalBytes()/1024;
    free = total - used;
    sprintf(char_buf,"文件系统:%d/%d|剩余%dkB",used ,total , free);
    if (filename == NULL)
    {
        goto fanhui;
    }
    // GUI::info_msgbox("提示", "正在获取文件信息...");
file_info:
    if (strncmp(filename, "/sd/", 4) == 0) {
        a = getFileSize(filename,true);
        file_system = "TF";
        _filename = remove_path_prefix(filename,"/sd");
    } 
    else if (strncmp(filename, "/littlefs/", 10) == 0) {
        a = getFileSize(filename,false);
        file_system = "LittleFS";
        _filename = remove_path_prefix(filename,"/littlefs");
    }
    int lastSlash = _filename.lastIndexOf('/');
    dir = _filename.substring(0, lastSlash);
    if (dir == "")
        dir = "/";
    struct tm *filetimeinfo; 
    filetimeinfo = localtime(&LastWrite_time);
    char Str[128];
    if (a <= 1024){
        sprintf(Str, "大小 %dBytes %d.%d.%d %d:%d", (int)a, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min); 
    }else if (a <= 1048576){
        sprintf(Str, "大小 %.2fKB %d.%d.%d %d:%d", (float)a / 1024.0, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min);
    }else if (a <= 1073741824){
        sprintf(Str, "大小 %.2fMB %d.%d.%d %d:%d", (float)a / 1048576.0, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min);
    }else{
        sprintf(Str, "大小 %.2fGB %d.%d.%d %d:%d", (float)a / 1073741824.0, filetimeinfo->tm_year + 1900,filetimeinfo->tm_mon + 1, filetimeinfo->tm_mday, filetimeinfo->tm_hour, filetimeinfo->tm_min);
    }
    static const menu_item appMenu_main[] = {
    {NULL, "返回"},
    {NULL, "新建"},
    {NULL, "复制文件"},
    {NULL, "重命名"},
    {NULL, "删除"},
    {NULL, Str},
    {NULL, "打开"},
    {NULL, "切换文件系统"},
    {NULL, "退出"},
    {NULL, char_buf},
    {NULL, NULL},
    };
    int res = 0;
    while (hasToApp == false)
    {
        int res = GUI::menu(filename, appMenu_main);
        switch (res)
        {
        case 0:
            {
                filename = GUI::fileDialog("文件管理", false, NULL, NULL, dir, file_system);
                if (filename == NULL)
                {
                    goto fanhui;
                }
                sprintf(buf,"%s",filename);     //将filename指向的数据拷贝到buf
                filename = buf;                 //将filename指向到buf
                used = LittleFS.usedBytes()/1024;
                free = total - used;
                sprintf(char_buf,"文件系统:%d/%d|剩余%dkB",used ,total , free);
                goto file_info;
            }
            break;
        case 1:
            {
                const char* newfile;
                bool ok = false;
                if(GUI::msgbox_yn("文件管理","新建","文件夹","文件"))
                {
                    newfile = GUI::englishInput("输入路径，例如：/testing");
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
                    newfile = GUI::englishInput("输入路径，例如：/testing.txt");
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
                float filesize = (float)file.size() / 1024.0;
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
                unsigned long begin = millis();
                if(!hal.copy(newfile,file)){
                    GUI::msgbox("提示","复制失败!");
                    LittleFS.remove(combinePath(directoryname,getFileName(filename)));
                }else{
                    unsigned long usetime = millis() - begin;
                    char buf[512];
                    sprintf(buf,"从TF卡复制 %s 到littlefs,\n耗时:%0.1f S\n速度:%0.2f KB/S", getFileName(filename), (float)usetime / 1000.0, filesize / ((float)usetime / 1000.0));
                    GUI::info_msgbox("提示",buf);
                    delay(1500);
                }
            } 
            else if (strncmp(filename, "/littlefs/", 10) == 0) {
                newfile = SD.open(combinePath(directoryname,getFileName(filename)),"w");
                file = LittleFS.open(remove_path_prefix(filename,"/littlefs"));
                float filesize = (float)file.size() / 1024.0;
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
                unsigned long begin = millis();
                if(!hal.copy(newfile,file)){
                    GUI::msgbox("提示","复制失败!");
                    SD.remove(combinePath(directoryname,getFileName(filename)));
                }else{
                    unsigned long usetime = millis() - begin;
                    char buf[512];
                    sprintf(buf,"从littlefs复制 %s 到TF卡,\n耗时:%0.1f S\n速度:%0.2f KB/S", getFileName(filename), (float)usetime / 1000.0, filesize / ((float)usetime / 1000.0));
                    GUI::info_msgbox("提示",buf);
                    delay(1500);
                }
            }
            //newfile.close();
            //file.close();
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
                char info[256];
                if(GUI::msgbox_yn("提示","删除的为","文件夹","文件") == false)
                {
                    if(GUI::msgbox_yn("提示","确定删除") == false)
                    {
                        break;
                    }else{
                        if(GUI::msgbox_yn("提示","确定删除", "取消", "确定") == false){
                            if (strncmp(filename, "/sd/", 4) == 0) {
                                OK = SD.remove(remove_path_prefix(filename,"/sd"));
                            } 
                            else if (strncmp(filename, "/littlefs/", 10) == 0) {
                                OK = LittleFS.remove(remove_path_prefix(filename,"/littlefs"));
                            }
                            if(OK){
                                sprintf(info,"成功删除%s",filename);
                                GUI::msgbox("提示", info);
                            }else{
                                sprintf(info,"无法删除%s",filename);
                                GUI::msgbox("提示", info);
                            }
                        }
                    }
                }else{
                    selctwenjianjia(true);
                    if(GUI::msgbox_yn("提示","确定删除") == false)
                    {
                        break;
                    }else{
                        if(GUI::msgbox_yn("提示","确定删除", "取消", "确定") == false){
                            if (strncmp(filename, "/sd/", 4) == 0) {
                                //OK = SD.rmdir(directoryname);
                                String dirname = "/sd" + String(directoryname);
                                dirname[dirname.length() - 1] = '\0';
                                hal.rm_rf(dirname.c_str());
                            } else if (strncmp(filename, "/littlefs/", 10) == 0) {
                                //OK = LittleFS.rmdir(directoryname);
                                String dirname = "/littlefs" + String(directoryname);
                                dirname[dirname.length() - 1] = '\0';
                                hal.rm_rf(dirname.c_str());
                            }
                        }
                    }
                }
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
            {
                if(GUI::msgbox_yn("提示","使用默认的方式打开文件？否则使用bin文件查看器(hex)打开", "默认方式", "hex"))
                    openfile();
                else
                    openbin(); 
            }
            break;
        case 7:
            goto fanhui;
            break;
        case 8:
            appManager.goBack(); 
            return;
            break;
        case 9:
            break;
        default:
            GUI::msgbox("提示", "无效的选项");
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
/**
 * 文件名获取函数
 * @param filePath 完整文件路径
 * @return 文件名
 */
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
/**
 * 文件路径拼接函数
 * @param directory 目录路径
 * @param fileName 文件名
 * @return 完整文件路径
 */
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
/**
 * 去除路径特定前缀函数
 * @param path 完整路径
 * @param prefix 特定前缀
 * @return 去除前缀后的路径
 */
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
/**
 * 获取目录路径函数】
 * @param filePath 完整文件路径
 * @return 目录路径
 */
const char* Appwenjian::getDirectoryPath(const char* filePath) {
    // 找到最后一个斜杠的位置
    const char* lastSlash = strrchr(filePath, '/');
    if (lastSlash != nullptr) {
        // 计算目录路径的长度
        size_t dirLength = lastSlash - filePath;
        // 分配内存存储目录路径
        char* dirPath = new char[dirLength + 1];
        strncpy(dirPath, filePath, dirLength);
        dirPath[dirLength] = '\0';
        return dirPath;
    } else {
        // 如果没有找到斜杠，返回根目录或空字符串
        return "";
    }
}
/**
 * 获取文件后缀函数
 * @param filename 文件名
 * @return 文件后缀
 */
const char* Appwenjian::get_houzhui(const char* filename) {
    const char* dot = strrchr(filename, '.'); // 找到最后一个 '.' 的位置
    if (!dot || dot == filename) { // 如果找不到 '.' 或者 '.' 是第一个字符
        return nullptr; // 没有后缀
    }
    return dot + 1; // 返回 '.' 后面的字符串，即后缀
}
/**
 * 打开文件函数，将对应文件的处理或查看方式对应至对应的app
 */
void Appwenjian::openfile()
{
    LOG("openfile,filename:%s\n",filename);
    const char* houzhui = get_houzhui(filename);
    if(strcmp(houzhui, "txt") == 0 || strcmp(houzhui, "TXT") == 0)
    {
        if(GUI::msgbox_yn("提示","将会覆盖原有的历史纪录"))
        {
            hal.pref.putBytes(SETTINGS_PARAM_LAST_EBOOK, filename, strlen(filename));
            hal.pref.putInt(SETTINGS_PARAM_LAST_EBOOK_PAGE, 0);
            hasToApp = true;
            toApp = "ebook";
            appManager.gotoApp(toApp.c_str());
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
        HEADGRAY header;
        /* fread(&w, 2, 1, fp);
        fread(&h, 2, 1, fp);
        fread(&grayLevels, 2, 1, fp); */
        fread(&header, sizeof(HEADGRAY), 1, fp);
        w = header.w;
        h = header.h;
        fclose(fp);
        display.fillScreen(GxEPD_WHITE);
        display.display();
        GUI::drawLBM((296 - w) / 2,(128 - h) / 2,filename, GxEPD_BLACK);
        while (1)
        {
            if(hal.btnr.isPressing())
            {
                while(hal.btnr.isPressing())
                    delay(10);
                hal.powerOff(false);
            }
            if(digitalRead(PIN_BUTTONC) == 1)
            {
                break;
            }
            hal.wait_input();
        }
        display.setgray(15);
    }
    else if(strcmp(houzhui, "bmp") == 0 || strcmp(houzhui, "BMP") == 0)
    {
        display.clearScreen();
        display.display();
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
                while(hal.btnr.isPressing())
                    delay(10);
                hal.powerOff(false);
            }
            if(hal.btnc.isPressing())
            {
                break;
            }
            hal.wait_input();
        }
    }else if(strcmp(houzhui, "JPG") == 0 || strcmp(houzhui, "jpg") == 0){
        display.clearScreen();
        display.display();
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
                while(hal.btnr.isPressing())
                    delay(10);
                hal.powerOff(false);
            }
            if(hal.btnc.isPressing())
            {
                break;
            }
            hal.wait_input();
        }
    }else if(strcmp(houzhui, "LUA") == 0 || strcmp(houzhui, "lua") == 0){
        setPath(filepath);
        Serial.printf("pach:%s\n", filepath);
        String _str = "./" + (String)getFileName(filename);
        String _str2 = (String)filepath + "/conf.lua";
        const char* _file = _str.c_str();
        Serial.println("准备运行Lua脚本...");
        if (file_exist(_str2.c_str())){
            Serial.printf("存在配置文件%s，加载配置文件...\n", _str2.c_str());
            closeLua();
            openLua_simple();
            lua_pushinteger(L, 0);
            lua_setglobal(L, "peripherals_requested");
            lua_execute(_str2.c_str());
            lua_getglobal(L, "peripherals_requested");
            if (lua_isinteger(L, -1)){
                //peripherals_requested = lua_tointeger(L, -1);
                peripherals.load(lua_tointeger(L, -1));
            }
            lua_settop(L, 0);
            closeLua();
        }
        Serial.printf("目标运行脚本: %s\n", getRealPath(_file));
        closeLua();
        openLua();
        if (file_exist(getRealPath(_file)))
        {
            lua_execute(_file);
            
            lua_getglobal(L, "setup");
            if (lua_isfunction(L, -1))
            {
                lua_call(L, 0, 0);
            }
        }
        GUI::info_msgbox("提示", "lua脚本执行完毕", 136, 32);
        hal.wait_input();
    }else {
        GUI::msgbox("提示","文件格式没有支持的显示或处理方式，将使用16进制(bin)模式打开");
        openbin();
    }
}
/**
 * 文件夹选择菜单
 * @param _file 控制函数打开的文件系统
 * @note 使用全局变量 directoryname 返回选择的文件夹名称
 */
void Appwenjian::selctwenjianjia(bool _file)
{

    directorylist.clear();
    File root, file;
    if (_file){
        if (strncmp(filename, "/sd/", 4) == 0) {
            root = SD.open("/");
            if (!root)
            {
                LOG("\033[33mroot未打开\033[32m\n");
            }
            file = root.openNextFile();
        } 
        else if (strncmp(filename, "/littlefs/", 10) == 0) {
            root = LittleFS.open("/");
            if (!root)
            {
                LOG("\033[33mroot未打开\033[32m\n");
            }
            file = root.openNextFile();
        }
    }else
    {
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
    }
    GUI::info_msgbox("提示", "正在创建文件夹列表...");
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

    menu_item *fileList = new menu_item[directorylist.size() + 3];
    fileList[0].title = "使用默认";
    fileList[0].icon = NULL;
    fileList[1].title = "根目录";
    fileList[1].icon = NULL;
    int i = 2;
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
    }else if (appIdx == 1){
        delete fileList;
        directoryname = "/";
    }else
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
