#include "AppManager.h"

class Appsnake : public AppBase
{
private:
    /* data */
public:
    Appsnake()
    {
        name = "snake";
        title = "贪吃蛇";
        description = "墨水屏贪吃蛇";
        image = NULL;
        noDefaultEvent = true;
        _showInList = true;
    }
    void changeR();
    void changeL();
    void spawnFood();
    bool isSnakeAt(int16_t x, int16_t y);
    void resetGame();
    void drawGame();
    void moveSnake();
    void zhuangqiang();
    void chiziij();
    void eatFood();
    void saveHighscore();
    void menu();
    void setup();
};
static Appsnake snake;

const uint8_t snakeSize = 4;  // 蛇身单位尺寸
const uint8_t screenWidth = 288 / snakeSize;
const uint8_t screenHeight = 103 / snakeSize;

const uint8_t wallOffsetX = 0;   // 墙距离屏幕左右边缘的距离
const uint8_t wallOffsetY = 21;  // 墙距离屏幕上边缘的距离

bool end = false;

struct Point {
  int8_t x;
  int8_t y;
};

Point snakeshuzu[screenWidth * screenHeight];  // 蛇身数组
uint8_t snakeLength;
Point food;         // 食物坐标
int score = 0;      // 分数
int highscore = 0;  // 最高分数
enum Direction { UP,
                 DOWN,
                 LEFT,
                 RIGHT };  // 方向枚举
Direction snakeDirection = RIGHT;
unsigned long moveInterval = 100;
unsigned long moveTimer = 0;
unsigned long lastChangeTime = 0;

void Appsnake::changeR() {
  // 检查距离上次切换的时间是否足够长，如果是则切换方向
  if (millis() - lastChangeTime > 200) {
    Serial.println("按键1按下");
    lastChangeTime = millis();
    if (snakeDirection == UP) {
      snakeDirection = RIGHT;
    } else if (snakeDirection == RIGHT) {
      snakeDirection = DOWN;
    } else if (snakeDirection == DOWN) {
      snakeDirection = LEFT;
    } else if (snakeDirection == LEFT) {
      snakeDirection = UP;
    }
  }
}

void Appsnake::changeL() {
  // 检查距离上次切换的时间是否足够长，如果是则切换方向
  if (millis() - lastChangeTime > 200) {
    Serial.println("按键2按下");
    lastChangeTime = millis();
    if (snakeDirection == UP) {
      snakeDirection = LEFT;
    } else if (snakeDirection == LEFT) {
      snakeDirection = DOWN;
    } else if (snakeDirection == DOWN) {
      snakeDirection = RIGHT;
    } else if (snakeDirection == RIGHT) {
      snakeDirection = UP;
    }
  }
}


void Appsnake::spawnFood() {
  // 随机生成食物坐标
  do {
    food.x = random(screenWidth);
    food.y = random(screenHeight);
  } while (isSnakeAt(food.x, food.y));  // 如果和蛇身重合，则重新生成坐标
}

bool Appsnake::isSnakeAt(int16_t x, int16_t y) {
  // 检查蛇身是否在指定的坐标上
  for (uint8_t i = 0; i < snakeLength; i++) {
    if (snakeshuzu[i].x == x && snakeshuzu[i].y == y) {
      return true;
    }
  }
  return false;
}

void Appsnake::resetGame() {
  snakeLength = 5;
  for (int i = 0; i < screenWidth * screenHeight; i++) {
    snakeshuzu[i].x = -1;
    snakeshuzu[i].y = -1;
  }
  snakeshuzu[0].x = 10;
  snakeshuzu[0].y = 10;
  for (uint8_t i = 1; i < snakeLength; i++) {
    snakeshuzu[i].x = snakeshuzu[i - 1].x - 1;
    snakeshuzu[i].y = snakeshuzu[i - 1].y;
  }
  score = 0;
  drawGame();
}

void Appsnake::drawGame() {

  display.fillScreen(GxEPD_WHITE);

  // 画蛇
  for (uint8_t i = 0; i < snakeLength; i++) {
    display.fillRect((snakeshuzu[i].x * snakeSize) + wallOffsetX, (snakeshuzu[i].y * snakeSize) + wallOffsetY, snakeSize, snakeSize, GxEPD_BLACK);
  }

  // 画食物
  display.fillRect((food.x * snakeSize) + wallOffsetX, (food.y * snakeSize) + wallOffsetY, snakeSize, snakeSize, GxEPD_BLACK);


  // 显示分数
  u8g2Fonts.setCursor(5, 15);
  u8g2Fonts.print("得分:");
  u8g2Fonts.setCursor(35, 15);
  u8g2Fonts.print(score);



  u8g2Fonts.setCursor(228, 15);
  u8g2Fonts.print("最高:");
  u8g2Fonts.setCursor(270, 15);
  u8g2Fonts.print(highscore);

  u8g2Fonts.setCursor(109, 15);
  u8g2Fonts.print("LiClock-贪吃蛇");

  display.drawRoundRect(4, 21, 288, 102, 2, GxEPD_BLACK);

  //Serial.println(score);

  display.display(true);
}

