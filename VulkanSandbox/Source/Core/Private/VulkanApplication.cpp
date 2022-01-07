#include "VulkanApplication.h"

#include "SDL.h"

void VulkanApplication::Run()
{
    Init();
    MainLoop();
    Cleanup();
}

void VulkanApplication::Init()
{
    LOG("VulkanApplication::Init - Initializing application...");

    SDL_Init(SDL_INIT_VIDEO);

    InitWindow();
    int32 width;
    int32 height;
    SDL_GetWindowSize(window_, &width, &height);

    vulkan_context_.Init(window_);
    renderer_.Init(&vulkan_context_, static_cast<uint32>(width), static_cast<uint32>(height));
}

void VulkanApplication::MainLoop()
{
    LOG("VulkanApplication::MainLoop - Entering Main Loop...");

    is_running_ = true;
    while (is_running_)
    {
        SDL_Event sdl_event;
        while (SDL_PollEvent(&sdl_event))
        {
            if(sdl_event.type == SDL_WINDOWEVENT)
            {
                if(sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    int width;
                    int height;
                    SDL_GetWindowSize(window_, &width, &height);
                    renderer_.OnFrameBufferResize(width, height);
                }
            }

            // Close application on window close or esc pressed
            if (sdl_event.type == SDL_WINDOWEVENT && sdl_event.window.event == SDL_WINDOWEVENT_CLOSE ||
                sdl_event.type == SDL_KEYDOWN && sdl_event.key.keysym.sym == SDLK_ESCAPE ||
                sdl_event.type == SDL_QUIT)
            {
                is_running_ = false;
                break;
            }
        } // end SDL event polling

        tick_timer_.Update();

        for (uint32_t i = 0; i < tick_timer_.GetAccumulatedTicks(); i++)
        {
            Update(TickTimer::MICROSEC_PER_TICK);
        }

        Render();
    }
}

void VulkanApplication::Cleanup()
{
    LOG("VulkanApplication::Cleanup - Tearing down application...");
    renderer_.Cleanup();
    vulkan_context_.Shutdown();
    DestroyWindow();

    SDL_Quit();
}

void VulkanApplication::InitWindow()
{
    window_ = SDL_CreateWindow(WINDOW_TITLE_.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        INITIAL_SCREEN_WIDTH_, INITIAL_SCREEN_HEIGHT_,
        SDL_WindowFlags::SDL_WINDOW_VULKAN);
    CHECK(window_ != nullptr);
}

void VulkanApplication::DestroyWindow()
{
    if(window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
}

void VulkanApplication::Update(float delta)
{

}

void VulkanApplication::Render()
{
    renderer_.DrawFrame();
}
