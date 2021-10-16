#include "VulkanApplication.h"

int main()
{
    Log::Init();

    VulkanApplication app;
    app.Run();

    return EXIT_SUCCESS;
}