void Appsnake::moveSnake() {

  // 保存蛇头位置
  Point head = snakeshuzu[0];

  // 从尾部开始，移动蛇身
  for (uint8_t i = snakeLength - 1; i > 0; i--) {
    snakeshuzu[i].x = snakeshuzu[i - 1].x;
    snakeshuzu[i].y = snakeshuzu[i - 1].y;
  }

  // 将蛇头移动到新的位置
  switch (snakeDirection) {
    case UP:
      head.y--;
      break;
    case DOWN:
      head.y++;
      break;
    case LEFT:
      head.x--;
      break;
    case RIGHT:
      head.x++;
      break;
  }

  // 碰撞检测
  if (head.x < 0 || head.x >= screenWidth || head.y < 0 || head.y >= screenHeight) {
    // 蛇头撞墙
    zhuangqiang();
    // 等待按键输入
    while (!hal.btnl.isPressing() && !hal.btnr.isPressing() && !hal.btnc.isPressing()) {
      delay(50);
    }

    resetGame();
    return;
  }

  for (int i = 1; i < snakeLength; i++) {
    if (head.x == snakeshuzu[i].x && head.y == snakeshuzu[i].y) {
      // 蛇头撞到自己身体
      Serial.println("吃自己了！！！！！！！！");
      chiziij();
      // 等待按键输入
      while (!hal.btnl.isPressing() && !hal.btnr.isPressing() && !hal.btnc.isPressing()) {
        delay(50);
      }

      resetGame();
      return;
    }
  }

  // 将蛇头放回数组的第一个位置
  snakeshuzu[0] = head;

  if (snakeshuzu[0].x == food.x && snakeshuzu[0].y == food.y) {
    // 吃到食物
    eatFood();
  }
  drawGame();
}

void Appsnake::zhuangqiang() {

  display.fillScreen(GxEPD_WHITE);
  u8g2Fonts.setCursor(80, 59);
  u8g2Fonts.print("怎么玩的，别往墙上撞啊!");
  if (score > highscore) {
    highscore = score;
    saveHighscore();
    u8g2Fonts.setCursor(61, 75);
    u8g2Fonts.print("但是恭喜获得新纪录：最高分");
    u8g2Fonts.setCursor(218, 75);
    u8g2Fonts.print(highscore);
  }
  display.display(true);
}

void Appsnake::chiziij() {
  display.fillScreen(GxEPD_WHITE);
  u8g2Fonts.setCursor(80, 59);
  u8g2Fonts.print("吃食物啊，吃自己干嘛!");
  if (score > highscore) {
    highscore = score;
    saveHighscore();
    u8g2Fonts.setCursor(61, 75);
    u8g2Fonts.print("但是恭喜获得新纪录：最高分");
    u8g2Fonts.setCursor(218, 75);
    u8g2Fonts.print(highscore);
  }

  display.display(true);
}

void Appsnake::eatFood() {
  // 增加蛇的长度
  snakeLength++;
  score += 1;  // 将分数增加10
  // 将新的蛇头位置添加到数组的第一个元素
  for (uint8_t i = snakeLength - 1; i > 0; i--) {
    snakeshuzu[i].x = snakeshuzu[i - 1].x;
    snakeshuzu[i].y = snakeshuzu[i - 1].y;
  }
  snakeshuzu[0].x = food.x;
  snakeshuzu[0].y = food.y;
  // 生成新的食物坐标
  spawnFood();
}

void Appsnake::saveHighscore() {
  File file = LittleFS.open("/dat/snakehighscore.txt", "w");
  if (file) {
    file.print(highscore);
    file.close();
  }
  //printHighscore();
}

void Appsnake::menu()
{
    static const menu_item appMenu_main[] = {
    {NULL, "返回"},
    {NULL, "退出"},
    //{NULL, buf},
    {NULL, NULL},
    };
    int res = GUI::menu("菜单",appMenu_main);
    switch (res)
        {
        case 0:
            break;
        case 1:
            end = true;
            appManager.goBack();
            break;
        case 2:
            saveHighscore();
            break;
        default:
            break;
        }
}

void Appsnake::setup()
{
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);  // 设置前景色
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);  // 设置背景色
    if(LittleFS.exists("/dat") == false){LittleFS.mkdir("/dat");}
    File file = LittleFS.open("/dat/snakehighscore.txt", "r");
    if (file) {
        highscore = file.parseInt();
        Serial.println("读取最高分");
        file.close();
    }
    display.fillScreen(GxEPD_WHITE);

    int logoX = 76;
    int logoY = 104;
    int moveStep = -8;  // 每次向上移动4个像素

    display.fillScreen(GxEPD_WHITE);
    logoY += moveStep;
    //display.drawInvertedBitmap(logoX, logoY, logo, 143, 66, GxEPD_BLACK);
    u8g2Fonts.setCursor(88, logoY);
    u8g2Fonts.print("按任意按键开始游戏");
    display.display();

    // 等待按键输入
    while (!hal.btnl.isPressing() && !hal.btnr.isPressing() && !hal.btnc.isPressing()) {
        delay(50);
    }
    display.fillScreen(GxEPD_WHITE);
    spawnFood();
    resetGame();
    int a = 0;
    while(end == false)
    {
        a++;
        if(hal.btnr.isPressing()){changeR();}
        if(hal.btnl.isPressing()){changeL();}
        if(hal.btnc.isPressing()){menu();}
        if(a > 20){a = 0;display.display();}
        if (millis() - moveTimer > moveInterval) {
            moveTimer = millis();
            moveSnake();
        }
    }

}
