#include <libgen.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

void seccomp_apply_filter()
{
    struct sock_filter filter[] = {
#include "filter.bpfc"
    };

    struct sock_fprog p = { .len = LENGTH(filter), .filter = filter };
    int r = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &p);
    CHECK(r, "prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER)");
}

void openlibs(struct lua_State* L)
{
    lua_stack_neutral_begin(L);

    static const luaL_Reg loadedlibs[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_LOADLIBNAME, luaopen_package},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_IOLIBNAME, luaopen_io},
        {LUA_OSLIBNAME, luaopen_os},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {NULL, NULL},
        {LUA_DBLIBNAME, luaopen_debug},
        {LUA_COLIBNAME, luaopen_coroutine},
    };

    for(const luaL_Reg* lib = loadedlibs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }

    lua_stack_neutral_end(L);
}

void remove_stdlib_function(struct lua_State* L,
                            const char* lib, const char* f)
{
    lua_stack_neutral_begin(L);

    int t = lua_getglobal(L, lib);
    LUA_EXPECT_TYPE(L, t, LUA_TTABLE, "%s", lib);

    lua_pushnil(L);
    lua_setfield(L, -2, f);
    lua_pop(L, 1);

    lua_stack_neutral_end(L);
}


#define DEFAULT_TMP "/tmp"

struct options {
    const char* input;

    int allow_localtime;
    int allow_script_dir;

    int allow_tmp;
    const char* tmp;
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... INPUT\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -l       allow reading /etc/localtime\n");
    dprintf(fd, "  -s       allow reading files beneath the input script's directory\n");
    dprintf(fd, "  -t       allow read+write access to %s\n", DEFAULT_TMP);
    dprintf(fd, "  -h       print this message\n");
}

static void parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

    o->allow_localtime = 0;
    o->allow_script_dir = 0;

    o->allow_tmp = 0;
    o->tmp = DEFAULT_TMP;

    int res;
    while((res = getopt(argc, argv, "hlst")) != -1) {
        switch(res) {
        case 'l':
            o->allow_localtime = 1;
            break;
        case 's':
            o->allow_script_dir = 1;
            break;
        case 't':
            o->allow_tmp = 1;
            break;
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }

    if(optind < argc) {
        o->input = argv[optind];
    } else {
        dprintf(2, "error: no input file specified\n");
        print_usage(2, argv[0]);
        exit(1);
    }

    debug("input: %s", o->input);
}

int main(int argc, char* argv[])
{
    no_new_privs();

    struct options o;
    parse_options(&o, argc, argv);

    int rsfd = landlock_new_ruleset();

    if(o.allow_localtime) {
        debug("allowing read access: /etc/localtime");
        landlock_allow_read(rsfd, "/etc/localtime");
    }

    if(o.allow_script_dir) {
        char buf0[PATH_MAX];
        strncpy(buf0, o.input, sizeof(buf0)-1);
        buf0[sizeof(buf0)-1] = '\0';
        char* dir = dirname(buf0);

        char buf2[PATH_MAX];
        char* script_dir = realpath(dir, buf2);
        CHECK_NOT(script_dir, NULL, "realpath(%s)", dir);

        debug("allowing read access beneath: %s", script_dir);
        landlock_allow_read(rsfd, script_dir);
    } else {
        debug("allowing read access: %s", o.input);
        landlock_allow_read(rsfd, o.input);
    }

    if(o.allow_tmp) {
        debug("allowing read+write access beneath: %s", o.tmp);
        landlock_allow_read_write(rsfd, o.tmp);
    }

    landlock_apply(rsfd);
    int r = close(rsfd); CHECK(r, "close");

    seccomp_apply_filter();

    lua_State* L = luaL_newstate();
    CHECK_NOT(L, NULL, "unable to create Lua state");

    openlibs(L);
    remove_stdlib_function(L, "os", "execute");
    remove_stdlib_function(L, "package", "loadlib");

    r = luaL_loadfile(L, o.input);
    CHECK_LUA(L, r, "luaL_loadfile(%s)", o.input);

    r = lua_pcall(L, 0, LUA_MULTRET, 0);
    CHECK_LUA(L, r, "lua_pcall");

    lua_close(L);

    return 0;
}