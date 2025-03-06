#ifndef  __BTMUSICBOX_H
#define __BTMUSICBOX_H

#include "Arduino.h"
#include "Wire.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "esp_avrc_api.h"
#include "filter.h"
#include "DRC.h"

// postprocessing 
enum {
    NOTHING = 0,
    FILTER,
	COMPRESS,
	FILTER_COMPRESS,
};
class btMusicBox {
  public:
	//Constructor
	btMusicBox(const char *devName);
	
	// Bluetooth functionality
	void begin();  
	void end();
	void disconnect();
	void reconnect();
	void setSinkCallback(void (*sinkCallback)(const uint8_t *data, uint32_t len));
	
	// I2S Audio
	void I2S(int bck = -1, int dout = -1, int ws = -1);
	void volume(float vol);
	
	
	float _T=60.0;
	float _alphAtt=0.001;
	float _alphRel=0.1; 
	float _R=4.0;
	float _w=10.0;
	float _mu=0.0; 
	
  private:
    const char *_devName;
	bool _filtering=false;
	bool _compressing=false;
    static int32_t  _sampleRate;
	static int _postprocess;
	
	// static function causes a static infection of variables
	static void i2sCallback(const uint8_t *data, uint32_t len);
	static void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
	
	// bluetooth address of connected device
	static uint8_t _address[6];
	static DRC _DRCR;
	static DRC _DRCL;
	static float _vol;
	static filter _filtLlp;
    static filter _filtRlp;
    static filter _filtLhp;
    static filter _filtRhp;	
};

#endif
