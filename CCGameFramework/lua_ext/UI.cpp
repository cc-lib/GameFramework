#include "stdafx.h"
#include "UI.h"
#include "ext.h"

extern CTraceCategoryEx<CTraceCategoryEx<>::TraceUser> atlTraceLua(_T("atlTraceLua"));

static int ui_trace(lua_State *L)
{
    auto s = luaL_checkstring(L, 1);
    ATLTRACE(atlTraceLua, 0, "%s\n", s);
    return 0;
}

static const luaL_Reg ui_lib[] = {
    { "trace", ui_trace },
    { "clear_scene", ui_clear_scene },
    { "add_obj", ui_add_obj },
    { "update", ui_update_obj },
    { "info", ui_info },
    { "set_minw", ui_win_set_minsize },
    { "paint", ui_paint },
    { "set_timer", ui_set_timer },
    { "kill_timer", ui_kill_timer },
    { "quit", ui_quit },
    { "hsb2rgb", ui_helper_hsl2rgb },
    { "rgb2hsb", ui_helper_rgb2hsl },
    { nullptr, nullptr }
};

static int luaopen_UI(lua_State *L) {
    luaL_newlib(L, ui_lib);
    return 1;
}

void lua_ext_register_UI(lua_State *L)
{
    luaL_requiref(L, "UIExt", luaopen_UI, 1);
}

#define HSLMAX 240
#define RGBMAX 255
#define UNDEFINED (HSLMAX*2/3)

static int hue2rgb(short n1, short n2, short hue)
{
    if (hue < 0)
        hue += HSLMAX;
    if (hue > HSLMAX)
        hue -= HSLMAX;

    if (hue < (HSLMAX / 6))
        return (n1 + (((n2 - n1)*hue + (HSLMAX / 12)) / (HSLMAX / 6)));
    if (hue < (HSLMAX / 2))
        return (n2);
    if (hue < ((HSLMAX * 2) / 3))
        return (n1 + (((n2 - n1)*(((HSLMAX * 2) / 3) - hue) + (HSLMAX / 12)) / (HSLMAX / 6)));
    return (n1);
}

int ui_helper_hsl2rgb(lua_State* L)
{
    auto h = short(luaL_checkinteger(L, 1));
    auto s = short(luaL_checkinteger(L, 2));
    auto l = short(luaL_checkinteger(L, 3));
    
    byte r, g, b;
    short Magic1, Magic2;

    if (0 == s)
    {
        r = g = b = (l*RGBMAX) / HSLMAX;
    }
    else
    {
        if (l <= (HSLMAX / 2))
            Magic2 = (l*(HSLMAX + s) + (HSLMAX / 2)) / HSLMAX;
        else
            Magic2 = l + s - ((l*s) + (HSLMAX / 2)) / HSLMAX;

        Magic1 = 2 * l - Magic2;

        r = (hue2rgb(Magic1, Magic2, h + (HSLMAX / 3))*RGBMAX + (HSLMAX / 2)) / HSLMAX;
        g = (hue2rgb(Magic1, Magic2, h)*RGBMAX + (HSLMAX / 2)) / HSLMAX;
        b = (hue2rgb(Magic1, Magic2, h - (HSLMAX / 3))*RGBMAX + (HSLMAX / 2)) / HSLMAX;
    }

    CStringA str;
    str.AppendFormat("#%02X%02X%02X", (byte)(r * 255.0), (byte)(g * 255.0), (byte)(b * 255.0));
    lua_pushlstring(L, str.GetBuffer(), str.GetLength());

    return 1;
}

int ui_helper_rgb2hsl(lua_State* L)
{
    auto r = byte(luaL_checkinteger(L, 1));
    auto g = byte(luaL_checkinteger(L, 2));
    auto b = byte(luaL_checkinteger(L, 3));
    auto cMax = __max(r, __max(g, b));
    auto cMin = __min(r, __min(g, b));

    double h, s, l;
    short Rdelta, Gdelta, Bdelta;

    l = (((cMax + cMin)*HSLMAX) + RGBMAX) / (2 * RGBMAX);

    if (cMax == cMin)
    {
        s = 0;
        h = UNDEFINED;
    }
    else
    {
        if (l <= (HSLMAX / 2))
            s = (((cMax - cMin)*HSLMAX) + ((cMax + cMin) / 2)) / (cMax + cMin);
        else
            s = (((cMax - cMin)*HSLMAX) + ((2 * RGBMAX - cMax - cMin) / 2)) / (2 * RGBMAX - cMax - cMin);

        Rdelta = (((cMax - r)*(HSLMAX / 6)) + ((cMax - cMin) / 2)) / (cMax - cMin);
        Gdelta = (((cMax - g)*(HSLMAX / 6)) + ((cMax - cMin) / 2)) / (cMax - cMin);
        Bdelta = (((cMax - b)*(HSLMAX / 6)) + ((cMax - cMin) / 2)) / (cMax - cMin);

        if (r == cMax)
            h = Bdelta - Gdelta;
        else if (g == cMax)
            h = (HSLMAX / 3) + Rdelta - Bdelta;
        else
            h = ((2 * HSLMAX) / 3) + Gdelta - Rdelta;

        if (h < 0)
            h += HSLMAX;
        if (h > HSLMAX)
            h -= HSLMAX;
    }

    lua_pushnumber(L, h);
    lua_pushnumber(L, s);
    lua_pushnumber(L, l);

    return 3;
}