#include "stc-isp.h"

void ISP::isp_init(){
    Serial1.begin(2400, SERIAL_8E1, RXD_2, TXD_2);
}

void ISP::ispCommunicationInit(){
    uint i;
    
    for(i = 0; i < sizeof(g_ucispRxBuffer); i ++)
        g_ucispRxBuffer[i] = 0;
        
    g_nispReceiveSum = 0;
    g_ucispReceiveIndex = g_ucispReceiveCount = g_ucispReceiveStep = 0;
    g_ucispReceiveFlag = 0;
    
}

void ISP::isp_cmd(uint8_t ucSize) {
    uint nSum;
    uint8_t i, c;
        
    Serial1.print((char)0x46);
    Serial1.print((char)0xb9);
    Serial1.print((char)0x6a);
    Serial1.print((char)0x00);
    
    nSum = ucSize + 6 + 0x6a;
    Serial1.print((char)ucSize + 6);
    
    for(i = 0; i < ucSize; i ++) {
        c = g_ucispTxBuffer[i];
        nSum += c;
        Serial1.print((char)c);
    }
    
    Serial1.print((char)(uint8_t)(nSum >> 8));
    Serial1.print((char)(uint8_t)(nSum & 0xff));
    Serial1.print((char)0x16);
    
    
    ispCommunicationInit();
    
}

void ISP::ispProcChar(){
    uint8_t ucChar;
    
    if(!Serial1.available()) return;    
    ucChar = Serial1.read();
    
    //--------------------------------------------------------------------------
//    printf("%bx ", ucChar);
    
    //------------------------------------------------------------------------        
    switch(g_ucispReceiveStep) {
        case 1:
            if(ucChar != 0xb9) goto L_CheckFirst;
            g_ucispReceiveStep ++;
            break;
           
        case 2:
            if(ucChar != 0x68) goto L_CheckFirst;
            g_ucispReceiveStep ++;
            break;
        
        case 3:
            if(ucChar != 0x0) goto L_CheckFirst;
            g_ucispReceiveStep ++;
            break;
            
        case 4:
            g_nispReceiveSum = 0x68 + ucChar;
            g_ucispReceiveCount = ucChar - 6;
            g_ucispReceiveIndex = 0;
            g_ucispReceiveStep ++;
            break;
            
        case 5:
            g_nispReceiveSum += ucChar;
            g_ucispRxBuffer[g_ucispReceiveIndex ++] = ucChar;
            if(g_ucispReceiveIndex == g_ucispReceiveCount)
                g_ucispReceiveStep ++;
            break;
            
        case 6:
            if(ucChar != (uint8_t)(g_nispReceiveSum >> 8))
                goto L_CheckFirst;
            g_ucispReceiveStep ++;
            break;
        
        case 7:
            if(ucChar != (uint8_t)(g_nispReceiveSum & 0xff))
                goto L_CheckFirst;
            g_ucispReceiveStep ++;
            break;
            
        case 8:
            if(ucChar != 0x16) goto L_CheckFirst;
            g_ucispReceiveFlag = 1;
            g_ucispReceiveStep ++;
            break;
            
L_CheckFirst:
        case 0:
        default:
            ispCommunicationInit();
            if(ucChar == 0x46) g_ucispReceiveStep = 1;
            else g_ucispReceiveStep = 0;
            
            break;
    }
}

uint8_t ISP::connect_chip(){
    uint8_t ucArg;
    long nMS10;
    uint nBaudTimeReload;
    ispCommunicationInit();
    nMS10 = (uint)(millis() + 10000);
    int i;
    //display.clearScreen();
    GUI::info_msgbox("提示", "请连接芯片并上电...");
    for (i = 0; i < ISP_DOWNLOAD_CHECKLOOP; i++){
    //while(1){
        ispProcChar(); 
        if (hal.btnl.isPressing())
            if (GUI::waitLongPress(PIN_BUTTONL))
                break;       
        if(g_ucispReceiveStep == 0) {
            if(millis() == nMS10) {
                Serial1.print((char)0x7f);
                nMS10 = (uint)(millis() + 10000);
                i++;
            }           
        }
        
        if(g_ucispReceiveFlag) {
            ucArg = g_ucispRxBuffer[4];
            if(g_ucispRxBuffer[0] == 0x50)
                break;
            
            return 1;                       // return error 1
        }
        
        //delay(1);
    }
    
    
    if(i >= ISP_DOWNLOAD_CHECKLOOP) return 2; // 2
    
    //--------------------------------------------------------------------------
    // Set new baud
//    printf("\r\nSet new Buad:%ld/%d\r\n", lnBaud, i);
    
//    nBaudTimeReload = Baud2TimeReload(lnBaud, lnFosc);
//    printf("%x\r\n", nBaudTimeReload);
    //Serial1.updateBaudRate(download_band);
    nBaudTimeReload = 65536 - _chip_fosc / 4 / download_band;
    
    ucArg = g_ucispRxBuffer[4];
    g_ucispTxBuffer[0] = 0x01;
    g_ucispTxBuffer[1] = ucArg;
    g_ucispTxBuffer[2] = 0x40;
    g_ucispTxBuffer[3] = HIBYTE(nBaudTimeReload);
    g_ucispTxBuffer[4] = LOBYTE(nBaudTimeReload);
    g_ucispTxBuffer[5] = 0x00;
    g_ucispTxBuffer[6] = 0x00;
    g_ucispTxBuffer[7] = (uint8_t)chip_is;//0x97;//0x81;//0x97;                // 0x81: STC8A
                                                            // 0x97: STC8G,8H

    nMS10 = (uint)(millis() + ISP_DOWNLOAD_TIMEOUT * 1000);
    isp_cmd(8);
    
    for(;;) {
        ispProcChar();
        if(g_ucispReceiveFlag) {
            if(g_ucispRxBuffer[0] == 0x01) break;
            return 3; // 3
        }
        
        if(millis() == nMS10) return 4; // 4
        
    }
    

    //----------------------------------------------------------------------
    // Prepare the new download baud    
    
    Serial1.updateBaudRate(download_band);
    
//    WaitTime(10);
    
    g_ucispTxBuffer[0] = 0x05;
    g_ucispTxBuffer[1] = 0x00;
    g_ucispTxBuffer[2] = 0x00;
    g_ucispTxBuffer[3] = 0x5a;
    g_ucispTxBuffer[4] = 0xa5;
        
    nMS10 = (uint)(millis() + ISP_DOWNLOAD_TIMEOUT * 1000);
    isp_cmd(5);
    for(;;) {
        ispProcChar();
        
        if(g_ucispReceiveFlag) {
            if(g_ucispRxBuffer[0] == 0x05) break;
            return 5;
        }
        
        if(millis() == nMS10) return 6; // 6

    }
    

    return 0;

}

