#include <node.h>
#include <uv.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

#include "capabilities.c"
#include "seccomp.c"

int main(int argc, char* argv[])
{
    drop_capabilities();
    no_new_privs();

    seccomp_apply_filter();

    argv = uv_setup_args(argc, argv);
    std::vector<std::string> args(argv, argv + argc);

#if (NODE_MAJOR_VERSION == 19)
    auto result = node::InitializeOncePerProcess(
        args, {
            node::ProcessInitializationFlags::kNoInitializeV8,
            node::ProcessInitializationFlags::kNoInitializeNodeV8Platform
        });
    for (const std::string& err: result->errors()) {
        error("%s", err.c_str());
    }
    if (result->early_return() != 0) {
        exit(result->exit_code());
    }
#elif (NODE_MAJOR_VERSION == 12)
    std::vector<std::string> exec_args;
    std::vector<std::string> errors;
    int ec = node::InitializeNodeWithArgs(&args, &exec_args, &errors);
    for (const std::string& err: errors) {
        error("%s", err.c_str());
    }
    if(ec != 0) {
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
    std::shared_ptr<node::ArrayBufferAllocator> allocator =
        node::ArrayBufferAllocator::Create();
    if(allocator == nullptr) {
        failwith("unable to create allocator");
    }

    debug("creating v8::Isolate");
    v8::Isolate* isolate = node::NewIsolate(allocator.get(), &loop, platform.get());
    if(isolate == nullptr) {
        failwith("unable to create v8::Isolate");
    }

    int exit_code = 0;
    {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);

        debug("creating node::IsolateData");
        std::unique_ptr<node::IsolateData, decltype(&node::FreeIsolateData)> isolate_data(
            node::CreateIsolateData(isolate, &loop, platform.get()),
            node::FreeIsolateData);

        v8::HandleScope handle_scope(isolate);

        v8::Local<v8::Context> context = node::NewContext(isolate);
        if(context.IsEmpty()) {
            failwith("unable to initialize v8::Context");
        }

        v8::Context::Scope context_scope(context);

        debug("creating node::Environment");
        std::unique_ptr<node::Environment, decltype(&node::FreeEnvironment)> env(
#if (NODE_MAJOR_VERSION == 19)
              node::CreateEnvironment(isolate_data.get(), context, result->args(), result->exec_args()),
#elif (NODE_MAJOR_VERSION == 12)
              node::CreateEnvironment(isolate_data.get(), context, args, exec_args),
#else
#error "unsupported node version"
#endif
              node::FreeEnvironment);
        if(!env) {
            failwith("unable to create environment");
        }

        const char main_script_source_utf8[] = {
#include "main.jsc"
        };

        debug("loading environment");
        auto loadenv_ret = node::LoadEnvironment(env.get(), main_script_source_utf8);
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
#if (NODE_MAJOR_VERSION == 19)
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
#if (NODE_MAJOR_VERSION == 19)
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
#if (NODE_MAJOR_VERSION == 19)
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
