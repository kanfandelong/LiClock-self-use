// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
// Controller: UC8151D : https://v4.cecdn.yun300.cn/100001_1909185148/UC8151D.pdf
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_290_T5D.h"

GxEPD2_290_T5D::GxEPD2_290_T5D(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
}

void GxEPD2_290_T5D::__clearScreen(uint8_t value)
{
  __writeScreenBuffer(value);
  __refresh(true);
  __writeScreenBufferAgain(value);
}

void GxEPD2_290_T5D::__writeScreenBuffer(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  if (_initial_write) _writeScreenBuffer(0x10, value); // set previous
  _writeScreenBuffer(0x13, value); // set current
  _initial_write = false; // initial full screen buffer clean done
}

void GxEPD2_290_T5D::__writeScreenBufferAgain(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  _writeScreenBuffer(0x13, value); // set current
}

void GxEPD2_290_T5D::_writeScreenBuffer(uint8_t command, uint8_t value)
{
  if (T5D){
    _writeCommand(command);
    _startTransfer();
    for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
    {
      _transfer(value);
    }
    _endTransfer();
  }
  else{
    _writeCommand(0x13); // set current
    for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
    {
      _writeData(value);
    }
    if (_initial_refresh)
    {
      _writeCommand(0x10); // preset previous
      for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
      {
        _writeData(0xFF); // 0xFF is white
      }
    }
  }
}

void GxEPD2_290_T5D::__writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x10, bitmap, x, y, w, h, invert, mirror_y, pgm);
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::_writeImage(uint8_t command, const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) __writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  int16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
  x -= x % 8; // byte boundary
  w = wb * 8; // byte boundary
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_using_partial_mode) _Init_Part();
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(command);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb, h of bitmap for index!
      int16_t idx = mirror_y ? j + dx / 8 + ((h - 1 - (i + dy))) * wb : j + dx / 8 + (i + dy) * wb;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _transfer(data);
    }
  }
  _endTransfer();
  _writeCommand(0x92); // partial out
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_290_T5D::__writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x13, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x13, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::_writeImagePart(uint8_t command, const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) __writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if ((w_bitmap < 0) || (h_bitmap < 0) || (w < 0) || (h < 0)) return;
  if ((x_part < 0) || (x_part >= w_bitmap)) return;
  if ((y_part < 0) || (y_part >= h_bitmap)) return;
  int16_t wb_bitmap = (w_bitmap + 7) / 8; // width bytes, bitmaps are padded
  x_part -= x_part % 8; // byte boundary
  w = w_bitmap - x_part < w ? w_bitmap - x_part : w; // limit
  h = h_bitmap - y_part < h ? h_bitmap - y_part : h; // limit
  x -= x % 8; // byte boundary
  w = 8 * ((w + 7) / 8); // byte boundary, bitmaps are padded
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_using_partial_mode) _Init_Part();
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(T5D ? command : 0x13);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      int16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + ((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + (y_part + i + dy) * wb_bitmap;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _transfer(data);
    }
  }
  _endTransfer();
  _writeCommand(0x92); // partial out
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_290_T5D::__writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __writeImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    __writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  __writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  __refresh(x, y, w, h);
  __writeImageAgain(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                   int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  __writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  __refresh(x, y, w, h);
  __writeImagePartAgain(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __drawImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                   int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __drawImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    __drawImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__refresh(bool partial_update_mode)
{
  if (partial_update_mode) __refresh(0, 0, WIDTH, HEIGHT);
  else
  {
    if (_using_partial_mode) _Init_Full();
    _Update_Full();
    _initial_refresh = false; // initial full update done
  }
}

void GxEPD2_290_T5D::__refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  if (_initial_refresh) return __refresh(false); // initial update needs be full update
  // intersection with screen
  int16_t w1 = x < 0 ? w + x : w; // reduce
  int16_t h1 = y < 0 ? h + y : h; // reduce
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  w1 = x1 + w1 < int16_t(WIDTH) ? w1 : int16_t(WIDTH) - x1; // limit
  h1 = y1 + h1 < int16_t(HEIGHT) ? h1 : int16_t(HEIGHT) - y1; // limit
  if ((w1 <= 0) || (h1 <= 0)) return; 
  // make x1, w1 multiple of 8
  w1 += x1 % 8;
  if (w1 % 8 > 0) w1 += 8 - w1 % 8;
  x1 -= x1 % 8;
  if (!_using_partial_mode) _Init_Part();
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _Update_Part();
  _writeCommand(0x92); // partial out
}

void GxEPD2_290_T5D::__powerOff(void)
{
  _PowerOff();
}

void GxEPD2_290_T5D::__hibernate()
{
  if (T5D){
    _writeCommand(0X50);  //VCOM AND DATA INTERVAL SETTING     
    _writeData(0xf7); //WBmode:VBDF 17|D7 VBDW 97 VBDB 57    WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7  
  }
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommand(0x07); // deep sleep
    _writeData(0xA5);    // check code
    _hibernating = true;
  }
}

