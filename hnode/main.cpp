#include <libgen.h>

#include <node.h>
#include <uv.h>

#define RLIMIT_DEFAULT_CPU (1<<2)
#define RLIMIT_DEFAULT_DATA (1<<30)
#define RLIMIT_DEFAULT_NOFILE (1<<5)
#define RLIMIT_DEFAULT_NPROC (1<<11)
#define RLIMIT_DEFAULT_RSS (1<<29)
#define RLIMIT_DEFAULT_AS ((long unsigned int)1<<31)

#define LIBR_IMPLEMENTATION
#include "r.h"

#include "capabilities.c"
#include "seccomp.c"

struct options {
    const char* input;

    int allow_script_dir_read;
    int allow_script_dir_exec;

    struct rlimit_spec rlimits[RLIMIT_NLIMITS];
};

static void print_usage(int fd, const char* prog)
{
    dprintf(fd, "usage: %s [OPTION]... INPUT\n", prog);
    dprintf(fd, "\n");
    dprintf(fd, "options:\n");
    dprintf(fd, "  -s       allow reading files beneath the input script's directory\n");
    dprintf(fd, "  -x       allow executing files beneath the input script's directory (implies read access)\n");
    dprintf(fd, "  -h       print this message\n");
    dprintf(fd, "  -v       print version information\n");
    dprintf(fd, "\n");
    dprintf(fd, "rlimit options:\n");
    dprintf(fd, "  -rRLIMIT=VALUE set RLIMIT to VALUE\n");
    dprintf(fd, "  -R             use inherited rlimits instead of default\n");
}

#include "version.c"

static void parse_options(struct options* o, int argc, char* argv[])
{
    memset(o, 0, sizeof(*o));

    rlimit_default(o->rlimits, LENGTH(o->rlimits));

    int res;
    while((res = getopt(argc, argv, "hvsxr:R")) != -1) {
        switch(res) {
        case 's':
            o->allow_script_dir_read = 1;
            break;
        case 'x':
            o->allow_script_dir_exec = 1;
            break;
        case 'r': {
            int r = rlimit_parse(o->rlimits, LENGTH(o->rlimits), optarg);
            if(r != 0) {
                dprintf(1, "unable to parse rlimit: %s\n", optarg);
                exit(1);
            }
            break;
        }
        case 'R':
            rlimit_inherit(o->rlimits, LENGTH(o->rlimits));
            break;
        case 'v':
            print_version(argv[0]);
            exit(0);
        case 'h':
        default:
            print_usage(res == 'h' ? 1 : 2, argv[0]);
            exit(res == 'h' ? 0 : 1);
        }
    }

    if(optind < argc) {
        o->input = argv[optind];
        debug("input: %s", o->input);

        struct stat st;
        int r = stat(o->input, &st);
        if(r == -1 && errno == ENOENT) {
            dprintf(2, "error; unable to access input file: %s\n", o->input);
            exit(1);
        }
        CHECK(r, "stat(%s)", o->input);
    } else {
        dprintf(2, "error: no input file specified\n");
        print_usage(2, argv[0]);
        exit(1);
    }
}

#if (NODE_MAJOR_VERSION >= 24)
#include <cppgc/platform.h>

static int RunNodeInstance(
        node::MultiIsolatePlatform* platform,
        const std::vector<std::string>& args,
        const std::vector<std::string>& exec_args,
        struct options* o)
{
    int exit_code = 0;

    std::vector<std::string> errors;
    auto setup = node::CommonEnvironmentSetup::Create(platform, &errors, args, exec_args);

    if (!setup) {
        for(const std::string& err: errors) {
            error("node environment setup error: %s", err.c_str());
        }
        return 1;
    }

    v8::Isolate* isolate = setup->isolate();
    node::Environment* env = setup->env();

    {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);

        setup->context()->Global()->Set(setup->context(),
            v8::String::NewFromUtf8(isolate, "input_script_filename").ToLocalChecked(),
            v8::String::NewFromUtf8(isolate, o->input).ToLocalChecked()
        ).Check();

        v8::Context::Scope context_scope(setup->context());

        const char main_script_source_utf8[] = {
#include "main.jsc"
        };

        debug("loading environment");
        auto loadenv_ret = node::LoadEnvironment(env, main_script_source_utf8);
        if(loadenv_ret.IsEmpty()) {
            failwith("unable to load envionment");
        }

        exit_code = node::SpinEventLoop(env).FromMaybe(1);
    }

    node::Stop(env);

    return exit_code;
}

