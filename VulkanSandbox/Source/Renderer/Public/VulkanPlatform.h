#pragma once

#ifdef _WIN32
    #include "VulkanWindowsPlatform.h"
#else
    static_assert(false);   // Backend not implemented
#endif
