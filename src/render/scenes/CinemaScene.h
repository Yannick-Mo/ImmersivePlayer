#pragma once
#include "render/Scene.h"
#include "render/RenderTypes.h"
#include "render/MeshUtils.h"
#include "render/ScreenModel.h"
#include <vector>
#include <memory>

class CinemaScene : public Scene {
public:
    CinemaScene();
    ~CinemaScene() override;

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

    const std::vector<RenderObject>& opaqueObjects() const override { return m_objects; }
    std::vector<ScreenModel*> screens() override;
    std::vector<Mesh> shadowOnlyMeshes() const override;

    glm::vec3 defaultTarget() const override { return {0.0f, 18.0f, -35.0f}; }
    float defaultYaw() const override { return 0.0f; }
    float defaultPitch() const override { return 4.0f; }
    float defaultRadius() const override { return 113.0f; }
    float minRadius() const override { return 15.0f; }
    float maxRadius() const override { return 250.0f; }
    // preview.html: maxPolarAngle=PI*0.48(~86°), minPolarAngle=PI*0.08(~14°)
    // In our pitch system: max ~82°, min ~-76°
    float maxPitch() const override { return 82.0f; }
    float minPitch() const override { return -76.0f; }

    // Lights (set by scene, read by widget if needed)
    const PointLight& dirLight() const { return m_dirLight; }
    const std::vector<PointLight>& pointLights() const { return m_pointLights; }
    const DirectionalLight& fillLight() const { return m_fillLight; }
    const SpotLight& screenSpot() const { return m_screenSpot; }
    glm::vec3 ambientColor() const { return m_ambientColor; }
    float ambientIntensity() const { return m_ambientIntensity; }

private:
    void buildScene(MeshUtils::GL* gl);
    void buildSeats(MeshUtils::GL* gl);
    void buildLobby(MeshUtils::GL* gl);
    void buildParticles(MeshUtils::GL* gl);

    void renderOpaque(QOpenGLShaderProgram* shader,
                      const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& cameraPos);
    void renderEmissive(QOpenGLShaderProgram* shader,
                        const glm::mat4& view, const glm::mat4& proj,
                        const glm::vec3& cameraPos);
    void renderParticles_(QOpenGLShaderProgram* shader,
                         const glm::mat4& view, const glm::mat4& proj);

    // Helpers
    struct Mesh createHalfTorus(MeshUtils::GL* gl, float R, float r, int sectors, int sides);
    RenderObject makeBox(MeshUtils::GL* gl, float W, float H, float D,
                         float x, float y, float z,
                         const glm::vec3& color,
                         const glm::vec3& emissive = {0,0,0},
                         float ei = 0, float rough = 0.7f, float metal = 0.08f);
    RenderObject makeCyl(MeshUtils::GL* gl, float rTop, float rBot, float H,
                         float x, float y, float z,
                         const glm::vec3& color,
                         const glm::vec3& emissive = {0,0,0},
                         float ei = 0, float rough = 0.7f, float metal = 0.08f);
    RenderObject makeSphere_(MeshUtils::GL* gl, float R, int sec, int stk,
                             float x, float y, float z,
                             const glm::vec3& color,
                             const glm::vec3& emissive = {0,0,0},
                             float ei = 0, float rough = 0.7f, float metal = 0.08f);
    RenderObject makeTorus_(MeshUtils::GL* gl, float R, float r, int sec, int sides,
                            float x, float y, float z, float rx, float ry, float rz,
                            const glm::vec3& color,
                            const glm::vec3& emissive = {0,0,0},
                            float ei = 0, float rough = 0.7f, float metal = 0.08f);

    // Opaque geometry
    std::vector<RenderObject> m_objects;
    // Emissive (additive blended) geometry
    std::vector<RenderObject> m_emissiveObjects;
    // Lobby objects (animated rotation, rendered separately)
    std::vector<RenderObject> m_lobbyObjects;
    std::vector<RenderObject> m_lobbyEmissiveObjects;
    // Aisle strips (emissive blue glow between seat rows)
    std::vector<RenderObject> m_aisleStrips;

    // Combined seat meshes by material
    struct Mesh m_seatBaseMesh;
    struct Mesh m_seatCushionMesh;
    struct Mesh m_seatBackMesh;
    struct Mesh m_seatArmMesh;
    struct Mesh m_seatLegMesh;
    glm::vec3 m_seatBaseColor;
    glm::vec3 m_seatCushionColor;
    glm::vec3 m_seatBackColor;
    glm::vec3 m_seatArmColor;
    glm::vec3 m_seatLegColor;
    PhongMaterial m_seatBaseMat;
    PhongMaterial m_seatCushionMat;
    PhongMaterial m_seatBackMat;
    PhongMaterial m_seatArmMat;
    PhongMaterial m_seatLegMat;

    // Screen
    std::unique_ptr<ScreenModel> m_screen;

    // Particles
    Mesh m_particleMesh;
    int m_particleCount = 0;
    std::vector<glm::vec3> m_particleBasePos;

    // Lights
    PointLight m_dirLight;
    std::vector<PointLight> m_pointLights;
    DirectionalLight m_fillLight;
    SpotLight m_screenSpot;
    glm::vec3 m_ambientColor{0.725f, 0.784f, 1.0f};
    float m_ambientIntensity = 0.35f;

    // Animation
    float m_time = 0.0f;
    float m_lobbyRotation = 0.0f;
    float m_particleRotationY = 0.0f;
    float m_particleRotationX = 0.0f;
};