uint8_t ISP::chip_erase(){
    long nMS10;
    
    //----------------------------------------------------------------------    
    // Erase the IC
    delay(10);
    
    g_ucispTxBuffer[0] = 0x03;
    g_ucispTxBuffer[1] = 0x00;
    g_ucispTxBuffer[2] = 0x00;
    g_ucispTxBuffer[3] = 0x5a;
    g_ucispTxBuffer[4] = 0xa5;
    
    
    nMS10 = (uint)(millis() + ISP_DOWNLOAD_TIMEOUT_ERASE * 1000);
    isp_cmd(5);
    
    for(;;) {
        ispProcChar();
        
        if(g_ucispReceiveFlag) {
            if(g_ucispRxBuffer[0] == 0x03) break;
            return false; // 1
        }

        if(millis() == nMS10) return false; // 2
    }
    
    return true;    
}

uint8_t ISP::chip_download(File code, uint32_t code_size, uint32_t address){
    uint i, nStep, nLength, j;
    uint nDownloadAddress, nStart, nEnd, nOffset;
    uint nPoint;
    uint nMS10;
    
    
    nStep = (int)((code_size + ISP_DOWNLOAD_PAGESIZE - 1) / ISP_DOWNLOAD_PAGESIZE);
    
    g_ucispTxBuffer[0] = 0x22;
    g_ucispTxBuffer[3] = 0x5a;
    g_ucispTxBuffer[4] = 0xa5;
    
    nOffset = 5;
    //nPoint = 0;
    code.seek(0);
    for(i = 0; i < nStep; i ++) {
        nStart = i * ISP_DOWNLOAD_PAGESIZE;
        nEnd = nStart + ISP_DOWNLOAD_PAGESIZE;
        if(nEnd > code_size) nEnd = code_size;
        nLength = nEnd - nStart;
        
        nDownloadAddress = address + nStart;
        g_ucispTxBuffer[1] = HIBYTE(nDownloadAddress);
        g_ucispTxBuffer[2] = LOBYTE(nDownloadAddress);
        
        for(j = 0; j < nLength; j ++) {
            g_ucispTxBuffer[nOffset + j] = code.read();
            //nPoint ++;        
        }
        
        nMS10 = (unsigned int)(millis() + ISP_DOWNLOAD_TIMEOUT * 1000);
        
        isp_cmd(nLength + nOffset);
        
        
        for(;;) {
            ispProcChar();
            
            if(g_ucispReceiveFlag) {
                if(g_ucispRxBuffer[0] == 0x2 &&
                   g_ucispRxBuffer[1] == 'T') break;
                   
                return 1;                   
            }
            if(millis() == nMS10) return 2;
        }
        
        g_ucispTxBuffer[0] = 0x02;
        
    }
    
    return 0;
}

bool ISP::FS_bin_isp(FS *fs, const char *filename, isp_chip chip, int band, long chip_fosc){
    File code;
    chip_is = chip;
    if (chip == not_supported_chip){
        GUI::info_msgbox("错误", "无效的选项或不受支持的芯片系列");
        return false;
    }
    download_band = band;
    _chip_fosc = chip_fosc;
    if(!fs->exists(filename)) return false;
    else code = fs->open(filename);
    display.clearScreen();
    u8g2Fonts.drawUTF8(0, 13, "初始化串口");
    display.display(true);
    isp_init();
    u8g2Fonts.drawUTF8(0, 26, "ISP握手...");
    display.display(true);
    error_code[0] = connect_chip();
    if (error_code[0] == 0){ 
        u8g2Fonts.drawUTF8(u8g2Fonts.getUTF8Width("ISP握手..."), 26, "   成功");
        display.display(true);
    }else {
        u8g2Fonts.drawUTF8(u8g2Fonts.getUTF8Width("ISP握手..."), 26, "   失败");
        display.display(true);
        return false;
    }
    u8g2Fonts.drawUTF8(0, 39, "擦除芯片...");
    display.display(true);
    error_code[1] = chip_erase();
    if (error_code[1] == 0){ 
        u8g2Fonts.printf("   成功");
        display.display(true);
    }else {
        u8g2Fonts.printf("   失败");
        display.display(true);
        return false;
    }
    u8g2Fonts.drawUTF8(0, 52, "下载代码...");
    display.display(true);
    error_code[2] = chip_download(code, code.size(), 0);
    if (error_code[2] == 0) {
        u8g2Fonts.printf("   成功");
        display.display(true);
    }else {
        u8g2Fonts.printf("   失败");
        display.display(true);
        return false;
    }
    u8g2Fonts.drawUTF8(0, 65, "ISP下载完成");
    display.display(true);
    return true;
}

ISP isp;