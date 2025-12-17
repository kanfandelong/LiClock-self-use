#include "AppManager.h"

class Appsuanshi : public AppBase
{
private:
    /* data */
public:
    Appsuanshi()
    {
        name = "suanshi";
        title = "随机算式";
        description = "模板";
        image = NULL;
    }
    void set();
    void setup();
    void suanshi();
private:
    char _op[3];
    const char* get_op(uint8_t op){
        switch (op) {
            case 0: sprintf(_op,"+"); break;
            case 1: sprintf(_op,"-"); break;
            case 2: sprintf(_op,"*"); break;
            case 3: sprintf(_op,"/"); break;
        }
        return _op;
    }
    int randomInt(int min, int max) {
        return random(min, max + 1);
    }
};
static Appsuanshi app;
char _suanshi[128];
char _result[128];

void Appsuanshi::set(){
    _showInList = hal.pref.getBool(hal.get_char_sha_key(title), true);
}
void Appsuanshi::setup()
{
    suanshi();
    GUI::msgbox("随机算式", _suanshi);
    GUI::msgbox("答案", _result);
    appManager.nextWakeup = 60;
    appManager.noDeepSleep = false;
}

void Appsuanshi::suanshi()
{    // 生成三个随机数
 int num1 = randomInt(1, 100); // 随机数范围可以根据需要调整
int num2 = randomInt(1, 100);
int num3 = randomInt(1, 100);
char operators[] = {'+', '-', '*', '/'};
int opIndex1 = random(0, 4);
int opIndex2 = random(0, 4);
char op1 = operators[opIndex1];
char op2 = operators[opIndex2];
int result;

// 确保除数不为零，并且结果为非负整数
if (op1 == '/') {
    num2 = randomInt(1, num1); // 确保 num1 / num2 的结果为整数
}
if (op2 == '/') {
    num3 = randomInt(1, num2); // 确保 num2 / num3 的结果为整数
}

// 计算中间结果
int intermediateResult;
switch (op1) {
    case '+':
        intermediateResult = num1 + num2;
        break;
    case '-':
        intermediateResult = num1 - num2;
        break;
    case '*':
        intermediateResult = num1 * num2;
        break;
    case '/':
        intermediateResult = num1 / num2;
        break;
}

// 根据中间结果调整 num3 的范围
switch (op2) {
    case '+':
        num3 = randomInt(1, 100 - intermediateResult); // 确保 intermediateResult + num3 不超过 100
        break;
    case '-':
        num3 = randomInt(1, intermediateResult); // 确保 intermediateResult - num3 为非负数
        break;
    case '*':
        num3 = randomInt(1, 100 / intermediateResult); // 确保 intermediateResult * num3 不超过 100
        break;
    case '/':
        num3 = randomInt(1, intermediateResult); // 确保 intermediateResult / num3 的结果为整数
        break;
}

// 计算最终结果
switch (op2) {
    case '+':
        result = intermediateResult + num3;
        break;
    case '-':
        result = intermediateResult - num3;
        break;
    case '*':
        result = intermediateResult * num3;
        break;
    case '/':
        result = intermediateResult / num3;
        break;
}

// 打印生成的算式和结果
Serial.print(num1);
Serial.print(op1);
Serial.print(num2);
Serial.print(op2);
Serial.print(num3);
Serial.print(" = ");
Serial.println(result);

// 构建表达式字符串
sprintf(_result, "%d %c %d %c %d = %d", num1, op1, num2, op2, num3, result);
sprintf(_suanshi, "%d %c %d %c %d = ", num1, op1, num2, op2, num3);
}
