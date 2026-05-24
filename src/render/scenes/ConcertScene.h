#pragma once
#include "render/Scene.h"
#include "render/RenderTypes.h"
#include "render/MeshUtils.h"
#include "render/ScreenModel.h"
#include <vector>
#include <memory>

class ConcertScene : public Scene {
public:
    ConcertScene();
    ~ConcertScene() override;

    void initialize(QOpenGLFunctions_4_5_Core* gl) override;
    void cleanup(QOpenGLFunctions_4_5_Core* gl) override;
    void update(float dt, VideoManager* decoder) override;
    void render(QOpenGLFunctions_4_5_Core* gl,
                QOpenGLShaderProgram* phongShader,
                QOpenGLShaderProgram* videoShader,
                QOpenGLShaderProgram* particleShader,
                const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& cameraPos) override;

    std::vector<ScreenModel*> screens() override;
    const std::vector<RenderObject>& opaqueObjects() const override { return m_opaqueObjects; }

private:
    void buildScene(MeshUtils::GL* gl);
    void renderPhongObjects(QOpenGLShaderProgram* shader,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& cameraPos);
    void renderLightCubes(QOpenGLShaderProgram* shader,
                          const glm::mat4& view, const glm::mat4& proj,
                          const glm::vec3& cameraPos);
    void renderGlowObjects(QOpenGLShaderProgram* shader,
                           const glm::mat4& view, const glm::mat4& proj,
                           const glm::vec3& cameraPos);

    void updateAnimations(float t);
    void cleanupMeshes(MeshUtils::GL* gl);

    // Scene objects
    RenderObject m_stageBase;
    RenderObject m_stageTop;
    RenderObject m_screenBezel;
    RenderObject m_discoBall;
    std::vector<RenderObject> m_pillars;
    RenderObject m_edgeStripInner;
    RenderObject m_edgeStripOuter;

    RenderObject m_figureBody;
    RenderObject m_figureHead;
    RenderObject m_figureHair;
    RenderObject m_figureLeftArm;
    RenderObject m_figureRightArm;
    RenderObject m_figureMicStick;
    RenderObject m_figureMicHead;
    RenderObject m_figureLeftEye;
    RenderObject m_figureRightEye;
    RenderObject m_figureNose;
    RenderObject m_figureMouth;
    glm::vec3 m_figurePos{0.0f, 0.1f, 0.6f};

    RenderObject m_speakerLeft;
    RenderObject m_speakerRight;
    std::vector<RenderObject> m_steps;
    RenderObject m_movingBeamLeft;
    RenderObject m_movingBeamRight;
    std::vector<RenderObject> m_beamParticles;
    std::vector<float> m_beamParticleLife;
    RenderObject m_groundRingInner;
    RenderObject m_groundRingOuter;

    std::vector<RenderObject> m_lightCubes;
    std::vector<glm::vec3> m_cubeBasePos;
    std::vector<float> m_cubePhase;
    std::vector<glm::vec3> m_cubeRotSpeed;
    std::vector<float> m_cubeFloatAmp;

    // Particles
    Mesh m_audienceMesh;
    int m_audienceCount = 0;
    std::vector<glm::vec3> m_audienceBasePos;
    std::vector<glm::vec3> m_audienceColor;
    std::vector<float> m_audiencePhase;
    std::vector<float> m_audienceBaseAngle;
    std::vector<float> m_audienceBaseRadius;

    Mesh m_floatingParticlesMesh;
    int m_floatingParticleCount = 0;
    std::vector<glm::vec3> m_floatingParticleBasePos;
    std::vector<float> m_floatingParticlePhase;
    std::vector<float> m_floatingParticleBaseAngle;
    std::vector<float> m_floatingParticleBaseRadius;

    // Opaque objects list (for shadow pass)
    std::vector<RenderObject> m_opaqueObjects;

    // Lights
    PointLight m_dirLight;
    std::vector<PointLight> m_pointLights;

    // Screen
    std::unique_ptr<ScreenModel> m_screen;

    float m_time = 0.0f;
};
