#pragma once

#ifndef NDEBUG
    #define VERIFY_VK_RESULT(VkCall) { const VkResult r = VkCall; CHECK_MSG(r != VK_SUCCESS, "Vulkan Call Failed!"); }
#else
    #define VERIFY_VK_RESULT(VkCall) { VkCall; }
#endif
