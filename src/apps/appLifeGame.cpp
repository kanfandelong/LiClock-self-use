#include "AppManager.h"

class AppLifeGame : public AppBase
{
private:
    volatile bool exitFlag = false;
    uint32_t generation = 0; // 世代计数器
    
    static void onLeftClick(void* arg) {
        AppLifeGame* app = (AppLifeGame*)arg;
        app->exitFlag = true;
    }

public:
    AppLifeGame()
    {
        name = "life";
        title = "生命游戏";
        description = "康威元胞自动机";
        image = NULL;
    }
    void set() { _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true); }
    void setup();
};
static AppLifeGame app;

// ---------- 像素读写宏（保持你原有的正确版本） ----------
#define GET_PIXEL(buf, c, r) \
    (((buf)[((c) >> 1) * TOTAL_ROWS + ((r) >> 2)] >> (7 - (((r) & 3) * 2 + ((c) & 1)))) & 1)

#define SET_PIXEL(buf, c, r, val) do { \
    uint16_t idx = ((c) >> 1) * TOTAL_ROWS + ((r) >> 2); \
    uint8_t bit = 7 - (((r) & 3) * 2 + ((c) & 1)); \
    if (val) buf[idx] |= (1 << bit); \
    else buf[idx] &= ~(1 << bit); \
} while(0)

void AppLifeGame::setup()
{
    // ---------- 1. 高密度随机撒点 ----------
    display.clearScreen(TFT_BLACK);
    // 直接学 SDL 例程，撒 2 万个活细胞，密度约 31%，绝对够生成混沌
    for (int i = 0; i < 20000; i++) {
        display.drawPixel(random(MAX_X), random(MAX_Y), TFT_WHITE);
    }
    display.display(false); // 初始显示

    // ---------- 2. 注册左键返回 ----------
    hal.btnl.attachClick(onLeftClick, this);

    // ---------- 3. 核心迭代函数（纯粹的 0/1 位图判定，无需年龄） ----------
    auto updateLife = [&](uint8_t* curr, uint8_t* next) {
        memset(next, 0, BYTES_PER_BUFFER);

        for (int c = 0; c < MAX_X; c++) {
            for (int r = 0; r < MAX_Y; r++) {
                // 计算环形边界邻居
                int neighbors = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nc = c + dx, nr = r + dy;
                        if (nc < 0) nc += MAX_X; else if (nc >= MAX_X) nc -= MAX_X;
                        if (nr < 0) nr += MAX_Y; else if (nr >= MAX_Y) nr -= MAX_Y;
                        neighbors += GET_PIXEL(curr, nc, nr);
                    }
                }

                int alive = GET_PIXEL(curr, c, r);
                
                if (alive) {
                    // 活细胞：邻居 2 或 3 存活
                    if (neighbors == 2 || neighbors == 3) {
                        SET_PIXEL(next, c, r, 1);
                    }
                } else {
                    // 死细胞：邻居 3 诞生
                    if (neighbors == 3) {
                        SET_PIXEL(next, c, r, 1);
                    }
                }
            }
        }
    };

    // ---------- 4. 帧率控制 + 500代重置 ----------
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);
    generation = 0;

    while (1) {
        if (exitFlag) return;

        int cur_idx = display.current_buffer_idx;
        uint8_t* curr = display.getBuffer();
        int next_idx = (cur_idx + 1) % 2;
        display.swapBuffer(next_idx);
        uint8_t* next = display.getBuffer();

        // 计算下一代
        updateLife(curr, next);
        generation++;

        // ===== SDL 例程的精髓：500 代强制大重置 =====
        if (generation >= 500) {
            memset(next, 0, BYTES_PER_BUFFER);
            // 再次撒入 20000 个细胞，掀起新一轮混沌
            for (int i = 0; i < 20000; i++) {
                SET_PIXEL(next, random(MAX_X), random(MAX_Y), 1);
            }
            generation = 0; // 重置世代计数器
        }
        // ============================================

        // 异步 DMA 刷新
        display.display(false);

        // 挂起任务
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}