void GxEPD2_290_T5D::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
  uint16_t ye = y + h - 1;
  x &= 0xFFF8; // byte boundary
  _writeCommand(0x90); // partial window
  //_writeData(x / 256);
  _writeData(x % 256);
  //_writeData(xe / 256);
  _writeData(xe % 256);
  _writeData(y / 256);
  _writeData(y % 256);
  _writeData(ye / 256);
  _writeData(ye % 256);
  _writeData(0x01); // don't see any difference
  //_writeData(0x00); // don't see any difference
}

void GxEPD2_290_T5D::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommand(0x04);
    _waitWhileBusy("_PowerOn", T5D ? 100 : 400);
  }
  _power_is_on = true;
}

void GxEPD2_290_T5D::_PowerOff()
{
  _writeCommand(0x02); // power off
  _waitWhileBusy("_PowerOff", T5D ? 50 : 250);
  _power_is_on = false;
  _using_partial_mode = false;
}

void GxEPD2_290_T5D::_InitDisplay()
{
  if (_hibernating) {
    if (T5D)
    {
      _reset();
      // _reset();
      // _reset();
    }
    else
      _reset();
  }
  if (T5D){
    _writeCommand(0x00); // panel setting
    _writeData(0x1f);    // LUT from OTP, 128x296
    _writeCommand(0x61); //resolution setting
    _writeData (WIDTH);
    _writeData (HEIGHT >> 8);
    _writeData (HEIGHT & 0xFF);
    _writeCommand(0x50); // VCOM AND DATA INTERVAL SETTING
    _writeData(0x97);    // WBmode:VBDF 17|D7 VBDW 97 VBDB 57   WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7
    __PLL_set(PLL_val); //0x3a(58):100Hz, 0x29(37):150Hz
  }
  else{
    _writeCommand(0x01); //POWER SETTING
    _writeData (0x03);
    _writeData (0x00);
    _writeData (0x2b);
    _writeData (0x2b);
    _writeData (0x03);
    _writeCommand(0x06); //boost soft start
    _writeData (0x17);   //A
    _writeData (0x17);   //B
    _writeData (0x17);   //C
    _writeCommand(0x00); //panel setting
    //_writeData(0xbf);    //LUT from register, 128x296
    //_writeData(0x1f);    //LUT from OTP, 128x296
    _writeData(hasFastPartialUpdate ? 0xbf : 0x1f); // for test with OTP LUT
    _writeData(0x0d);    //VCOM to 0V fast
    __PLL_set(PLL_val); //0x3a(58):100Hz, 0x29(37):150Hz 39 200HZ 31 171HZ
    _writeCommand(0x61); //resolution setting
    _writeData (WIDTH);
    _writeData (HEIGHT >> 8);
    _writeData (HEIGHT & 0xFF);
  }
}

