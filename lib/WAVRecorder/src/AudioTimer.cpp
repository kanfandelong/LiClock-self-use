#include "AudioTimer.h"
#include "driver/timer.h"

#ifdef ESP32
hw_timer_t* timer = NULL;
#endif

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL_SEC   (1.0) // sample time for the timer
#define TEST_WITH_RELOAD      1     // testing will reload the counter value after the alarm
void AudioTimer::setup(uint32_t frequency, void (*f)(void*)) {
	stop();

#ifdef ESP8266
	timer1_attachInterrupt(f);	
	uint32_t tick = (80 * 1000000) / frequency;
	timer1_write(tick); /* ( 80 / 1 ) * 125 */
#endif

#ifdef ESP32
/*	timer = timerBegin(0, 20, true);
 	timerAttachInterrupt(timer, f, true);
	timerAlarmWrite(timer, 4000000 / frequency, true); // 500 for 8Khz, 250 for 16Khz, 125 for 32Khz */ 
	timer_config_t config;
    config.divider = 4;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = TIMER_AUTORELOAD_EN;
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 5000000 / frequency);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, f, (void*) TIMER_0, ESP_INTR_FLAG_EDGE, NULL);

#endif


#ifdef ARDUINO_SAM_DUE 
	// Here we are going to use timer 4
	Timer4.attachInterrupt(f).setFrequency(frequency);
#endif


}

void AudioTimer::start() {
#ifdef ESP8266
	timer1_enable(TIM_DIV1/*from 80 Mhz*/, TIM_EDGE/*TIM_LEVEL*/, TIM_LOOP/*TIM_SINGLE*/);
#endif

#ifdef ESP32
	//timerAlarmEnable(timer);
    timer_start(TIMER_GROUP_0, TIMER_0);
#endif

#ifdef ARDUINO_SAM_DUE 
	Timer4.start();
#endif
}

void AudioTimer::stop() {
#ifdef ESP8266
	timer1_disable();
#endif

#ifdef ESP32
	if (timer)
		timer_pause(TIMER_GROUP_0, TIMER_0);
		//timerAlarmDisable(timer);
#endif

#ifdef ARDUINO_SAM_DUE 
	Timer4.stop();
#endif	
}