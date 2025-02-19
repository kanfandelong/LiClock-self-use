#include "A_Config.h"

#ifndef __STC_ISP_H__
#define __STC_ISP_H__

#define RXD_2 36
#define TXD_2 32

#define ISP_PROGRAM_BUFFER          256


#define LOBYTE(w) ((uint8_t)(uint16_t)(w))
#define HIBYTE(w) ((uint8_t)((uint16_t)(w) >> 8))

#define FUSER                       35000000L
#define RL(n)                       (0x10000 - FUSER/4/(n))

#define ISP_DOWNLOAD_CHECKLOOP      2000
#define ISP_DOWNLOAD_BAUD           115200L
#define ISP_DOWNLOAD_TIMEOUT        2000
#define ISP_DOWNLOAD_TIMEOUT_ERASE  2000
#define ISP_DOWNLOAD_PAGESIZE       128

#define ISP_ARG7_8H                 0x97
#define ISP_ARG7_8G                 0x97
#define ISP_ARG7_8A                 0x81

typedef enum {
    STC_8H = 0x97,
    STC_8G = 0x97,
    STC_8A = 0x81,
    not_supported_chip = 0xFF
} isp_chip;

class ISP
{
public:
    uint8_t error_code[3];
    bool FS_bin_isp(FS *fs, const char *filename, isp_chip chip, int band = 9600, long chip_fosc = 24000000ul);
    bool FS_hex_isp(FS *fs, const char *filename, isp_chip chip, int band = 9600, long chip_fosc = 24000000ul);
private:
    unsigned int g_nIspPowerOffTime;
    unsigned int g_nispReceiveSum;
    unsigned char g_ucispReceiveIndex, g_ucispReceiveCount, g_ucispReceiveStep;
    unsigned char g_ucispRxBuffer[64], g_ucispTxBuffer[150];
    unsigned char g_ucispReceiveFlag;
    unsigned int g_nispProgramStart;
    unsigned char g_ucispProgramLength;
    unsigned char g_ucispProgramBuffer[ISP_PROGRAM_BUFFER];
    isp_chip chip_is;
    long _chip_fosc;
    int download_band = 9600;
    char rx_buffer[256];
    void isp_init();
    void ispCommunicationInit();
    void isp_cmd(uint8_t ucSize);
    void ispProcChar();
    uint8_t connect_chip();
    uint8_t chip_erase();
    uint8_t chip_download(File code, uint32_t code_size, uint32_t address);
};
extern ISP isp;
#endif