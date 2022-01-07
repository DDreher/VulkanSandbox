#include "VulkanApplication.h"

int main()
{
    Log::Init();

    VulkanApplication* app = new VulkanApplication();
    app->Run();

    delete app;
    app = nullptr;

    return EXIT_SUCCESS;
}
