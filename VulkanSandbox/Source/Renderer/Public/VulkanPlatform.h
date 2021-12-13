#pragma once

#ifdef _WIN32
    #include "VulkanPlatformWindows.h"
#else
    static_assert(false);   // Backend not implemented
#endif
