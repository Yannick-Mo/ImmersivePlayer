#pragma once
#include "render/Scene.h"
#include "render/RenderTypes.h"
#include "render/MeshUtils.h"
#include "render/ScreenModel.h"
#include <vector>
#include <memory>

class TechPlazaScene : public Scene {
public:
    TechPlazaScene();
    ~TechPlazaScene() override;

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

    const std::vector<RenderObject>& opaqueObjects() const override { return m_opaqueObjects; }
    std::vector<ScreenModel*> screens() override;
    std::vector<Mesh> shadowOnlyMeshes() const override;

    glm::vec3 defaultTarget() const override { return {0.0f, 15.0f, 0.0f}; }
    float defaultYaw() const override { return -30.0f; }
    float defaultPitch() const override { return 12.0f; }
    float defaultRadius() const override { return 60.0f; }
    float minRadius() const override { return 18.0f; }
    float maxRadius() const override { return 160.0f; }
    float maxPitch() const override { return 78.0f; }
    float minPitch() const override { return -15.0f; }

private:
    void buildGround(MeshUtils::GL* gl);
    void buildCore(MeshUtils::GL* gl);
    void buildBuildings(MeshUtils::GL* gl);
    void buildScreens(MeshUtils::GL* gl);
    void buildFloatingStructures(MeshUtils::GL* gl);
    void buildLightPillars(MeshUtils::GL* gl);
    void buildDome(MeshUtils::GL* gl);
    void buildParticles(MeshUtils::GL* gl);
    void buildDrones(MeshUtils::GL* gl);

    void renderOpaque(QOpenGLShaderProgram* shader,
                      const glm::mat4& view, const glm::mat4& proj,
                      const glm::vec3& cameraPos);
    void renderEmissive(QOpenGLShaderProgram* shader,
                        const glm::mat4& view, const glm::mat4& proj,
                        const glm::vec3& cameraPos);
    void renderParticleSystems(QOpenGLShaderProgram* shader,
                               const glm::mat4& view, const glm::mat4& proj);

    // Helpers
    enum class RoofStyle { Antenna, Dome, Crown, Spire };

    Mesh createTaperedBox(MeshUtils::GL* gl, float w, float h, float d,
                          float taper = 0.25f, int segY = 4, int segXZ = 4);
    void addBuilding(MeshUtils::GL* gl, float x, float z, float h, float w, float d,
                     const glm::vec3& bodyColor, const glm::vec3& glowColor, RoofStyle style);

    // Ground & Plaza
    RenderObject m_ground;
    RenderObject m_plaza;
    RenderObject m_innerGlowRing;
    std::vector<RenderObject> m_gridCircles;
    std::vector<RenderObject> m_radialLines;
    RenderObject m_centerGlow;
    struct PlazaDot { RenderObject obj; float baseY; float phase; float speed; };
    std::vector<PlazaDot> m_plazaDots;

    // Energy Core
    RenderObject m_coreBase;
    RenderObject m_coreSphere;
    RenderObject m_coreTip;
    RenderObject m_coreGlow;
    RenderObject m_tipGlow;
    std::vector<RenderObject> m_coreRings;
    std::vector<glm::vec3> m_coreRingRotSpeeds;

    // Buildings
    struct Building {
        RenderObject body;
        std::vector<RenderObject> stripes;
        RenderObject topEdge;
        RenderObject roof;
        RenderObject roofGlow;
    };
    std::vector<Building> m_buildings;

    // Screens
    std::unique_ptr<ScreenModel> m_mainScreen;
    struct SecScreen {
        std::unique_ptr<ScreenModel> screenModel;
    };
    std::vector<SecScreen> m_secondaryScreens;
    struct HoloScreen {
        RenderObject screen;
        RenderObject glow;
        float baseY;
        float phase;
    };
    std::vector<HoloScreen> m_holoScreens;

    // Floating structures
    std::vector<RenderObject> m_floatingRings;
    std::vector<float> m_floatingRingSpeeds;
    struct FloatPlatform {
        RenderObject plat;
        RenderObject glow;
        float baseY;
    };
    std::vector<FloatPlatform> m_floatPlatforms;

    // Light pillars
    std::vector<RenderObject> m_lightPillarsInner;
    std::vector<RenderObject> m_lightPillarsOuter;
    std::vector<float> m_lightPillarPhases;

    // Dome
    std::vector<RenderObject> m_domeArches;
    RenderObject m_domeTopRing;

    // Drones
    struct Drone {
        RenderObject body;
        std::vector<RenderObject> arms;
        RenderObject glow;
        float baseAngle, baseDist, baseHeight;
        float orbitSpeed, vertSpeed, vertAmp, phase, radiusVar;
    };
    std::vector<Drone> m_drones;

    // Particles
    Mesh m_dataParticlesMesh;
    int m_dataParticleCount = 0;
    struct DataParticle { float baseAngle, baseDist, baseHeight, speed, vertSpeed, phase, amplitude; };
    std::vector<DataParticle> m_dataParticles;

    Mesh m_skyParticlesMesh;
    int m_skyParticleCount = 0;
    struct SkyParticle { float radius, theta, phi, speedTheta, speedPhi, baseYOffset; };
    std::vector<SkyParticle> m_skyParticles;

    Mesh m_starsMesh;
    int m_starsCount = 0;

    // Main lists
    std::vector<RenderObject> m_opaqueObjects;
    std::vector<RenderObject> m_emissiveObjects;

    // Lights
    PointLight m_dirLight;
    DirectionalLight m_fillLight;
    DirectionalLight m_accentLight;
    std::vector<PointLight> m_pointLights;
    glm::vec3 m_ambientColor{0.04f, 0.064f, 0.145f};
    float m_ambientIntensity = 2.0f;

    float m_time = 0.0f;
};
