#include "btMusicBox.h"
#include <Preferences.h> // For saving audio source BT addr for auto-reconnect
////////////////////////////////////////////////////////////////////
////////////// Nasty statics for i2sCallback ///////////////////////
////////////////////////////////////////////////////////////////////
 float btMusicBox::_vol=0.95;
 esp_bd_addr_t btMusicBox::_address;
 int32_t btMusicBox::_sampleRate=44100;
 
 int btMusicBox::_postprocess=0;
 filter btMusicBox::_filtLhp = filter(2,_sampleRate,3,highpass); 
 filter btMusicBox::_filtRhp = filter(2,_sampleRate,3,highpass);
 filter btMusicBox::_filtLlp = filter(20000,_sampleRate,3,lowpass); 
 filter btMusicBox::_filtRlp = filter(20000,_sampleRate,3,lowpass);  
 DRC btMusicBox::_DRCL = DRC(_sampleRate,60.0,0.001,0.2,4.0,10.0,0.0); 
 DRC btMusicBox::_DRCR = DRC(_sampleRate,60.0,0.001,0.2,4.0,10.0,0.0); 
 
 Preferences preferences;

////////////////////////////////////////////////////////////////////
////////////////////////// Constructor /////////////////////////////
////////////////////////////////////////////////////////////////////
btMusicBox::btMusicBox(const char *devName) {
  _devName=devName;  
}

////////////////////////////////////////////////////////////////////
////////////////// Bluetooth Functionality /////////////////////////
////////////////////////////////////////////////////////////////////
void btMusicBox::begin() {
	
  //Arduino bluetooth initialisation
  btStart();

  // bluedroid allows for bluetooth classic
  esp_bluedroid_init();
  esp_bluedroid_enable();
  
  //set up device name
  esp_bt_dev_set_device_name(_devName);
  
  // initialize AVRCP controller
  esp_avrc_ct_init();
  esp_avrc_ct_register_callback(avrc_callback);
  
  // this sets up the audio receive
  esp_a2d_sink_init();
  
  esp_a2d_register_callback(a2d_cb);
  
  // set discoverable and connectable mode, wait to be connected
#if ESP_IDF_VERSION_MAJOR > 3
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
  esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif
}

void btMusicBox::end() {
  esp_a2d_sink_deinit();
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  btStop();
}

void btMusicBox::reconnect() {
  // Load rememebered device address from flash
  preferences.begin("btMusicBox", false);
  _address[0] = preferences.getUChar("btaddr0", 0);
  _address[1] = preferences.getUChar("btaddr1", 0);
  _address[2] = preferences.getUChar("btaddr2", 0);
  _address[3] = preferences.getUChar("btaddr3", 0);
  _address[4] = preferences.getUChar("btaddr4", 0);
  _address[5] = preferences.getUChar("btaddr5", 0);
  preferences.end();
  
  // Only attempt connection if address exists
  if (_address[0] + _address[1] +
      _address[2] + _address[3] +
      _address[4] + _address[5] != 0)
  {
    ESP_LOGI("btMusicBox", "Connecting to remembered BT device: %d %d %d %d %d %d", 
            _address[0], _address[1],
            _address[2], _address[3],
            _address[4], _address[5]);
    // Connect to remembered device
    esp_a2d_sink_connect(_address);
  }
}

void btMusicBox::disconnect() {
  esp_a2d_sink_disconnect(_address);
}

