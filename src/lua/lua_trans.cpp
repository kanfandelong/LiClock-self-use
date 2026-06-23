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

// 带参数版本（arg[0]=文件名，arg[1..]为传入参数）
void lua_execute(const char *filename, int argc, const char **argv)
{
    // 1. 加载文件
    if (luaL_loadfile(L, filename) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        if (!err) err = "(unknown load error)";
        lua_printf("加载文件错误: %s", err);
        lua_pop(L, 1);
        return;
    }

    // 2. 构造全局 arg 表
    lua_newtable(L);
    lua_pushinteger(L, 0);
    lua_pushstring(L, filename);
    lua_settable(L, -3);
    for (int i = 0; i < argc; i++) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, argv[i] ? argv[i] : ""); // 防止空指针
        lua_settable(L, -3);
    }
    lua_setglobal(L, "arg");

    // 3. 执行（无错误处理函数，与 luaL_dofile 相同）
    int status = lua_pcall(L, 0, 0, 0);

    // 4. 处理执行错误
    if (status != LUA_OK) {
        // 获取原始错误信息
        const char *err = lua_tostring(L, -1);
        if (!err) {
            lua_printf("运行错误: 未知错误 (类型: %s)", lua_typename(L, lua_type(L, -1)));
            lua_pop(L, 1);
            return;
        }

        // 尝试生成带堆栈的完整错误信息
        const char *full_err = err;  // 默认使用原始错误
        lua_getglobal(L, "debug");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "traceback");
            if (lua_isfunction(L, -1)) {
                // 安全调用 debug.traceback(err, 2)
                lua_pushstring(L, err);
                lua_pushinteger(L, 2);
                if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
                    const char *trace = lua_tostring(L, -1);
                    if (trace && trace[0] != '\0') {
                        full_err = trace;   // 使用堆栈信息
                    } else {
                        // 如果 trace 为空，保留原始错误，但需要弹出空字符串
                        lua_pop(L, 1);
                    }
                } else {
                    // debug.traceback 调用失败，弹出错误信息（忽略）
                    lua_pop(L, 1);
                }
            } else {
                lua_pop(L, 1); // 弹出 debug 表（traceback 不存在）
            }
            lua_pop(L, 1); // 弹出 debug 表（如果未弹出）
        } else {
            // debug 库不可用，仅保留原始错误
        }

        // 打印最终错误信息（此时 full_err 必定非空）
        lua_printf("运行错误: %s", full_err);

        // 如果 full_err 指向的是 trace 字符串（即栈顶），需要弹出它
        // 注意：如果 full_err 仍是原始错误，原始错误仍在栈顶，同样弹出
        lua_pop(L, 1); // 弹出错误信息
    }
}

// 兼容旧调用：无参数版本
void lua_execute(const char *filename) {
    lua_execute(filename, 0, NULL);
}

void closeLua()
{
    if (L)
        lua_close(L);
    L = NULL;
}
