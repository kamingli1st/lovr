#include "lib/maf.h"
#include "math/pool.h"
#include "math/randomGenerator.h"

void luax_pushlightmathtype(lua_State* L, float* p, MathType type);
float* luax_tomathtype(lua_State* L, int index, MathType* type);
float* luax_checkmathtype(lua_State* L, int index, MathType type, const char* expected);
float* luax_newmathtype(lua_State* L, MathType type);
int luax_readvec3(lua_State* L, int index, vec3 v, const char* expected);
int luax_readscale(lua_State* L, int index, vec3 v, int components, const char* expected);
int luax_readquat(lua_State* L, int index, quat q, const char* expected);
int luax_readmat4(lua_State* L, int index, mat4 m, usize scaleComponents);
Seed luax_checkrandomseed(lua_State* L, int index);
int l_lovrRandomGeneratorRandom(lua_State* L);
int l_lovrRandomGeneratorRandomNormal(lua_State* L);
int l_lovrRandomGeneratorGetSeed(lua_State* L);
int l_lovrRandomGeneratorSetSeed(lua_State* L);
int l_lovrVec3Set(lua_State* L);
int l_lovrQuatSet(lua_State* L);
int l_lovrMat4Set(lua_State* L);
