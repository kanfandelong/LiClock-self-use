// lua_truetype.cpp
#include "A_Config.h"

extern const char *err_invalid_param;

// 设置字体文件
static int lua_ttf_setTtfFile(lua_State *L) {
    if (lua_gettop(L) != 1) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    const char* filename = luaL_checkstring(L, 1);
    File file;
    // 打开字体文件
    file = hal.open(filename);
    
    if (!file) {
        lua_pushboolean(L, 0);
        return 1;
    }
    
    uint8_t result = ttf.setTtfFile(file);
    lua_pushboolean(L, result);
    return 1;
}

// 设置帧缓冲参数
static int lua_ttf_setFramebuffer(lua_State *L) {
    if (lua_gettop(L) != 3) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    uint16_t width = luaL_checkinteger(L, 1);
    uint16_t height = luaL_checkinteger(L, 2);
    uint8_t drawbit = luaL_checkinteger(L, 3);
    
    ttf.setFramebuffer(width, height, drawbit);
    return 0;
}

// 设置字符大小
static int lua_ttf_setCharacterSize(lua_State *L) {
    if (lua_gettop(L) != 1) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    uint16_t size = luaL_checkinteger(L, 1);
    ttf.setCharacterSize(size);
    return 0;
}

// 设置字符间距
static int lua_ttf_setCharacterSpacing(lua_State *L) {
    int n = lua_gettop(L);
    if (n < 1 || n > 2) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    int16_t space = luaL_checkinteger(L, 1);
    uint8_t kerning = 1;
    if (n > 1) {
        kerning = luaL_checkinteger(L, 2);
    }
    
    ttf.setCharacterSpacing(space, kerning);
    return 0;
}

// 设置文本边界
static int lua_ttf_setTextBoundary(lua_State *L) {
    if (lua_gettop(L) != 3) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    uint16_t start_x = luaL_checkinteger(L, 1);
    uint16_t end_x = luaL_checkinteger(L, 2);
    uint16_t end_y = luaL_checkinteger(L, 3);
    
    ttf.setTextBoundary(start_x, end_x, end_y);
    return 0;
}

// 设置文本颜色
static int lua_ttf_setTextColor(lua_State *L) {
    if (lua_gettop(L) != 2) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    uint16_t onLine = luaL_checkinteger(L, 1);
    uint16_t inside = luaL_checkinteger(L, 2);
    
    ttf.setTextColor(onLine, inside);
    return 0;
}

// 设置文本旋转
static int lua_ttf_setTextRotation(lua_State *L) {
    if (lua_gettop(L) != 1) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    uint16_t rotation = luaL_checkinteger(L, 1);
    ttf.setTextRotation(rotation);
    return 0;
}

// 绘制文本
static int lua_ttf_textDraw(lua_State *L) {
    if (lua_gettop(L) != 3) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    int16_t x = luaL_checkinteger(L, 1);
    int16_t y = luaL_checkinteger(L, 2);
    String text = luaL_checkstring(L, 3);
    
    ttf.textDraw(x, y, text);
    return 0;
}

// 获取字符串宽度
static int lua_ttf_getStringWidth(lua_State *L) {
    if (lua_gettop(L) != 1) {
        lua_pushstring(L, err_invalid_param);
        lua_error(L);
        return 0;
    }
    
    String text = luaL_checkstring(L, 1);
    uint16_t width = ttf.getStringWidth(text);
    
    lua_pushinteger(L, width);
    return 1;
}

// 结束并清理
static int lua_ttf_end(lua_State *L) {
    ttf.end();
    return 0;
}

static const luaL_Reg lua_truetype_lib[] = {
    {"setTtfFile", lua_ttf_setTtfFile},
    {"setFramebuffer", lua_ttf_setFramebuffer},
    {"setCharacterSize", lua_ttf_setCharacterSize},
    {"setCharacterSpacing", lua_ttf_setCharacterSpacing},
    {"setTextBoundary", lua_ttf_setTextBoundary},
    {"setTextColor", lua_ttf_setTextColor},
    {"setTextRotation", lua_ttf_setTextRotation},
    {"textDraw", lua_ttf_textDraw},
    {"getStringWidth", lua_ttf_getStringWidth},
    {"ttfend", lua_ttf_end},
    {NULL, NULL}
};

extern "C" int luaopen_truetype(lua_State *L) {
    luaL_newlib(L, lua_truetype_lib);
    return 1;
}