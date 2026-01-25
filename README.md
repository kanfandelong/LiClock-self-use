![:name](https://count.getloli.com/@aLiClock-self-use?name=LiClock-self-use&theme=green&padding=7&offset=0&align=top&scale=1&pixelated=1&darkmode=auto&prefix=0)
# <center>LiClock 墨水屏天气时钟

###  **关于发行版** 
- 基于2.0.10修改，兼容新旧版硬件，群主原工程地址[前往](https://github.com/diylxy/LiClock)
- 本人画不好APP的图标，故新增APP无图标（传感器群主并未绘制图标，若想增加与新增APP相同），若你有意向绘制图标，请在交流群中@我并私聊或发送图标至我的邮箱jiangzihan192959@outlook.com
-  **与原版的对比** 
1. 取消BMP280，AHT20，天气预警，电源，误差补偿，的隐藏
2. 增加了贪吃蛇，文件管理，APP
3. 对设置APP进行了优化，改变了闹钟设定响铃日期的输入方式，编写了关于，在网络设置中新增搜索周围的WIFI，其他设置增加CPU频率设置，SD卡时钟频率设定，长按判断时间设定，以及休眠电压设定（电量不足自动休眠，避免触发电池保护板的保护，导致系统断电，导致DS3231丢失时间），电池电压校准（分为外部仪表校准和芯片eFuse的ADC校准数据），TF加载方式（TF卡的电源供给方式，若使用了TF卡：1.与系统休眠一同断电。2.卸载TF卡（APP不再请求TF的使用）才断电）
4. 增强app管理，允许自定义显示在app列表中的app
-  **关于文件管理** 
- 关于退出，随意选择一个文件，在弹出的选项列表中选择退出
- 关于重命名，举例，SD卡的根目录有一个文件a.txt，若要改为b.txt,实际要输入/b.txt,实际修改时有提示（将会考虑修改）
- 关于复制，选择文件夹时，默认就是选择/userdat，~~（将会考虑根目录的问题）~~，不支持同文件系统内复制，仅支持littlefs<-->TF卡（FAT16/FAT32），在littlefs-->TF卡时暂时不考虑剩余空间是否足够的问题（TF卡-->littlefs时会考虑）
- 可以运行lua脚本，且支持热加载lua配置文件中请求的外设，
- 播放buz文件，显示jpg/bmp，以hex查看文件，lbm文件支持2/4/16阶灰（需使用py转换脚本或image2lcd转换，且目前灰度显示（4/16阶灰）仅在UC8151上支持）
-  **关于贪吃蛇** 
- 左键是蛇头逆时针旋转，右键顺时针
- 中键为菜单
- 提高CPU频率会提高运行速度 
### BUG反馈
- 如果有报错重启请提供日志
-  **反馈渠道** 
- 在交流群中@我
- 发送至邮箱，jiangzihan192959@outlook.com
### <center>一种兼具易用性与扩展性的多功能墨水屏天气时钟 
![封面](images/封面.png)
## <center>硬件购买注意事项
屏幕型号：`E029A01`  
ESP32：wroom或者其它封装和引脚兼容的模组，建议Flash选大一点  
### **尽量不要买esp32-solo-1，虽然能用，但价格没有任何优势，除非用拆机件**  
其它照着BOM买就行，买之前请认真阅读开源平台下面的DIY注意事项  
### 元器件购买相关说明[请看Wiki](https://github.com/diylxy/LiClock/wiki/%E5%85%83%E5%99%A8%E4%BB%B6%E8%B4%AD%E4%B9%B0)
---
## <center>软件使用说明

### 程序烧录[请看Wiki](https://github.com/diylxy/LiClock/wiki/%E5%9B%BA%E4%BB%B6%E7%83%A7%E5%BD%95)  

### 手动编译固件[请看Wiki](https://github.com/diylxy/LiClock/wiki/%E6%89%8B%E5%8A%A8%E7%BC%96%E8%AF%91%E5%9B%BA%E4%BB%B6)  

---
### 拨轮开关使用说明
|  按键   | 短按功能  | 长按功能 |
|  ----  | ----  | ---- |
| 左键  | 输入数字/时间：当前位-1 | 返回上个App<br/>输入数字/时间：光标左移<br/>电子书：上一页 |
| 右键  | 输入数字/时间：当前位+1 | 输入数字/时间：光标右移<br/>电子书：下一页|
| 中键  | 确认 | 重置输入为默认值 |
---
### 点此查看[Lua App编写规范](src/lua/README.md)  

## <center>Blockly
~~因为现在的Lua语言与Blockly并未完全适配，有些“积木”后续会进行修改，其中包括：~~  

### Blockly使用教程  
暂无，挂一张照片在这吧  
![封面](images/Blockly屏幕截图.png)

## <center>其它
### midi转buz[请看Wiki](https://github.com/diylxy/LiClock/wiki/midi%E8%BD%ACbuz)  

### 图像转lbm[请看Wiki](https://github.com/diylxy/LiClock/wiki/%E5%9B%BE%E5%83%8F%E8%BD%AClbm)  

## <center> 开源协议
### 因为用了彩云天气的API，仅供学习研究，如果需要商用，则不得包含此源代码或由其产生的二进制文件  
### 本项目src路径下的代码没有使用其它任何与墨水屏相关的项目代码  
源代码开源协议为**GPL-3.0**，允许开源情况下商用，但请标明原作者和工程链接，不得售卖源代码或作为闭源项目发布  
另外，按照GPL-3.0协议要求，由此项目衍生出的代码也需要以GPL3.0开源  
此处的开源指使任何人可以自由且免费地获得、修改**源代码**和（或）**硬件工程源文件**  