void btMusicBox::a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t*param){
	esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(param);
	switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
    {
        uint8_t* temp= a2d->conn_stat.remote_bda;
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
        {
            _address[0]= *temp;     _address[1]= *(temp+1);
		    _address[2]= *(temp+2); _address[3]= *(temp+3);
		    _address[4]= *(temp+4); _address[5]= *(temp+5);
            ESP_LOGI("btMusicBox", "Connected to BT device: %d %d %d %d %d %d", _address[0], _address[1], _address[2], _address[3], _address[4], _address[5]);

		    // Store connected BT address for use by reconnect()
		    preferences.begin("btMusicBox", false);
            if (preferences.getUChar("btaddr0", 0) != _address[0]) { preferences.putUChar("btaddr0", _address[0]); ESP_LOGI("btMusicBox", "Writing BTaddr0"); }
            if (preferences.getUChar("btaddr1", 0) != _address[1]) { preferences.putUChar("btaddr1", _address[1]); ESP_LOGI("btMusicBox", "Writing BTaddr1"); }
            if (preferences.getUChar("btaddr2", 0) != _address[2]) { preferences.putUChar("btaddr2", _address[2]); ESP_LOGI("btMusicBox", "Writing BTaddr2"); }
            if (preferences.getUChar("btaddr3", 0) != _address[3]) { preferences.putUChar("btaddr3", _address[3]); ESP_LOGI("btMusicBox", "Writing BTaddr3"); }
            if (preferences.getUChar("btaddr4", 0) != _address[4]) { preferences.putUChar("btaddr4", _address[4]); ESP_LOGI("btMusicBox", "Writing BTaddr4"); }
            if (preferences.getUChar("btaddr5", 0) != _address[5]) { preferences.putUChar("btaddr5", _address[5]); ESP_LOGI("btMusicBox", "Writing BTaddr5"); }
            preferences.end();
            break;
        }
    }
	case ESP_A2D_AUDIO_CFG_EVT: {
        ESP_LOGI("BT_AV", "A2DP audio stream configuration, codec type %d", a2d->audio_cfg.mcc.type);
        // for now only SBC stream is supported
        if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
            _sampleRate = 16000;
            char oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
            if (oct0 & (0x01 << 6)) {
                _sampleRate = 32000;
            } else if (oct0 & (0x01 << 5)) {
                _sampleRate = 44100;
            } else if (oct0 & (0x01 << 4)) {
                _sampleRate = 48000;
            }
            ESP_LOGI("BT_AV", "Configure audio player %x-%x-%x-%x",
                     a2d->audio_cfg.mcc.cie.sbc[0],
                     a2d->audio_cfg.mcc.cie.sbc[1],
                     a2d->audio_cfg.mcc.cie.sbc[2],
                     a2d->audio_cfg.mcc.cie.sbc[3]);
					 if(i2s_set_sample_rates(I2S_NUM_0, _sampleRate)==ESP_OK){
						//ESP_LOGI("BT_AV", "Audio player configured, sample rate=%d", _sampleRate);
            Serial.printf("Audio player configured, sample rate=%d\n", _sampleRate);
					 }
		}
		
        break;
    }
    default:
        log_e("a2dp invalid cb event: %d", event);
        break;
    }
}
void btMusicBox::updateMeta() {
  uint8_t attr_mask = ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_GENRE;
  esp_avrc_ct_send_metadata_cmd(1, attr_mask);
}
void btMusicBox::avrc_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
  esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(param);
  char *attr_text;
  String mystr;

  switch (event) {
    case ESP_AVRC_CT_METADATA_RSP_EVT: {
      attr_text = (char *) malloc (rc->meta_rsp.attr_length + 1);
      memcpy(attr_text, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
      attr_text[rc->meta_rsp.attr_length] = 0;
      mystr = String(attr_text);

      switch (rc->meta_rsp.attr_id) {
        case ESP_AVRC_MD_ATTR_TITLE:
          //Serial.print("Title: ");
          //Serial.println(mystr);
		      //title= mystr;
          break;
        case ESP_AVRC_MD_ATTR_ARTIST:
          //Serial.print("Artist: ");
          //Serial.println(mystr);
          //artist= mystr;
		  break;
        case ESP_AVRC_MD_ATTR_ALBUM:
          //Serial.print("Album: ");
          //Serial.println(mystr);
          //album= mystr;
		  break;
        case ESP_AVRC_MD_ATTR_GENRE:
          //Serial.print("Genre: ");
          //Serial.println(mystr);
          //genre= mystr;
		  break;
      }
      free(attr_text);
  }break;
    default:
      ESP_LOGE("RCCT", "unhandled AVRC event: %d", event);
      break;
  }
}
void btMusicBox::setSinkCallback(void (*sinkCallback)(const uint8_t *data, uint32_t len) ){
	 esp_a2d_sink_register_data_callback(sinkCallback);
}
////////////////////////////////////////////////////////////////////
////////////////// I2S Audio Functionality /////////////////////////
////////////////////////////////////////////////////////////////////
void btMusicBox::I2S(int bck, int dout, int ws) {
   // i2s configuration
/*   static const i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
    .sample_rate = _sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
#if ESP_IDF_VERSION_MAJOR > 3
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,//static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_STAND_I2S|I2S_COMM_FORMAT_STAND_MSB),
#else
    .communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_I2S|I2S_COMM_FORMAT_I2S_MSB),
#endif
    // .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // default interrupt priority
    // .dma_buf_count = 3,
    // .dma_buf_len = 600,
    // .use_apll = false,
    // .tx_desc_auto_clear = true
     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,          
     .dma_buf_count = 3,                                                     
     .dma_buf_len = 300,                                                     
     .use_apll = true,                                                       
     .tx_desc_auto_clear = true,                                             
     .fixed_mclk = 0  
  }; */
  i2s_mode_t mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config_t i2s_config_dac = {
    .mode = (i2s_mode_t)(mode | I2S_MODE_DAC_BUILT_IN),
    .sample_rate = (uint32_t)_sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
    .dma_buf_count = 3,
    .dma_buf_len = 300,
    .use_apll = true, // Use audio PLL
    .tx_desc_auto_clear = true, // Silence on underflow
    .fixed_mclk = 0, // Unused
    .mclk_multiple = I2S_MCLK_MULTIPLE_256, // Unused
    .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT // Use bits per sample
  };
  // i2s pinout
  static const i2s_pin_config_t pin_config = {
    .mck_io_num = -1,
    .bck_io_num =bck,//27
    .ws_io_num = ws, //25
    .data_out_num = dout, //26
    .data_in_num = -1 //I2S_PIN_NO_CHANGE
  };
  
  // now configure i2s with constructed pinout and config
  i2s_driver_install(I2S_NUM_0, &i2s_config_dac, 0, NULL);
  i2s_set_pin(I2S_NUM_0, NULL); // 使用内置DAC
  
  // Sets the function that will handle data (i2sCallback)
   esp_a2d_sink_register_data_callback(i2sCallback);
}
void btMusicBox::i2sCallback(const uint8_t *data, uint32_t len){
  size_t i2s_bytes_write = 0; 
  int16_t* data16=(int16_t*)data; //playData doesnt want const
  int16_t fy[2];
  float temp;
  
  int jump =4; //how many bytes at a time get sent to buffer
  int  n = len/jump; // number of byte chunks	
	switch (_postprocess) {
   case NOTHING:
        for(int i=0;i<n;i++){
		 //process left channel
		 fy[0] = (int16_t)((*data16)*_vol);
		 data16++;
		 
		 // process right channel
		 fy[1] = (int16_t)((*data16)*_vol);
		 data16++;
		 i2s_write(I2S_NUM_0, fy, jump, &i2s_bytes_write,  100 ); 
		}
		break;
   case FILTER:
		for(int i=0;i<n;i++){
		 //process left channel
		 temp = _filtLlp.process(_filtLhp.process((*data16)*_vol));
		 
		 // overflow check
		 if(temp>32767){
		 temp=32767;
		 }
		 if(temp < -32767){
			temp= -32767;
		 }
		 fy[0] = (int16_t)(temp);
	     data16++;
		 
		 // process right channel
		 temp = _filtRlp.process(_filtRhp.process((*data16)*_vol));
		 if(temp>32767){
		 temp=32767;
		 }
		 if(temp < -32767){
			temp= -32767;
		 }
		 fy[1] =(int16_t) (temp);
		 data16++; 
		 i2s_write(I2S_NUM_0, fy, jump, &i2s_bytes_write,  100 );
		} 
		break;
   case COMPRESS:
	    for(int i=0;i<n;i++){
		 //process left channel
		 fy[0] = (*data16); 
		 fy[0]=_DRCL.softKnee(fy[0]*_vol);
		 data16++;
		 
		 // process right channel
		 fy[1] = (*data16);
		 fy[1]=_DRCR.softKnee(fy[1]*_vol);
		 data16++;
		 i2s_write(I2S_NUM_0, fy, jump, &i2s_bytes_write,  100 );
		}
		break;
   case FILTER_COMPRESS:
      for(int i=0;i<n;i++){
		 //process left channel(overflow check built into DRC)
		 fy[0] = _DRCL.softKnee(_filtLhp.process(_filtLlp.process((*data16)*_vol)));
		 data16++;
		 
		 //process right channel(overflow check built into DRC)
		 fy[1] = _DRCR.softKnee(_filtRhp.process(_filtRlp.process((*data16)*_vol)));
		 data16++;
		 i2s_write(I2S_NUM_0, fy, jump, &i2s_bytes_write,  100 );
		}
		break;	
  }

}
void btMusicBox::volume(float vol){
	_vol = constrain(vol,0,1);	
}

