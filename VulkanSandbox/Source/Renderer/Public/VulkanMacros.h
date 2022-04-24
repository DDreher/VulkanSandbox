#pragma once

#define VERIFY_VK_RESULT_MSG(VkCall, Msg) { VERIFY_MSG(VkCall == VK_SUCCESS, Msg); }
#define VERIFY_VK_RESULT(VkCall) { VERIFY_VK_RESULT_MSG(VkCall, "Vulkan Call Failed!"); }