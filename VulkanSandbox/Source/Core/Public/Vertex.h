#pragma once

#define GLM_FORCE_RADIANS   // Ensure that matrix functions use radians as units
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM perspective projection matrix will use depth range of -1.0 to 1.0 by default. We need range of 0.0 to 1.0 for Vulkan.
#define GLM_ENABLE_EXPERIMENTAL // Needed so we can use the hash functions of GLM types
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan_core.h>

struct Vertex
{
    glm::vec3 pos_;
    glm::vec3 color_;
    glm::vec2 tex_coords_;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        // Describes how to pass data to the vertex shader.
        // Specifies number of bytes between data entries and the input rate, i.e. whether to move to next data entry
        // after each vertex or after each instance
        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;    // Specifies index of the binding in an array of bindings. 
                                            // Our data is in one array, so we have only one binding.
        binding_description.stride = sizeof(Vertex);    // Number of bytes from one entry to the next
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;    // VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
                                                                        // VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance
                                                                        // In this case we stick to per-vertex data.
        return binding_description;
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {
        // Describes how to extract a vertex attribute from a chunk of vertex data coming from a binding description
        // -> We have three attributes (pos, color, UVs), so we need two attribute descriptions.
        // -> We need add UVs as vertex input attribute description so we can pass them on to the fragment shader as interpolated value!
        std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};

        // Pos attribute
        attribute_descriptions[0].binding = 0;  // Which binding does the per-vertex data come from?
        attribute_descriptions[0].location = 0; // References the location of the attribute in the vertex shader
        attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Data type of the attribute. Implicitely defines the byte size of the attribute data
                                                                    // -> For some reason we have to use the color format enums here... Weird af
                                                                    // float: VK_FORMAT_R32_SFLOAT      
                                                                    // vec2 : VK_FORMAT_R32G32_SFLOAT
                                                                    // vec3 : VK_FORMAT_R32G32B32_SFLOAT
                                                                    // vec4 : VK_FORMAT_R32G32B32A32_SFLOAT
                                                                    // -> In this case it's the position, which has three 32bit float components (i.e. r,g,b channel)
                                                                    // If we specify less components than are actually required, then the BGA channels default to (0.0f, 0.0f, 1.0f).
                                                                    // -> SFLOAT means signed float. There's also UINT, SINT. Should match the type of the shader input

        attribute_descriptions[0].offset = offsetof(Vertex, pos_);  // Specifies the number of bytes since the start of the per-vertex data to read from
                                                                    // Binding is loading one vertex at a time and pos is at an offset of 0 bytes from the beginning of the struct.
                                                                    // -> We can easily calculate this with the offsetof macro though!

        // Color attribute
        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[1].offset = offsetof(Vertex, color_);

        // Tex coords attribute
        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[2].offset = offsetof(Vertex, tex_coords_);

        return attribute_descriptions;
    }

    // Need this so we can use hash maps
    bool operator==(const Vertex& other) const
    {
        return pos_ == other.pos_ && color_ == other.color_ && tex_coords_ == other.tex_coords_;
    }
};

// hash function for our Vertex struct
namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const
        {
            // Create hash according to http://en.cppreference.com/w/cpp/utility/hash
            return ((hash<glm::vec3>()(vertex.pos_) ^
                (hash<glm::vec3>()(vertex.color_) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.tex_coords_) << 1);
        }
    };
}
