#include "A_Config.h"

const char *err_invalid_param = "参数个数不符";
lua_State *L = NULL; // 公用Lua虚拟机状态
extern char __mypath[101];

static int common_delay(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int ms = luaL_checkinteger(L, 1);
    delay(ms);
    return 0;
}

static int common_NextWakeup(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int nwu = luaL_checkinteger(L, 1);
    appManager.nextWakeup = nwu;
    return 0;
}

static int common_digitalRead(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int pin = luaL_checkinteger(L, 1);
    int ret = digitalRead(pin);
    lua_pushinteger(L, ret);
    return 1;
}

static int common_digitalWrite(lua_State *L)
{
    if (lua_gettop(L) != 2)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int pin = luaL_checkinteger(L, 1);
    int val = luaL_checkinteger(L, 2);
    digitalWrite(pin, val);
    return 0;
}

static int common_analogRead(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int pin = luaL_checkinteger(L, 1);
    int ret = analogRead(pin);
    lua_pushinteger(L, ret);
    return 1;
}

static int common_adc_attenuation(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int pin = luaL_checkinteger(L, 1);
    int attenuation = luaL_checkinteger(L, 2);
	analogSetPinAttenuation(pin, (adc_attenuation_t)attenuation);
    return 1;	
}

static int common_adc_bit(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int adc_bit = luaL_checkinteger(L, 1);
	analogReadResolution(adc_bit);
    return 1;	
}

static int common_pin_capability(lua_State *L)
{
    if (lua_gettop(L) != 1)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int pin = luaL_checkinteger(L, 1);
	int cap = luaL_checkinteger(L, 2);
    gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)cap);
    return 1;
}

static int common_pinMode(lua_State *L)
{
    if (lua_gettop(L) != 2)
    {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    int pin = luaL_checkinteger(L, 1);
    int mode = luaL_checkinteger(L, 2);
    pinMode(pin, mode);
    return 0;
}

void openLua_simple()
{
    if (L)
        return;
    log_printf("Lua 部分初始化\n");
    L = luaL_newstate();
}

void openLua()
{
    if (L)
        return;
    log_printf("Lua 初始化\n");
    L = luaL_newstate();
    luaL_openlibs(L);
    lua_pushcfunction(L, common_delay);
    lua_setglobal(L, "delay");
    lua_pushcfunction(L, common_NextWakeup);
    lua_setglobal(L, "nextWakeup");
    lua_pushcfunction(L, common_digitalRead);
    lua_setglobal(L, "digitalRead");
    lua_pushcfunction(L, common_digitalWrite);
    lua_setglobal(L, "digitalWrite");
    lua_pushcfunction(L, common_analogRead);
    lua_setglobal(L, "analogRead");
	lua_pushcfunction(L, common_adc_attenuation);
    lua_setglobal(L, "analogSetPinAttenuation");
	lua_pushcfunction(L, common_adc_bit);
    lua_setglobal(L, "analogReadResolution");
    lua_pushcfunction(L, common_pin_capability);
    lua_setglobal(L, "gpio_set_drive_capability");
    lua_pushcfunction(L, common_pinMode);
    lua_setglobal(L, "pinMode");
}

// 自定义错误处理函数（在 Lua 栈顶压入带堆栈的错误信息）
static int traceback_error_handler(lua_State *L) {
    // 调用 debug.traceback，传入当前错误对象和消息级别（2 表示省略本函数）
    lua_getglobal(L, "debug");
    if (!lua_istable(L, -1)) {
        // 如果 debug 表不存在（嵌入式裁剪版可能没有），则直接返回原始错误
        lua_pop(L, 1);
        lua_pushstring(L, "（无 debug 库）");
        lua_insert(L, 1);  // 将错误信息移到栈底，保持兼容
        return 1;
    }
    lua_getfield(L, -1, "traceback");
    if (!lua_isfunction(L, -1)) {
        // 没有 traceback 函数，同样回退
        lua_pop(L, 2);
        lua_pushstring(L, "（无 traceback 函数）");
        lua_insert(L, 1);
        return 1;
    }
    // 将错误对象（位于栈顶）作为参数传给 traceback
    lua_pushvalue(L, 1);          // 复制原始错误
    lua_pushinteger(L, 2);        // 跳过本函数和 lua_pcall 的帧
    lua_call(L, 2, 1);            // 调用 traceback(error, 2)
    // 现在栈顶是生成的完整堆栈字符串，将其替换掉原始错误
    lua_replace(L, 1);
    lua_pop(L, 1);                // 移除 debug 表
    return 1;                     // 返回堆栈信息
}

// argc: 参数个数 (与命令行一致，传入的文件名不算在内，但 Lua 标准会让 arg[0]=文件名)
// argv: 参数字符串数组
void lua_execute(const char *filename, int argc, const char **argv)
{
    // 1. 加载 Lua 文件
    if (luaL_loadfile(L, filename) != LUA_OK) {
        lua_printf("加载文件错误: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    // 2. 构造全局 'arg' 表（模拟标准 Lua 解释器行为）
    lua_newtable(L);  // 创建表
    
    // 设置 arg[0] = 脚本文件名
    lua_pushinteger(L, 0);
    lua_pushstring(L, filename);
    lua_settable(L, -3);
    
    // 设置 arg[1] 到 arg[argc]（命令行传入的参数）
    for (int i = 0; i < argc; i++) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, argv[i]);
        lua_settable(L, -3);
    }
    
    lua_setglobal(L, "arg");  // 将表赋给全局变量 arg

    // 3. 压入错误处理函数（你之前实现的 debug.traceback）
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_remove(L, -2); // 移除 debug 表

    // 4. 执行
    if (lua_pcall(L, 0, 0, -2) != LUA_OK) {
        lua_printf("运行错误: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    // 5. 清理
    lua_pop(L, 1); // 弹出 traceback 函数
}

void lua_execute(const char *filename)
{
    // 1. 加载 Lua 文件（仅加载，不执行）
    if (luaL_loadfile(L, filename) != LUA_OK) {
        lua_printf("加载文件失败: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    // 2. 压入自定义错误处理函数（使用上面的 C 函数）
    lua_pushcfunction(L, traceback_error_handler);

    // 3. 执行代码块，错误处理函数位于栈底（索引 -2）
    //    参数：0 个参数，0 个返回值，错误处理函数在栈中的索引
    int status = lua_pcall(L, 0, 0, -2);

    // 4. 处理执行结果
    if (status != LUA_OK) {
        // 此时错误信息（已包含堆栈）位于栈顶，由错误处理函数生成
        lua_printf("(E): %s", lua_tostring(L, -1));
        lua_pop(L, 1);   // 弹出错误信息
    }

    // 5. 弹出错误处理函数（无论是否出错，都需要清理）
    lua_pop(L, 1);       // 弹出 traceback 处理函数
}

void closeLua()
{
    if (L)
        lua_close(L);
    L = NULL;
}
