#pragma once

#include <vulkan/vulkan_core.h>

struct Vertex
{
    Vec3 pos_;
    Vec3 color_;
    Vec2 tex_coords_;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        // Describes how to pass data to the vertex shader.
        // Specifies number of bytes between data entries and the input rate,
        // i.e. whether to move to next data entry after each vertex or after each instance
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
                                                                    // float: VK_FORMAT_R32_SFLOAT      
                                                                    // vec2 : VK_FORMAT_R32G32_SFLOAT
                                                                    // vec3 : VK_FORMAT_R32G32B32_SFLOAT
                                                                    // vec4 : VK_FORMAT_R32G32B32A32_SFLOAT
                                                                    // -> SFLOAT means signed float. There's also UINT, SINT. Should match the type of the shader input
                                                                    // If we specify less components than actually required, the BGA channels default to (0.0f, 0.0f, 1.0f).

        attribute_descriptions[0].offset = offsetof(Vertex, pos_);  // Specifies the number of bytes since the start of the per-vertex data to read from
                                                                    // Binding loads one vertex at a time. pos is at an offset of 0 bytes from the beginning of the struct.

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

// Define hash function for our Vertex struct.
// We use a hash map to get rid of duplicated vertices in loaded models.
namespace std
{
    template<>
    struct hash<Vertex>
    {
        size_t operator()(const Vertex& vertex) const
        {
            // Create hash according to http://en.cppreference.com/w/cpp/utility/hash
            return ((hash<Vec3>()(vertex.pos_) ^
                (hash<Vec3>()(vertex.color_) << 1)) >> 1) ^
                (hash<Vec2>()(vertex.tex_coords_) << 1);
        }
    };
}