//full screen update LUT
const unsigned char GxEPD2_290_T5D::lut_20_vcomDC[] PROGMEM =
{
  0x00, 0x08, 0x00, 0x00, 0x00, 0x02,
  0x60, 0x28, 0x28, 0x00, 0x00, 0x01,
  0x00, 0x14, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x12, 0x12, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_21_ww[] PROGMEM =
{
  0x40, 0x08, 0x00, 0x00, 0x00, 0x02,
  0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
  0x40, 0x14, 0x00, 0x00, 0x00, 0x01,
  0xA0, 0x12, 0x12, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_22_bw[] PROGMEM =
{
  0x40, 0x08, 0x00, 0x00, 0x00, 0x02,
  0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
  0x40, 0x14, 0x00, 0x00, 0x00, 0x01,
  0xA0, 0x12, 0x12, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_23_wb[] PROGMEM =
{
  0x80, 0x08, 0x00, 0x00, 0x00, 0x02,
  0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
  0x80, 0x14, 0x00, 0x00, 0x00, 0x01,
  0x50, 0x12, 0x12, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_24_bb[] PROGMEM =
{
  0x80, 0x08, 0x00, 0x00, 0x00, 0x02,
  0x90, 0x28, 0x28, 0x00, 0x00, 0x01,
  0x80, 0x14, 0x00, 0x00, 0x00, 0x01,
  0x50, 0x12, 0x12, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
//partial screen update LUT
//#define Tx19 0x19 // original value is 25 (phase length)
#define Tx19 0x20   // new value for test is 32 (phase length)
const unsigned char GxEPD2_290_T5D::lut_20_vcomDC_partial[] PROGMEM =
{
  0x00, Tx19, 0x01, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_21_ww_partial[] PROGMEM =
{
  0x00, Tx19, 0x01, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_22_bw_partial[] PROGMEM =
{
  0x80, Tx19, 0x01, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_23_wb_partial[] PROGMEM =
{
  0x40, Tx19, 0x01, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lut_24_bb_partial[] PROGMEM =
{
  0x00, Tx19, 0x01, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const unsigned char GxEPD2_290_T5D::lutFast_[] PROGMEM ={
  0x00,0x18,0x5a,0xa5,0x24
};


void GxEPD2_290_T5D::__SendLuts(uint8_t LutLevel){
  LutLevel = LutLevel?(LutLevel>15?15:LutLevel):15; 
  lutgray = LutLevel;
  uint8_t greyHQ = 3;
  for(uint8_t i=0;i<5;i++){
    _writeCommand(i+0x20);
    for(int j=0;j<(i==0?44:42);j++){
        if(j==4 && ((i==2) || (greyHQ==3 && i==4))) _writeData(0x0f); //刷黑->白
        else if(j==greyHQ) _writeData(LutLevel);
        else if(j==0) _writeData(pgm_read_byte(lutFast_+(i)));
        else if(j==5) _writeData(1);
        else _writeData(0x0);
    }
  }
  __PLL_set(PLL_val); //0x3a(58):100Hz, 0x29(37):150Hz
}

void GxEPD2_290_T5D::__PLL_set(uint8_t PLL_set_val){
  _writeCommand(0x30);
  _writeData(PLL_set_val); //0x3a(58):100Hz, 0x29(37):150Hz
  PLL_val = PLL_set_val;
}

void GxEPD2_290_T5D::__T5D_mode(bool mode){
  T5D = mode;
}

void GxEPD2_290_T5D::_Init_Full()
{
  _InitDisplay();
  if (!T5D)
  {
    _writeCommand(0x82); //vcom_DC setting
    _writeData (0x08);
    _writeCommand(0X50); //VCOM AND DATA INTERVAL SETTING
    _writeData(0x97);    //WBmode:VBDF 17|D7 VBDW 97 VBDB 57   WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7
    _writeCommand(0x20);
    _writeDataPGM(lut_20_vcomDC, sizeof(lut_20_vcomDC));
    _writeCommand(0x21);
    _writeDataPGM(lut_21_ww, sizeof(lut_21_ww));
    _writeCommand(0x22);
    _writeDataPGM(lut_22_bw, sizeof(lut_22_bw));
    _writeCommand(0x23);
    _writeDataPGM(lut_23_wb, sizeof(lut_23_wb));
    _writeCommand(0x24);
    _writeDataPGM(lut_24_bb, sizeof(lut_24_bb));
  }
  _PowerOn();
  _using_partial_mode = false;
}

void GxEPD2_290_T5D::_Init_Part()
{
  _InitDisplay();
  if (T5D){
    _writeCommand(0x00); //panel setting
    _writeData(hasFastPartialUpdate ? 0xbf : 0x1f); // for test with OTP LUT
  }
  _writeCommand(0x82); //vcom_DC setting
  _writeData (0x08);
  _writeCommand(0x50);
  _writeData(0x17);    //WBmode:VBDF 17|D7 VBDW 97 VBDB 57   WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7
  if (lutgray != 15){
    __SendLuts(lutgray);
  } else {
    _writeCommand(0x20);
    _writeDataPGM(lut_20_vcomDC_partial, sizeof(lut_20_vcomDC_partial));
    _writeCommand(0x21);
    _writeDataPGM(lut_21_ww_partial, sizeof(lut_21_ww_partial));
    _writeCommand(0x22);
    _writeDataPGM(lut_22_bw_partial, sizeof(lut_22_bw_partial));
    _writeCommand(0x23);
    _writeDataPGM(lut_23_wb_partial, sizeof(lut_23_wb_partial));
    _writeCommand(0x24);
    _writeDataPGM(lut_24_bb_partial, sizeof(lut_24_bb_partial));
  }
  _PowerOn();
  _using_partial_mode = true;
}

void GxEPD2_290_T5D::_Update_Full()
{
  _writeCommand(0x12); //display refresh
  _waitWhileBusy("_Update_Full", T5D ? 3500 : 2100);
}

void GxEPD2_290_T5D::_Update_Part()
{
  _writeCommand(0x12); //display refresh
  _waitWhileBusy("_Update_Part", T5D ? 750 : 400);
}


void GxEPD2_290_T5D::task_(){
  if (task_list != 0)
    task_list--;
}

enum function_type_t
{
  FUNC_clearScreen,
  FUNC_writeScreenBuffer,
  FUNC_writeScreenBufferAgain,
  FUNC_writeImage,
  FUNC_writeImageForFullRefresh,
  FUNC_writeImagePart,
  FUNC_writeImageAgain,
  FUNC_writeImagePartAgain,
  FUNC_writeImage2,
  FUNC_writeImagePart2,
  FUNC_writeNative,
  FUNC_drawImage,
  FUNC_drawImagePart,
  FUNC_drawImage2,
  FUNC_drawImagePart2,
  FUNC_drawNative,
  FUNC_refresh,
  FUNC_refresh2,
  FUNC_powerOff,
  FUNC_hibernate,
  FUNC_sendlut,
  FUNC_pll_set,
  FUNC_t5d_mode,
  FUNC_set_interactive_mode
};

typedef struct
{
  function_type_t function_type;
  uint8_t value;
  uint8_t lut_value;
  uint8_t pll_value;
  bool t5d_mode;
  const uint8_t *bitmap;
  const uint8_t *color;
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  bool invert;
  bool mirror_y;
  bool pgm;
  int16_t x_part;
  int16_t y_part;
  int16_t w_bitmap;
  int16_t h_bitmap;
  bool partial_update_mode;
  const uint8_t *data1;
  const uint8_t *data2;
  GxEPD2_290_T5D *instance;
} multi_thread_params_t;

static QueueHandle_t multi_thread_queue = NULL;
static bool queue_busy = false;

static void process_multi_thread_queue()
{
  multi_thread_params_t multi_thread_params;
  xQueueReceive(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
  queue_busy = true;
  switch (multi_thread_params.function_type)
  {
  case FUNC_clearScreen:
    multi_thread_params.instance->__clearScreen(multi_thread_params.value);
    break;
  case FUNC_writeScreenBuffer:
    multi_thread_params.instance->__writeScreenBuffer(multi_thread_params.value);
    break;
  case FUNC_writeScreenBufferAgain:
    multi_thread_params.instance->__writeScreenBufferAgain(multi_thread_params.value);
    break;
  case FUNC_writeImage:
    multi_thread_params.instance->__writeImage(multi_thread_params.bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeImageForFullRefresh:
    multi_thread_params.instance->__writeImageForFullRefresh(multi_thread_params.bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeImagePart:
    multi_thread_params.instance->__writeImagePart(multi_thread_params.bitmap, multi_thread_params.x_part, multi_thread_params.y_part, multi_thread_params.w_bitmap, multi_thread_params.h_bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeImageAgain:
    multi_thread_params.instance->__writeImageAgain(multi_thread_params.bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeImagePartAgain:
    multi_thread_params.instance->__writeImagePartAgain(multi_thread_params.bitmap, multi_thread_params.x_part, multi_thread_params.y_part, multi_thread_params.w_bitmap, multi_thread_params.h_bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeImage2:
    multi_thread_params.instance->__writeImage(multi_thread_params.bitmap, multi_thread_params.color, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeImagePart2:
    multi_thread_params.instance->__writeImagePart(multi_thread_params.bitmap, multi_thread_params.color, multi_thread_params.x_part, multi_thread_params.y_part, multi_thread_params.w_bitmap, multi_thread_params.h_bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_writeNative:
    multi_thread_params.instance->__writeNative(multi_thread_params.data1, multi_thread_params.data2, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_drawImage:
    multi_thread_params.instance->__drawImage(multi_thread_params.bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_drawImagePart:
    multi_thread_params.instance->__drawImagePart(multi_thread_params.bitmap, multi_thread_params.x_part, multi_thread_params.y_part, multi_thread_params.w_bitmap, multi_thread_params.h_bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_drawImage2:
    multi_thread_params.instance->__drawImage(multi_thread_params.bitmap, multi_thread_params.color, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_drawImagePart2:
    multi_thread_params.instance->__drawImagePart(multi_thread_params.bitmap, multi_thread_params.color, multi_thread_params.x_part, multi_thread_params.y_part, multi_thread_params.w_bitmap, multi_thread_params.h_bitmap, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_drawNative:
    multi_thread_params.instance->__drawNative(multi_thread_params.data1, multi_thread_params.data2, multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h, multi_thread_params.invert, multi_thread_params.mirror_y, multi_thread_params.pgm);
    break;
  case FUNC_refresh:
    multi_thread_params.instance->__refresh(multi_thread_params.partial_update_mode);
    break;
  case FUNC_refresh2:
    multi_thread_params.instance->__refresh(multi_thread_params.x, multi_thread_params.y, multi_thread_params.w, multi_thread_params.h);
    break;
  case FUNC_powerOff:
    multi_thread_params.instance->__powerOff();
    break;
  case FUNC_hibernate:
    multi_thread_params.instance->__hibernate();
    break;
  case FUNC_sendlut:
    multi_thread_params.instance->__SendLuts(multi_thread_params.lut_value);
    break;
  case FUNC_pll_set:
    multi_thread_params.instance->__PLL_set(multi_thread_params.pll_value);
    break;
  case FUNC_t5d_mode:
    multi_thread_params.instance->__T5D_mode(multi_thread_params.t5d_mode);
    break;
  case FUNC_set_interactive_mode:
    multi_thread_params.instance->_interactive_mode = multi_thread_params.t5d_mode;
    break;
  default:
    break;
  }
  multi_thread_params.instance->task_();
  queue_busy = false;
}

static void task_gxEPD2_290_T5D(void *params)
{
  while (1)
  {
    process_multi_thread_queue();
    queue_busy = false;
  }
}
/**
 * @brief 初始化并启动显示控制器的多线程任务队列
 * 
 * 该函数创建一个队列和一个任务来处理显示操作的异步执行。
 * 所有显示相关的操作（如刷新、写入图像等）将通过队列传递到后台任务执行，
 * 避免阻塞主线程，提高系统响应性。
 * 
 * @param list 队列长度，指定可以排队等待处理的显示操作数量上限
 * 
 * @note 该函数应该在系统初始化阶段调用一次
 * @note 队列和任务创建后会持续运行直到系统重启
 * @note 每个GxEPD2_290_T5D实例应该只调用一次此函数
 * 
 * @par 示例:
 * @code
 * GxEPD2_290_T5D display(...);
 * display.epd2.startQueue(10); // 创建长度为10的队列
 * @endcode
 * 
 * @see getQueue()
 * @see isBusy()
 */
void GxEPD2_290_T5D::startQueue(uint8_t list, uint8_t uxPriority)
{
  multi_thread_queue = xQueueCreate(list, sizeof(multi_thread_params_t));
  xTaskCreate(task_gxEPD2_290_T5D, "display_task", 4096, NULL, uxPriority, NULL);
}

QueueHandle_t GxEPD2_290_T5D::getQueue()
{
  return multi_thread_queue;
}

bool GxEPD2_290_T5D::isBusy()
{
  return queue_busy;
}

// 多线程包装方法
void GxEPD2_290_T5D::clearScreen(uint8_t value)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_clearScreen;
  multi_thread_params.value = value;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeScreenBuffer(uint8_t value)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeScreenBuffer;
  multi_thread_params.value = value;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeScreenBufferAgain(uint8_t value)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeScreenBufferAgain;
  multi_thread_params.value = value;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImage;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImageForFullRefresh;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImagePart;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x_part = x_part;
  multi_thread_params.y_part = y_part;
  multi_thread_params.w_bitmap = w_bitmap;
  multi_thread_params.h_bitmap = h_bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImageAgain;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                         int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImagePartAgain;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x_part = x_part;
  multi_thread_params.y_part = y_part;
  multi_thread_params.w_bitmap = w_bitmap;
  multi_thread_params.h_bitmap = h_bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImage2;
  multi_thread_params.bitmap = black;
  multi_thread_params.color = color;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeImagePart2;
  multi_thread_params.bitmap = black;
  multi_thread_params.color = color;
  multi_thread_params.x_part = x_part;
  multi_thread_params.y_part = y_part;
  multi_thread_params.w_bitmap = w_bitmap;
  multi_thread_params.h_bitmap = h_bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_writeNative;
  multi_thread_params.data1 = data1;
  multi_thread_params.data2 = data2;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_drawImage;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                   int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_drawImagePart;
  multi_thread_params.bitmap = bitmap;
  multi_thread_params.x_part = x_part;
  multi_thread_params.y_part = y_part;
  multi_thread_params.w_bitmap = w_bitmap;
  multi_thread_params.h_bitmap = h_bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_drawImage2;
  multi_thread_params.bitmap = black;
  multi_thread_params.color = color;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                   int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_drawImagePart2;
  multi_thread_params.bitmap = black;
  multi_thread_params.color = color;
  multi_thread_params.x_part = x_part;
  multi_thread_params.y_part = y_part;
  multi_thread_params.w_bitmap = w_bitmap;
  multi_thread_params.h_bitmap = h_bitmap;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_drawNative;
  multi_thread_params.data1 = data1;
  multi_thread_params.data2 = data2;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.invert = invert;
  multi_thread_params.mirror_y = mirror_y;
  multi_thread_params.pgm = pgm;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::refresh(bool partial_update_mode)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_refresh;
  multi_thread_params.partial_update_mode = partial_update_mode;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_refresh2;
  multi_thread_params.x = x;
  multi_thread_params.y = y;
  multi_thread_params.w = w;
  multi_thread_params.h = h;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::powerOff(void)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_powerOff;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::hibernate()
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_hibernate;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::SendLuts(uint8_t LutLevel)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_sendlut;
  multi_thread_params.lut_value = LutLevel;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::PLL_set(uint8_t PLL_set_val)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_pll_set;
  multi_thread_params.pll_value = PLL_set_val;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::T5D_mode(bool mode)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_t5d_mode;
  multi_thread_params.t5d_mode = mode;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

void GxEPD2_290_T5D::set_interactive_mode(bool mode)
{
  multi_thread_params_t multi_thread_params;
  multi_thread_params.function_type = FUNC_set_interactive_mode;
  multi_thread_params.t5d_mode = mode;
  multi_thread_params.instance = this;
  while (_interactive_mode && task_list > 1)
  {
    delay(10);
  }
  task_list ++;
  xQueueSend(multi_thread_queue, &multi_thread_params, portMAX_DELAY);
}

/* // 实际执行的方法（在多线程中调用）
void GxEPD2_290_T5D::__clearScreen(uint8_t value)
{
  writeScreenBuffer(value);
  refresh(true);
  writeScreenBufferAgain(value);
}

void GxEPD2_290_T5D::__writeScreenBuffer(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  if (_initial_write) _writeScreenBuffer(0x10, value); // set previous
  _writeScreenBuffer(0x13, value); // set current
  _initial_write = false; // initial full screen buffer clean done
}

void GxEPD2_290_T5D::__writeScreenBufferAgain(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  _writeScreenBuffer(0x13, value); // set current
}

void GxEPD2_290_T5D::__writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x10, bitmap, x, y, w, h, invert, mirror_y, pgm);
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                      int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x13, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __writeImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                      int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                           int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x13, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    __writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImageAgain(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                     int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImagePartAgain(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_290_T5D::__drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __drawImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                     int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    __drawImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    __drawImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_290_T5D::__refresh(bool partial_update_mode)
{
  if (partial_update_mode) __refresh(0, 0, WIDTH, HEIGHT);
  else
  {
    if (_using_partial_mode) _Init_Full();
    _Update_Full();
    _initial_refresh = false; // initial full update done
  }
}

void GxEPD2_290_T5D::__refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  if (_initial_refresh) return __refresh(false); // initial update needs be full update
  // intersection with screen
  int16_t w1 = x < 0 ? w + x : w; // reduce
  int16_t h1 = y < 0 ? h + y : h; // reduce
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  w1 = x1 + w1 < int16_t(WIDTH) ? w1 : int16_t(WIDTH) - x1; // limit
  h1 = y1 + h1 < int16_t(HEIGHT) ? h1 : int16_t(HEIGHT) - y1; // limit
  if ((w1 <= 0) || (h1 <= 0)) return; 
  // make x1, w1 multiple of 8
  w1 += x1 % 8;
  if (w1 % 8 > 0) w1 += 8 - w1 % 8;
  x1 -= x1 % 8;
  if (!_using_partial_mode) _Init_Part();
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _Update_Part();
  _writeCommand(0x92); // partial out
}

void GxEPD2_290_T5D::__powerOff(void)
{
  _PowerOff();
}

void GxEPD2_290_T5D::__hibernate()
{
  if (T5D){
    _writeCommand(0X50);  //VCOM AND DATA INTERVAL SETTING     
    _writeData(0xf7); //WBmode:VBDF 17|D7 VBDW 97 VBDB 57    WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7  
  }
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommand(0x07); // deep sleep
    _writeData(0xA5);    // check code
    _hibernating = true;
  }
}

void GxEPD2_290_T5D::__SendLuts(uint8_t LutLevel)
{
  SendLuts(LutLevel);
}

void GxEPD2_290_T5D::__PLL_set(uint8_t PLL_set_val)
{
  PLL_set(PLL_set_val);
}

void GxEPD2_290_T5D::__T5D_mode(bool mode)
{
  T5D_mode(mode);
} */
