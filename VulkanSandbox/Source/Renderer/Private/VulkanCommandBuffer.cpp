#include "VulkanCommandBuffer.h"

#include "VulkanDevice.h"
#include "VulkanMacros.h"
#include "VulkanQueue.h"

VulkanCommandBufferPool::VulkanCommandBufferPool(VulkanDevice* device, VulkanQueue* queue, const VkCommandPoolCreateFlags& flags)
    : device_(device)
{
    CHECK(device != nullptr);
    CHECK(queue != nullptr);
    VkCommandPoolCreateInfo cmd_pool_create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cmd_pool_create_info.queueFamilyIndex = queue->GetFamilyIndex();
    cmd_pool_create_info.flags = flags; // Optional.
                                        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Command buffers are rerecorded with new commands very often
                                        // (may change memory allocation behavior)
                                        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually.
                                        // Without this flag they all have to be reset together.
    VERIFY_VK_RESULT(vkCreateCommandPool(device->GetLogicalDeviceHandle(), &cmd_pool_create_info, nullptr, &handle_));
}

VulkanCommandBufferPool::~VulkanCommandBufferPool()
{
    if(handle_ != VK_NULL_HANDLE)
    {
        CHECK(device_ != nullptr);
        vkDestroyCommandPool(device_->GetLogicalDeviceHandle(), handle_, nullptr);
        // ^^^ Also destroys any command buffers we retrieved from the pool
    }
}

void VulkanCommandBufferPool::DestroyCommandBuffer(const VkCommandBuffer* command_buffer)
{
    CHECK(command_buffer != nullptr);
    CHECK(device_ != nullptr);
    vkFreeCommandBuffers(device_->GetLogicalDeviceHandle(), handle_, 1 /*num buffers*/, command_buffer);
}

VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice* device, VulkanCommandBufferPool* command_buffer_pool, VkCommandBufferLevel level)
    : device_(device),
    command_buffer_pool_(command_buffer_pool)
{
    CHECK(device != nullptr);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    alloc_info.commandPool = command_buffer_pool_->GetHandle();
    alloc_info.level = level;   // VK_COMMAND_BUFFER_LEVEL_PRIMARY -> Can submit to queue for execution, can't call from other command buffers
                                // VK_COMMAND_BUFFER_LEVEL_SECONDARY -> Can't submit directly, can be called from primary command buffers.
    alloc_info.commandBufferCount = 1;

    VERIFY_VK_RESULT(vkAllocateCommandBuffers(device->GetLogicalDeviceHandle(), &alloc_info, &handle_));
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    CHECK(handle_ != VK_NULL_HANDLE);
    CHECK(command_buffer_pool_ != nullptr);
    command_buffer_pool_->DestroyCommandBuffer(&handle_);
}

VkResult VulkanCommandBuffer::Begin(const VkCommandBufferUsageFlags flags)
{
    CHECK(handle_ != VK_NULL_HANDLE);

    if(is_recording_)
    {
        CHECK_MSG(false, "VulkanCommandBuffer::Begin - End should be called before Begin is called again!");
        return VK_NOT_READY;
    }

    VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = flags;

    VkResult result = vkBeginCommandBuffer(handle_, &begin_info);
    if(result != VK_SUCCESS)
    {
        VERIFY_VK_RESULT(result);
        return result;
    }

    is_recording_ = true;
    return VK_SUCCESS;
}

VkResult VulkanCommandBuffer::End()
{
    if(is_recording_ == false)
    {
        // Can't end recording if it was never started
        CHECK_MSG(false, "VulkanCommandBuffer::End - End called before Begin!");
        return VK_NOT_READY;
    }

    CHECK(handle_ != VK_NULL_HANDLE);
    VkResult result = vkEndCommandBuffer(handle_);
    VERIFY_VK_RESULT(result);
    
    is_recording_ = false;
    return result;
}

VkResult VulkanCommandBuffer::Reset()
{
    CHECK(handle_ != VK_NULL_HANDLE);
    return vkResetCommandBuffer(handle_, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    // ^ VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT: most or all memory resources currently owned by the command buffer
    // >should< be returned to the parent command pool
}
