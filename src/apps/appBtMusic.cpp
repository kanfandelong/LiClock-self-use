#include "AppManager.h"
#include "bt_music/btMusicBox.h"

btMusicBox *music;

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
    void setup();
};
static AppBtMusic app;

void AppBtMusic::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void AppBtMusic::menu(){

}
void AppBtMusic::setup(){
    music = new btMusicBox("LiClock");
    music->begin();
    music->I2S();   
}