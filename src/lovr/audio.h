#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "vendor/map/map.h"

map_int_t TimeUnits;

extern const luaL_Reg lovrAudio[];
int l_lovrAudioInit(lua_State* L);
int l_lovrAudioUpdate(lua_State* L);
int l_lovrAudioGetOrientation(lua_State* L);
int l_lovrAudioGetPosition(lua_State* L);
int l_lovrAudioGetVolume(lua_State* L);
int l_lovrAudioNewSource(lua_State* L);
int l_lovrAudioPause(lua_State* L);
int l_lovrAudioResume(lua_State* L);
int l_lovrAudioRewind(lua_State* L);
int l_lovrAudioSetOrientation(lua_State* L);
int l_lovrAudioSetPosition(lua_State* L);
int l_lovrAudioSetVolume(lua_State* L);
int l_lovrAudioStop(lua_State* L);