#pragma once
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLShaderProgram>
#include <glm/glm.hpp>
#include <vector>
#include "RenderTypes.h"

class VideoManager;
class ScreenModel;
struct Mesh;

class Scene {
public:
    virtual ~Scene() = default;

    virtual void initialize(QOpenGLFunctions_4_5_Core* gl) = 0;
    virtual void cleanup(QOpenGLFunctions_4_5_Core* gl) = 0;
    virtual void update(float dt, VideoManager* decoder) = 0;

    // Main render (global uniforms already set by OpenGLWidget)
    virtual void render(QOpenGLFunctions_4_5_Core* gl,
                        QOpenGLShaderProgram* phongShader,
                        QOpenGLShaderProgram* videoShader,
                        QOpenGLShaderProgram* particleShader,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        const glm::vec3& cameraPos) = 0;

    // Opaque objects for shadow pass (model transform + mesh VAO)
    virtual const std::vector<RenderObject>& opaqueObjects() const = 0;

    virtual std::vector<ScreenModel*> screens() = 0;

    virtual std::vector<Mesh> shadowOnlyMeshes() const { return {}; }

    virtual glm::vec3 defaultTarget() const { return {0.0f, 2.0f, 0.0f}; }
    virtual float defaultYaw() const { return -30.0f; }
    virtual float defaultPitch() const { return 20.0f; }
    virtual float defaultRadius() const { return 14.0f; }
    virtual float minRadius() const { return 3.0f; }
    virtual float maxRadius() const { return 200.0f; }
    virtual float maxPitch() const { return 85.0f; }
    virtual float minPitch() const { return -80.0f; }

protected:
    QOpenGLFunctions_4_5_Core* m_gl = nullptr;
};
