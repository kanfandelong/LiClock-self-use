/*----------------------------------------------*/
/* TJpgDec System Configurations R0.03          */
/*----------------------------------------------*/

#define	JD_SZBUF		512
/* 指定流输入缓冲区的大小 */

#define JD_FORMAT		2
/* 指定输出像素格式。
/  0: RGB888 (24-bit/pix)
/  1: RGB565 (16-bit/pix)
/  2: Grayscale (8-bit/pix)
*/

#define	JD_USE_SCALE 1 
/* 开关缩放功能.
/  0: Disable
/  1: Enable
*/

#define JD_TBLCLIP		0
/* 使用表格转换进行饱和运算。速度稍快，但增加了1KB的代码大小。
/  0: Disable
/  1: Enable
*/

#define JD_FASTDECODE	0  //8266像素抖动用0比较好
/* 优化级别
/  0: 基本优化。适用于8/16位MCU。
/     需要3100字节的工作区。
/  1: + 32位桶移位器。适用于32位MCU。
/     需要3480字节的工作区。
/  2: + 哈夫曼解码的表格转换（需要6<<HUFF_BIT字节的RAM）。
/     需要9644字节的工作区。
*/

// 不要改变这一点，它是解码器所需工作空间的最小字节大小
#if JD_FASTDECODE == 0
 #define TJPGD_WORKSPACE_SIZE 3100
#elif JD_FASTDECODE == 1
 #define TJPGD_WORKSPACE_SIZE 3500
#elif JD_FASTDECODE == 2
 #define TJPGD_WORKSPACE_SIZE (3500 + 6144)
#endif