static int run(int argc, char* argv[], struct options* o)
{
    char* c_args[] = {
        argv[0],
        NULL,
    };
    const int n_args = 1;

    argv = uv_setup_args(n_args, c_args);
    std::vector<std::string> args(c_args, c_args + n_args);

    auto result = node::InitializeOncePerProcess(
        args, {
            node::ProcessInitializationFlags::kNoInitializeV8,
            node::ProcessInitializationFlags::kNoInitializeNodeV8Platform,
            node::ProcessInitializationFlags::kDisableNodeOptionsEnv,
            node::ProcessInitializationFlags::kNoInitializeCppgc,
        });

    for(const std::string& err: result->errors()) {
        error("node initialization error: %s", err.c_str());
    }

    if(result->early_return() != 0) {
        return result->exit_code();
    }

    debug("initializing node platform");
    auto platform = node::MultiIsolatePlatform::Create(1);
    v8::V8::InitializePlatform(platform.get());
    cppgc::InitializeProcess(platform->GetPageAllocator());
    v8::V8::Initialize();

    int ret = RunNodeInstance(platform.get(), result->args(), result->exec_args(), o);

    v8::V8::Dispose();
    v8::V8::DisposePlatform();

    node::TearDownOncePerProcess();
    return ret;
}

#else // NODE_MAJOR_VERSION >= 24

static int run(int argc, char* argv[], struct options* o)
{
    char* c_args[] = {
        argv[0],
        NULL,
    };
    const int n_args = 1;

    argv = uv_setup_args(n_args, c_args);
    std::vector<std::string> args(c_args, c_args + n_args);
#if (NODE_MAJOR_VERSION >= 18)
    auto result = node::InitializeOncePerProcess(
        args, {
            node::ProcessInitializationFlags::kNoInitializeV8,
            node::ProcessInitializationFlags::kNoInitializeNodeV8Platform
        });
    for(const std::string& err: result->errors()) {
        error("node initialization error: %s", err.c_str());
    }
    if (result->early_return() != 0) {
        int ec = result->exit_code();
        debug("node exit: %d", ec);
        exit(result->exit_code());
    }
#elif (NODE_MAJOR_VERSION >= 12)
    std::vector<std::string> exec_args;
    std::vector<std::string> errors;
    int ec = node::InitializeNodeWithArgs(&args, &exec_args, &errors);
    for (const std::string& err: errors) {
        error("node initialization error: %s", err.c_str());
    }
    if(ec != 0) {
        debug("node exit: %d", ec);
        exit(ec);
    }
#else
#error "unsupported node version"
#endif

    debug("initializing node platform");
    auto platform = node::MultiIsolatePlatform::Create(1);
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    debug("initializing uv loop");
    uv_loop_t loop;
    int r = uv_loop_init(&loop);
    CHECK_UV(r, "uv_loop_init");

    debug("creating allocator");
    auto allocator = node::ArrayBufferAllocator::Create();
    if(allocator == nullptr) {
        failwith("unable to create allocator");
    }

    debug("creating v8::Isolate");
    auto isolate = node::NewIsolate(allocator.get(), &loop, platform.get());
    if(isolate == nullptr) {
        failwith("unable to create v8::Isolate");
    }

    int exit_code = 0;
    {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);

        debug("creating node::IsolateData");
        std::unique_ptr<node::IsolateData, decltype(&node::FreeIsolateData)>
            isolate_data(
                node::CreateIsolateData(isolate, &loop, platform.get()),
                node::FreeIsolateData);

        v8::HandleScope handle_scope(isolate);

        auto context = node::NewContext(isolate);
        if(context.IsEmpty()) {
            failwith("unable to initialize v8::Context");
        }

        auto global = context->Global();
        global->Set(context,
#if (NODE_MAJOR_VERSION >= 18)
            v8::String::NewFromUtf8(isolate, "input_script_filename").ToLocalChecked(),
            v8::String::NewFromUtf8(isolate, o->input).ToLocalChecked()
#elif (NODE_MAJOR_VERSION >= 12)
            v8::String::NewFromUtf8(isolate, "input_script_filename"),
            v8::String::NewFromUtf8(isolate, o->input)
#else
#error "unsupported node version"
#endif
        ).Check();

        v8::Context::Scope context_scope(context);

        debug("creating node::Environment");
        std::unique_ptr<node::Environment, decltype(&node::FreeEnvironment)>
            env(
                node::CreateEnvironment(
                    isolate_data.get(), context,
#if (NODE_MAJOR_VERSION >= 18)
                    result->args(), result->exec_args()
#elif (NODE_MAJOR_VERSION >= 12)
                    args, exec_args
#else
#error "unsupported node version"
#endif
                ),
                node::FreeEnvironment);
        if(!env) {
            failwith("unable to create environment");
        }

        const char main_script_source_utf8[] = {
#include "main.jsc"
        };

        debug("loading environment");
        auto loadenv_ret = node::LoadEnvironment(
            env.get(), main_script_source_utf8);
        if (loadenv_ret.IsEmpty()) {
            failwith("unable to load envionment");
        }

        {
            v8::SealHandleScope seal(isolate);
            bool more;
            do {
                debug("running loop");
                r = uv_run(&loop, UV_RUN_DEFAULT);
                CHECK_UV(r, "uv_run");

                debug("draining tasks");
                platform->DrainTasks(isolate);

                more = uv_loop_alive(&loop);
                if(more) continue;

                debug("emit before exit");
#if (NODE_MAJOR_VERSION >= 18)
                node::EmitProcessBeforeExit(env.get());
#elif (NODE_MAJOR_VERSION == 12)
                node::EmitBeforeExit(env.get());
#else
#error "unsupported node version"
#endif

                more = uv_loop_alive(&loop);
            } while(more);
        }

        debug("emit exit");
#if (NODE_MAJOR_VERSION >= 18)
        exit_code = node::EmitProcessExit(env.get()).FromJust();
#elif (NODE_MAJOR_VERSION == 12)
        exit_code = node::EmitExit(env.get());
#else
#error "unsupported node version"
#endif

        debug("stopping env");
        node::Stop(env.get());
    }

    debug("disposing isolate");
    bool platform_finished = false;
    platform->AddIsolateFinishedCallback(isolate, [](void* data) {
        debug("callbacking");
        *static_cast<bool*>(data) = true;
    }, &platform_finished);
    platform->UnregisterIsolate(isolate);
    isolate->Dispose();

    while(!platform_finished) {
        debug("waiting for platform to finish");
        r = uv_run(&loop, UV_RUN_ONCE);
        CHECK_UV(r, "uv_run");
    }

    debug("closing loop");
    r = uv_loop_close(&loop);
    CHECK_UV(r, "uv_loop_close");

    debug("dispose v8");
    v8::V8::Dispose();

    debug("dispose platform");
