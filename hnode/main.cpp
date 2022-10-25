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

        auto loadenv_ret = node::LoadEnvironment(
            env,
            "const publicRequire ="
            "  require('node:module').createRequire(process.cwd() + '/');"
            "globalThis.require = publicRequire;"
            "require('node:vm').runInThisContext(process.argv[1]);");
        if (loadenv_ret.IsEmpty()) {
            return 1;
        }

        exit_code = node::SpinEventLoop(env).FromMaybe(1);

        node::Stop(env);
    }

    return exit_code;
}

int main(int argc, char* argv[])
{
    drop_capabilities();
    no_new_privs();

    argv = uv_setup_args(argc, argv);
    std::vector<std::string> args(argv, argv + argc);
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

    auto platform = node::MultiIsolatePlatform::Create(1);
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    int r = run(platform.get(), result->args(), result->exec_args());

    v8::V8::Dispose();
    v8::V8::DisposePlatform();

    node::TearDownOncePerProcess();

    return r;
}