////////////////////////////////////////////////////////////////////
////////////////// Filtering Functionality /////////////////////////
////////////////////////////////////////////////////////////////////
void btMusicBox::createFilter(int n, float fc, int type){
   fc=constrain(fc,2,20000);
   switch (type) {
   case lowpass:
	_filtLlp= filter(fc,_sampleRate,n,type);
    _filtRlp= filter(fc,_sampleRate,n,type);
   break;
   case highpass:
	_filtLhp= filter(fc,_sampleRate,n,type);
    _filtRhp= filter(fc,_sampleRate,n,type);
   break;
   }
	_filtering=true;
	
	if(_filtering & _compressing){
	 _postprocess=3;	
	}else{
	 _postprocess=1;
	}
}
void btMusicBox::stopFilter(){
	_filtering=false;
	if(_compressing){
	  _postprocess = 2;
	}else{
	 _postprocess = 0;
	}
}

////////////////////////////////////////////////////////////////////
////////////////// Compression Functionality ///////////////////////
////////////////////////////////////////////////////////////////////
void btMusicBox::compress(float T,float alphAtt,float alphRel, float R,float w,float mu){
	_T=T;
	_alphAtt=alphAtt;
	_alphRel=alphRel;
	_R=R;
	_w=w;
	_mu=mu;
	
	_DRCL = DRC(_sampleRate,T,alphAtt,alphRel,R,w,mu);
	_DRCR = DRC(_sampleRate,T,alphAtt,alphRel,R,w,mu);
	_compressing=true;
	
	if(_filtering & _compressing){
	 _postprocess=3;	
	}else{
	 _postprocess=2;
	}	
}
void btMusicBox::decompress(){
	_compressing=false;
	
	if(_filtering){
	  _postprocess = 1;
	}else{
	 _postprocess = 0;
	}
}
