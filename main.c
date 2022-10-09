#include <sys/prctl.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

#ifdef LUA_VERSION
#define CHECK_LUA(L, err, format, ...) do { \
    if(err != LUA_OK) { \
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": %s\n", ##__VA_ARGS__, lua_tostring(L, -1)); \
    } \
} while(0)
#endif

int main(int argc, char* argv[])
{
    int r = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    CHECK(r, "enabling 'no new privileges'");

    lua_State* L = luaL_newstate();
    CHECK_NOT(L, NULL, "unable to create Lua state");

    luaL_openlibs(L);

    const char* fn = argv[1];
    r = luaL_loadfile(L, fn);
    CHECK_LUA(L, r, "luaL_loadfile(%s)", fn);

    r = lua_pcall(L, 0, LUA_MULTRET, 0);
    CHECK_LUA(L, r, "lua_pcall");

    lua_close(L);

    return 0;
}
