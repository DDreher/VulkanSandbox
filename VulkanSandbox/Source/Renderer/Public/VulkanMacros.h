#pragma once

#define VERIFY_VK_RESULT(VkCall) { VERIFY_MSG(VkCall == VK_SUCCESS, "Vulkan Call Failed!"); }