#if (NODE_MAJOR_VERSION >= 18)
    v8::V8::DisposePlatform();
    node::TearDownOncePerProcess();
#elif (NODE_MAJOR_VERSION == 12)
    v8::V8::ShutdownPlatform();
#else
#error "unsupported node version"
#endif

    debug("bye: %d", exit_code);
    return exit_code;
}

#endif // NODE_MAJOR_VERSION >= 24

int main(int argc, char* argv[])
{
    drop_capabilities();
    no_new_privs();

    struct options o;
    parse_options(&o, argc, argv);

    rlimit_apply(o.rlimits, LENGTH(o.rlimits));

    int rsfd = landlock_new_ruleset();

    if(o.allow_script_dir_read || o.allow_script_dir_exec) {
        char buf[PATH_MAX];
        char* input = realpath(o.input, buf);
        CHECK_NOT(input, NULL, "realpath(%s)", o.input);

        char* script_dir = dirname(input);

        if(o.allow_script_dir_read || o.allow_script_dir_exec) {
            debug("allowing read access beneath: %s", script_dir);
            landlock_allow_read(rsfd, script_dir);
        }

        if(o.allow_script_dir_exec) {
            debug("allowing execute access beneath: %s", script_dir);
            landlock_allow(rsfd, script_dir, LANDLOCK_ACCESS_FS_EXECUTE);
        }
    } else {
        debug("allowing read access: %s", o.input);
        landlock_allow_read(rsfd, o.input);
    }

#if (NODE_MAJOR_VERSION >= 19)
    landlock_allow_read(rsfd, "/etc/ssl/openssl.cnf");
#endif

#if (NODE_MAJOR_VERSION == 18)
    landlock_allow_read(rsfd, "/usr/share/nodejs");
    landlock_allow_read(rsfd, "/usr/lib/ssl/openssl.cnf");
#endif

    landlock_apply(rsfd);
    int r = close(rsfd); CHECK(r, "close");

    seccomp_apply_filter();

    return run(argc, argv, &o);
}
