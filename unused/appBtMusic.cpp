#include "AppManager.h"
#include "bt_music/btMusicBox.h"

btMusicBox *music;
extern bool _end;
extern bool gain;

static void bt_exit(){
    music->end();
    free(music);
}

class AppBtMusic : public AppBase
{
private:
    /* data */
public:
    AppBtMusic()
    {
        name = "BtMusic";
        title = "模板";
        description = "模板";
        image = NULL;
    }
    void set();
    void menu();
    void show_display();
    void setup();
};
static AppBtMusic app;

void AppBtMusic::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
static const menu_select menu_bt[] =
{
    {false,"返回"},
    {false,"退出"},
    {false,"断开连接"},
    {false,"重新连接"},
    {true, "设置音量"},
    {false,NULL},
};
void AppBtMusic::menu(){
    bool end = false;
    while (!end)
    {
        int res = GUI::select_menu("菜单", menu_bt);
        switch(res){
            case 0:
                end = true;
                break;
            case 1:
                end = true;
                _end = true;
                appManager.goBack();
                break;
            case 2:
                music->disconnect();
                break;
            case 3: 
                music->reconnect();
                break;
            case 4:
                gain = (float)GUI::msgbox_number("0-100", 3, gain * 100.0) / 100.0;
                if (gain > 1.0) {
                    gain = 1.0;
                }
                if (gain < 0.0) {
                    gain = 0.0;
                }
                music->volume(gain);
                break;
            default:
                GUI::info_msgbox("警告", "非法的输入值");
                break;
        }
    }
}
void AppBtMusic::show_display(){
    display.clearScreen();
    GUI::drawWindowsWithTitle("音乐播放器");
    u8g2Fonts.setCursor(3, 30);
    u8g2Fonts.printf("Gain：%.2f 电源电压：%dmV soc:%d%% soh:%d%%", gain, hal.VCC, hal.bat_info.soc, hal.bat_info.soh);
    u8g2Fonts.setCursor(3, 45);
    u8g2Fonts.printf("剩余堆内存：%.2fKB I:%dmA P:%dmW %dmAh", (float)ESP.getFreeHeap() / 1024.0, hal.bat_info.current.avg, hal.bat_info.power, hal.bat_info.capacity.remain);
    display.display(true);
}
void AppBtMusic::setup(){
    music = new btMusicBox("LiClock");
    music->begin();
    music->I2S();      
    hal.task_bat_info_update();
    gain = 0.95;
    exit = bt_exit;
    show_display();
    _end = false;
    int a = 0;
    int display_count = 0;
    while(!_end){
        if (hal.btnc.isPressing()){
            if (GUI::waitLongPress(PIN_BUTTONC)){
                menu();
                show_display();
                display_count++;
            }
            else {
                hal.task_bat_info_update();
                show_display();
                display_count++;
            }
        }
        if (hal.btnl.isPressing()) {
            gain = gain + 0.05;
            if (gain > 1.0) {
                gain = 1.0;
            }
            music->volume(gain);
        }
        if (hal.btnr.isPressing()) {
            gain = gain - 0.05;
            if (gain < 0.0) {
                gain = 0.0;
            }
            music->volume(gain);
        }
        if (a > 60) {
            a = 0;
            if (display_count > 15) {
                hal.task_bat_info_update();
                display_count = 0;
                display.clearScreen();
                display.display();
                show_display();
            }
            else
                show_display();
            display_count++;
        }
        a++;
        delay(50);
    }    
    appManager.noDeepSleep = false;
    appManager.nextWakeup = 30; 
}