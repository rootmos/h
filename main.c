#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

#define LUA_EXPECT_TYPE(L, t, expected, format, ...) do { \
    if(t != expected) {\
        r_failwith(__extension__ __FUNCTION__, __extension__ __FILE__, \
                   __extension__ __LINE__, 0, \
                   format ": unexpected type %s (expected %s)\n", \
                   ##__VA_ARGS__, lua_typename(L, t), \
                   lua_typename(L, expected)); \
    } \
} while(0)

void seccomp_apply_filter()
{
    struct sock_filter filter[] = {
#include "filter.bpfc"
    };

    struct sock_fprog p = { .len = LENGTH(filter), .filter = filter };
    int r = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &p);
    CHECK(r, "prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
}

void remove_stdlib_function(struct lua_State* L,
                            const char* lib, const char* f)
{
    int T = lua_gettop(L);

    int t = lua_getglobal(L, lib);
    LUA_EXPECT_TYPE(L, t, LUA_TTABLE, "%s", lib);

    lua_pushnil(L);
    lua_setfield(L, -2, f);
    lua_pop(L, 1);
    CHECK_IF(T != lua_gettop(L), "redundant stack elements present");
}

int main(int argc, char* argv[])
{
    const char* fn = argv[1];

    no_new_privs();

    int rsfd = landlock_new_ruleset();
    landlock_allow_read_file(rsfd, "/etc/localtime");
    landlock_allow_read_file(rsfd, fn);
    landlock_apply(rsfd);

    seccomp_apply_filter();

    lua_State* L = luaL_newstate();
    CHECK_NOT(L, NULL, "unable to create Lua state");

    luaL_openlibs(L);
    remove_stdlib_function(L, "os", "execute");

    int r = luaL_loadfile(L, fn);
    CHECK_LUA(L, r, "luaL_loadfile(%s)", fn);

    r = lua_pcall(L, 0, LUA_MULTRET, 0);
    CHECK_LUA(L, r, "lua_pcall");

    lua_close(L);

    return 0;
}
