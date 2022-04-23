#pragma once

#define GLM_FORCE_RADIANS   // Ensure that matrix functions use radians as units
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM perspective projection matrix will use depth range of -1.0 to 1.0 by default. We need range of 0.0 to 1.0 for Vulkan.
#define GLM_ENABLE_EXPERIMENTAL // Needed so we can use the hash functions of GLM types
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // matrix functions like glm::lookAt etc.
#include <glm/gtx/hash.hpp>

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
