#pragma once

#include <core/Application.h>
#include <core/Log.h>

#ifdef VKE_PLATFORM_WINDOWS

extern VKE::Application* VKE::CreateApplication();

int main(int argc, char** argv)
{
    VKE::Log::Init();
    VKE_CORE_WARN("Initialized Log!");

    auto app = VKE::CreateApplication();
    app->Run();
    delete app;

    return 0;
}

#endif // VKE_PLATFORM_WINDOWS
