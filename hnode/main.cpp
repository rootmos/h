#include <node.h>
#include <uv.h>

#define LIBR_IMPLEMENTATION
#include "r.h"

#include "capabilities.c"

int run(
    node::MultiIsolatePlatform* platform,
    const std::vector<std::string>& args,
    const std::vector<std::string>& exec_args)
{
    const char main_script_source_utf8[] = {
#include "main.jsc"
    };

#if (NODE_MAJOR_VERSION == 19)
    std::vector<std::string> errors;
    auto setup = node::CommonEnvironmentSetup::Create(
        platform, &errors, args, exec_args);
    if(!setup) {
        for (const std::string& err: errors) {
            error("%s", err.c_str());
        }
        return 1;
    }

    v8::Isolate* isolate = setup->isolate();
    node::Environment* env = setup->env();

    int exit_code = 0;
    {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        v8::Context::Scope context_scope(setup->context());

        auto loadenv_ret = node::LoadEnvironment(env, main_script_source_utf8);
        if (loadenv_ret.IsEmpty()) {
            return 1;
        }

        exit_code = node::SpinEventLoop(env).FromMaybe(1);

        node::Stop(env);
    }

    return exit_code;
#elif (NODE_MAJOR_VERSION == 12)
    uv_loop_t loop;
    int r = uv_loop_init(&loop);
    CHECK_UV(r, "uv_loop_init");

    std::shared_ptr<node::ArrayBufferAllocator> allocator =
        node::ArrayBufferAllocator::Create();

    v8::Isolate* isolate = node::NewIsolate(allocator.get(), &loop, platform);
    if(isolate == nullptr) {
        failwith("unable to create v8::Isolate");
    }

    int exit_code = 0;
    {
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        std::unique_ptr<node::IsolateData, decltype(&node::FreeIsolateData)> isolate_data(
            node::CreateIsolateData(isolate, &loop, platform, allocator.get()),
            node::FreeIsolateData);

        v8::HandleScope handle_scope(isolate);
        v8::Local<v8::Context> context = node::NewContext(isolate);
        if(context.IsEmpty()) {
            failwith("unable to initialize v8::Context");
        }

        v8::Context::Scope context_scope(context);

        std::unique_ptr<node::Environment, decltype(&node::FreeEnvironment)> env(
              node::CreateEnvironment(isolate_data.get(), context, args, exec_args),
              node::FreeEnvironment);

        auto loadenv_ret = node::LoadEnvironment(env.get(), main_script_source_utf8);
        if (loadenv_ret.IsEmpty()) {
            return 1;
        }

        {
            v8::SealHandleScope seal(isolate);
            bool more;
            do {
                r = uv_run(&loop, UV_RUN_DEFAULT);
                CHECK_UV(r, "uv_run");

                platform->DrainTasks(isolate);

                more = uv_loop_alive(&loop);
                if(more) continue;

                node::EmitBeforeExit(env.get());

                more = uv_loop_alive(&loop);
            } while(more);
        }

        exit_code = node::EmitExit(env.get());

        node::Stop(env.get());
    }

    bool platform_finished = false;
    platform->AddIsolateFinishedCallback(isolate, [](void* data) {
        *static_cast<bool*>(data) = true;
    }, &platform_finished);
    platform->UnregisterIsolate(isolate);
    isolate->Dispose();

    while(!platform_finished) {
        r = uv_run(&loop, UV_RUN_ONCE);
        CHECK_UV(r, "uv_run");
    }

    r = uv_loop_close(&loop);
    CHECK_UV(r, "uv_loop_close");

    return exit_code;
#else
#error "unsupported node version"
#endif
}

int main(int argc, char* argv[])
{
    drop_capabilities();
    no_new_privs();

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
        return result->exit_code();
    }
#elif (NODE_MAJOR_VERSION == 12)
    std::vector<std::string> exec_args;
    std::vector<std::string> errors;
    int exit_code = node::InitializeNodeWithArgs(&args, &exec_args, &errors);
    for (const std::string& err: errors) {
        error("%s", err.c_str());
    }
    if(exit_code != 0) {
        return exit_code;
    }
#else
#error "unsupported node version"
#endif

    auto platform = node::MultiIsolatePlatform::Create(1);
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

#if (NODE_MAJOR_VERSION == 19)
    int r = run(platform.get(), result->args(), result->exec_args());
#elif (NODE_MAJOR_VERSION == 12)
    int r = run(platform.get(), args, exec_args);
#else
#error "unsupported node version"
#endif

    v8::V8::Dispose();

#if (NODE_MAJOR_VERSION == 19)
    v8::V8::DisposePlatform();
    node::TearDownOncePerProcess();
#elif (NODE_MAJOR_VERSION == 12)
    v8::V8::ShutdownPlatform();
#else
#error "unsupported node version"
#endif

    return r;
}
