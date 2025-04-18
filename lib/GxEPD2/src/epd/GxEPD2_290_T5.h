// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
// Controller: IL0373 : http://www.e-paper-display.com/download_detail/downloadsId=535.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_290_T5_H_
#define _GxEPD2_290_T5_H_

#include "../GxEPD2_EPD.h"

class GxEPD2_290_T5 : public GxEPD2_EPD
{
  public:
    // attributes
    static const uint16_t WIDTH = 128;
    static const uint16_t WIDTH_VISIBLE = WIDTH;
    static const uint16_t HEIGHT = 296;
    static const GxEPD2::Panel panel = GxEPD2::GDEW029T5;
    static const bool hasColor = false;
    static const bool hasPartialUpdate = true;
    static const bool hasFastPartialUpdate = true; // set this false for test with OTP LUT (GDEW029I6F)
    static const uint16_t power_on_time = 100; // ms, e.g. 36553us
    static const uint16_t power_off_time = 50; // ms, e.g. 20759us
    static const uint16_t full_refresh_time = 3500; // ms, e.g. 2056899us
    static const uint16_t partial_refresh_time = 750; // ms, e.g. 353649us
    uint8_t lutgray = 15;
    // constructor
    GxEPD2_290_T5(int16_t cs, int16_t dc, int16_t rst, int16_t busy);
    // methods (virtual)
    void startQueue();
    QueueHandle_t getQueue();
    bool isBusy();
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void clearScreen(uint8_t value = 0xFF); // init controller memory and screen (default white)
    void writeScreenBuffer(uint8_t value = 0xFF); // init controller memory (default white)
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void refresh(bool partial_update_mode = false); // screen refresh from controller memory to full screen
    void refresh(int16_t x, int16_t y, int16_t w, int16_t h); // screen refresh from controller memory, partial screen
    void powerOff(); // turns off generation of panel driving voltages, avoids screen fading over time
    void hibernate(); // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)
    void SendLuts(uint8_t LutLevel = 15);
//private:
    //void _writeScreenBuffer(uint8_t value);
    void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void _PowerOn();
    void _PowerOff();
    void _InitDisplay();
    void _SendLuts(uint8_t LutLevel = 15);
    void _Init_Full();
    void _Init_Part();
    void _Update_Full();
    void _Update_Part();
    // 下面这些函数是实际调用的函数
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void __clearScreen(uint8_t value = 0xFF); // init controller memory and screen (default white)
    void __writeScreenBuffer(uint8_t value = 0xFF); // init controller memory (default white)
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void __writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    void __writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    void __drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    void __drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void __refresh(bool partial_update_mode = false); // screen refresh from controller memory to full screen
    void __refresh(int16_t x, int16_t y, int16_t w, int16_t h); // screen refresh from controller memory, partial screen
    void __powerOff(); // turns off generation of panel driving voltages, avoids screen fading over time
    void __hibernate(); // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)    
  private:
    static const unsigned char lut_20_vcomDC[];
    static const unsigned char lut_21_ww[];
    static const unsigned char lut_22_bw[];
    static const unsigned char lut_23_wb[];
    static const unsigned char lut_24_bb[];
    static const unsigned char lut_20_vcomDC_partial[];
    static const unsigned char lut_21_ww_partial[];
    static const unsigned char lut_22_bw_partial[];
    static const unsigned char lut_23_wb_partial[];
    static const unsigned char lut_24_bb_partial[];
    static const unsigned char lutFast_[];
};

#endif
