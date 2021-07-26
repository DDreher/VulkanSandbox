#include "VulkanApplication.h"

void VulkanApplication::Run()
{
    InitWindow();
    Renderer_.Init(window_);
    MainLoop();
    Cleanup();
}

void VulkanApplication::InitWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);   // Prevent creation of OpenGL context
    window_ = glfwCreateWindow(SCREEN_WIDTH_, SCREEN_HEIGHT_, WINDOW_TITLE_.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);    // Save pointer to app, so we can access it on frame buffer resize
    glfwSetFramebufferSizeCallback(window_, WindowResizeCallback);
}

void VulkanApplication::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
    // Have to use a static function here, because GLFW doesn't pass the pointer to our application.
    // However, glfw allows us to store our pointer with glfwSetWindowUserPointer. :) Yay.
    auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
    app->Renderer_.OnFrameBufferResize();
}

void VulkanApplication::MainLoop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        Renderer_.DrawFrame();
    }
}

void VulkanApplication::Cleanup()
{
    Renderer_.Cleanup();

    // Clean up glfw
    glfwDestroyWindow(window_);
    glfwTerminate();
}
