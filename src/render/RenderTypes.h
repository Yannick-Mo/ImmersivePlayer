#pragma once
#include <glm/glm.hpp>
#include "MeshUtils.h"

struct PhongMaterial {
    glm::vec3 color{1.0f};
    glm::vec3 emissive{0.0f};
    float emissiveIntensity = 0.0f;
    float roughness = 0.5f;
    float metalness = 0.0f;
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct DirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct SpotLight {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float cutOff = 0.5f;       // cos(inner angle)
    float outerCutOff = 0.3f;  // cos(outer angle)
};

struct RenderObject {
    Mesh mesh;
    PhongMaterial material;
    glm::mat4 transform{1.0f};
};
