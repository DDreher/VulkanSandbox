#include "VulkanApplication.h"

void VulkanApplication::Run()
{
    InitWindow();

    RHI_.Init();
    renderer_.Init(&RHI_, window_);

    MainLoop();
    Cleanup();
}

void VulkanApplication::InitWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Prevent creation of OpenGL context
    window_ = glfwCreateWindow(INITIAL_SCREEN_WIDTH_, INITIAL_SCREEN_HEIGHT_, WINDOW_TITLE_.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);    // Save pointer to app, so we can access it on frame buffer resize
    glfwSetFramebufferSizeCallback(window_, WindowResizeCallback);
}

void VulkanApplication::Cleanup()
{
    renderer_.Cleanup();
    RHI_.Shutdown();

    // Clean up glfw
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void VulkanApplication::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
    // Have to use a static function here, because GLFW doesn't pass the pointer to our application.
    // However, glfw allows us to store our pointer with glfwSetWindowUserPointer. :) Yay.
    auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    app->renderer_.OnFrameBufferResize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}

void VulkanApplication::Update(float delta)
{

}

void VulkanApplication::Render()
{
    renderer_.DrawFrame();
}

void VulkanApplication::MainLoop()
{
    LOG("VulkanApplication::MainLoop - Entering Main Loop");

    while (glfwWindowShouldClose(window_) == false)
    {
        glfwPollEvents();

        tick_timer_.Update();

        for (uint32_t i = 0; i < tick_timer_.GetAccumulatedTicks(); i++)
        {
            Update(TickTimer::MICROSEC_PER_TICK);
        }
        
        Render();
    }
}
