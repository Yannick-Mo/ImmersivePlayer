#include "ConcertScene.h"
#include "core/VideoManager.h"
#include "render/MeshUtils.h"
#include <cstdlib>
#include <random>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Thread-local random engine (thread-safe, good quality)
static thread_local std::mt19937 s_rng(std::random_device{}());
static thread_local std::uniform_real_distribution<float> s_dist(0.0f, 1.0f);

ConcertScene::ConcertScene() = default;
ConcertScene::~ConcertScene() = default;

// ============================================================
// Shorthands
// ============================================================

static glm::mat4 translate(const glm::vec3& v) {
    return glm::translate(glm::mat4(1.0f), v);
}

static glm::mat4 rotX(float a) { return glm::rotate(glm::mat4(1.0f), a, glm::vec3(1,0,0)); }
static glm::mat4 rotY(float a) { return glm::rotate(glm::mat4(1.0f), a, glm::vec3(0,1,0)); }
static glm::mat4 rotZ(float a) { return glm::rotate(glm::mat4(1.0f), a, glm::vec3(0,0,1)); }

// ============================================================
// Initialize
// ============================================================

void ConcertScene::initialize(QOpenGLFunctions_4_5_Core* gl) {
    m_gl = gl;
    m_screen = std::make_unique<ScreenModel>(8.0f, 4.5f);
    m_screen->initialize(gl);
    buildScene(gl);
}

void ConcertScene::buildScene(MeshUtils::GL* gl) {
    namespace M = MeshUtils;

    // Stage base
    m_stageBase.mesh = M::createBox(gl, 10.0f, 0.3f, 8.0f);
    m_stageBase.material = {{0.067f, 0.067f, 0.133f}, {0.0f, 0.067f, 0.2f}, 0.1f, 0.3f, 0.7f};
    m_stageBase.transform = translate({0.0f, -0.2f, 0.0f});

    // Stage top
    m_stageTop.mesh = M::createBox(gl, 9.0f, 0.15f, 7.0f);
    m_stageTop.material = {{0.133f, 0.133f, 0.267f}, {0.133f, 0.267f, 0.667f}, 0.06f, 0.2f, 0.85f};
    m_stageTop.transform = glm::mat4(1.0f);

    // Screen bezel
    m_screenBezel.mesh = M::createBox(gl, 8.2f, 4.7f, 0.3f);
    m_screenBezel.material = {{0.05f, 0.05f, 0.05f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.6f, 0.05f};
    m_screenBezel.transform = translate({0.0f, 2.5f, -3.8f});

    // Screen quad transform
    m_screen->setTransform(translate({0.0f, 2.5f, -3.64f}));

    // Disco ball
    m_discoBall.mesh = M::createSphere(gl, 0.7f, 32, 32);
    m_discoBall.material = {{0.867f, 0.867f, 0.867f}, {0.533f, 0.4f, 1.0f}, 0.15f, 0.2f, 0.95f};
    m_discoBall.transform = translate({0.0f, 5.5f, 0.0f});

    // Pillars
    for (int i = 0; i < 8; i++) {
        float angle = (float(i) / 8.0f) * 2.0f * M_PI;
        float radius = 5.6f;
        RenderObject pillar;
        pillar.mesh = M::createCylinder(gl, 0.2f, 0.3f, 1.2f, 8);
        pillar.material = {{0.533f, 0.667f, 1.0f}, {0.133f, 0.4f, 0.667f}, 0.3f, 0.4f, 0.6f};
        pillar.transform = translate({cos(angle) * radius, 0.2f, sin(angle) * radius});
        m_pillars.push_back(pillar);
    }

    // Edge strips
    m_edgeStripInner.mesh = M::createTorus(gl, 4.6f, 0.08f, 64, 16);
    m_edgeStripInner.material = {{1.0f, 0.267f, 0.667f}, {1.0f, 0.267f, 0.667f}, 0.5f, 0.3f, 0.0f};
    m_edgeStripInner.transform = translate({0.0f, 0.12f, 0.0f});

    m_edgeStripOuter.mesh = M::createTorus(gl, 5.2f, 0.08f, 64, 16);
    m_edgeStripOuter.material = {{0.2f, 0.8f, 1.0f}, {0.2f, 0.8f, 1.0f}, 0.4f, 0.3f, 0.0f};
    m_edgeStripOuter.transform = translate({0.0f, 0.12f, 0.0f});

    // Audience particles
    m_audienceCount = 5000;
    m_audienceBasePos.reserve(m_audienceCount);
    m_audienceColor.reserve(m_audienceCount);
    m_audiencePhase.reserve(m_audienceCount);
    m_audienceBaseAngle.reserve(m_audienceCount);
    m_audienceBaseRadius.reserve(m_audienceCount);

    std::vector<float> audienceData;
    audienceData.reserve(m_audienceCount * 6);
    for (int i = 0; i < m_audienceCount; i++) {
        float angle = (s_dist(s_rng) - 0.5f) * M_PI * 1.8f;
        float radius = 5.5f + s_dist(s_rng) * 5.0f;
        float x = sin(angle) * radius;
        float z = cos(angle) * radius - 1.5f;
        float y = -0.2f + s_dist(s_rng) * 0.8f;
        if (radius > 7.5f) y += 0.2f;

        float c = s_dist(s_rng);
        float r, g, b;
        if (c < 0.4f)      { r = 1.0f; g = 0.65f; b = 0.7f; }
        else if (c < 0.7f) { r = 1.0f; g = 0.75f; b = 0.8f; }
        else if (c < 0.9f) { r = 0.95f; g = 0.55f; b = 0.65f; }
        else               { r = 1.0f; g = 0.85f; b = 0.75f; }

        m_audienceBasePos.emplace_back(x, y, z);
        m_audienceColor.emplace_back(r, g, b);
        m_audiencePhase.push_back(s_dist(s_rng) * 2.0f * M_PI);
        m_audienceBaseAngle.push_back(atan2(z + 1.5f, x));
        m_audienceBaseRadius.push_back(sqrt(x * x + (z + 1.5f) * (z + 1.5f)));

        audienceData.push_back(x); audienceData.push_back(y); audienceData.push_back(z);
        audienceData.push_back(r); audienceData.push_back(g); audienceData.push_back(b);
    }
    m_audienceMesh = M::createParticleMesh(gl, audienceData);

    // Floating particles
    m_floatingParticleCount = 3000;
    m_floatingParticleBasePos.reserve(m_floatingParticleCount);
    m_floatingParticlePhase.reserve(m_floatingParticleCount);
    m_floatingParticleBaseAngle.reserve(m_floatingParticleCount);
    m_floatingParticleBaseRadius.reserve(m_floatingParticleCount);

    std::vector<float> floatData;
    floatData.reserve(m_floatingParticleCount * 6);
    for (int i = 0; i < m_floatingParticleCount; i++) {
        float x = (s_dist(s_rng) - 0.5f) * 30.0f;
        float y = (s_dist(s_rng) - 0.5f) * 8.0f + 2.0f;
        float z = (s_dist(s_rng) - 0.5f) * 24.0f - 2.0f;
        m_floatingParticleBasePos.emplace_back(x, y, z);
        m_floatingParticlePhase.push_back(s_dist(s_rng) * 2.0f * M_PI);
        m_floatingParticleBaseAngle.push_back(atan2(z, x));
        m_floatingParticleBaseRadius.push_back(sqrt(x * x + z * z));
        floatData.push_back(x); floatData.push_back(y); floatData.push_back(z);
        floatData.push_back(1.0f); floatData.push_back(0.667f); floatData.push_back(0.533f);
    }
    m_floatingParticlesMesh = M::createParticleMesh(gl, floatData);

    // Light cubes
    const int cubeCount = 6;
    for (int i = 0; i < cubeCount; i++) {
        float size = 0.3f + (s_dist(s_rng)) * 0.5f;
        float angle = (float(i) / cubeCount) * 2.0f * M_PI + (s_dist(s_rng)) * 0.5f;
        float radius = 2.0f + (s_dist(s_rng)) * 1.5f;
        float y = 3.0f + (s_dist(s_rng)) * 2.0f;

        RenderObject cube;
        cube.mesh = M::createBox(gl, size, size, size);
        cube.material = {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, 0.8f, 0.3f, 0.0f};
        cube.transform = translate({cos(angle) * radius, y, sin(angle) * radius});

        m_lightCubes.push_back(cube);
        m_cubeBasePos.emplace_back(cos(angle) * radius, y, sin(angle) * radius);
        m_cubePhase.push_back(s_dist(s_rng) * 2.0f * M_PI);
        m_cubeRotSpeed.emplace_back(
            0.3f + (s_dist(s_rng)) * 0.5f,
            0.4f + (s_dist(s_rng)) * 0.4f,
            0.2f + (s_dist(s_rng)) * 0.6f);
        m_cubeFloatAmp.push_back(0.2f + (s_dist(s_rng)) * 0.2f);
    }

    // Figure
    m_figureBody.mesh = M::createCylinder(gl, 0.55f, 0.5f, 1.2f, 12);
    m_figureBody.material = {{0.8f, 0.267f, 0.533f}, {0.267f, 0.067f, 0.133f}, 0.1f, 0.3f, 0.4f};
    m_figureHead.mesh = M::createSphere(gl, 0.45f, 16, 16);
    m_figureHead.material = {{1.0f, 0.867f, 0.733f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.2f, 0.0f};
    m_figureHair.mesh = M::createCylinder(gl, 0.5f, 0.55f, 0.25f, 8);
    m_figureHair.material = {{1.0f, 0.4f, 0.8f}, {1.0f, 0.133f, 0.6f}, 0.15f, 0.2f, 0.0f};
    m_figureLeftArm.mesh = M::createCylinder(gl, 0.18f, 0.18f, 0.9f, 8);
    m_figureLeftArm.material = {{1.0f, 0.867f, 0.733f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.3f, 0.0f};
    m_figureRightArm.mesh = M::createCylinder(gl, 0.18f, 0.18f, 0.9f, 8);
    m_figureRightArm.material = {{1.0f, 0.867f, 0.733f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.3f, 0.0f};
    m_figureMicStick.mesh = M::createCylinder(gl, 0.06f, 0.08f, 0.5f, 6);
    m_figureMicStick.material = {{0.8f, 0.8f, 0.867f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.2f, 0.9f};
    m_figureMicHead.mesh = M::createSphere(gl, 0.12f, 8, 8);
    m_figureMicHead.material = {{0.667f, 0.533f, 0.4f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.3f, 0.7f};

    // Face features
    m_figureLeftEye.mesh = M::createSphere(gl, 0.07f, 8, 8);
    m_figureLeftEye.material = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.1f, 0.0f};
    m_figureRightEye.mesh = M::createSphere(gl, 0.07f, 8, 8);
    m_figureRightEye.material = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.1f, 0.0f};
    m_figureNose.mesh = M::createSphere(gl, 0.04f, 6, 6);
    m_figureNose.material = {{1.0f, 0.6f, 0.4f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.3f, 0.0f};
    m_figureMouth.mesh = M::createTorus(gl, 0.07f, 0.025f, 8, 4);
    m_figureMouth.material = {{1.0f, 0.3f, 0.4f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.3f, 0.0f};

    // Speakers
    auto speakerMat = PhongMaterial{{0.133f, 0.133f, 0.133f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.3f, 0.7f};
    m_speakerLeft.mesh = M::createBox(gl, 1.2f, 2.0f, 1.0f);
    m_speakerLeft.material = speakerMat;
    m_speakerLeft.transform = translate({-4.2f, 0.8f, -3.2f});
    m_speakerRight.mesh = M::createBox(gl, 1.2f, 2.0f, 1.0f);
    m_speakerRight.material = speakerMat;
    m_speakerRight.transform = translate({4.2f, 0.8f, -3.2f});

    // Steps
    for (int i = 0; i < 3; i++) {
        RenderObject step;
        step.mesh = M::createBox(gl, 6.0f - i * 0.8f, 0.2f, 1.2f);
        step.material = {{0.8f, 0.667f, 0.533f}, {0.0f, 0.0f, 0.0f}, 0.0f, 0.4f, 0.3f};
        step.transform = translate({0.0f, -0.05f + i * 0.2f, 3.5f - i * 0.5f});
        m_steps.push_back(step);
    }

    // Moving beams
    m_movingBeamLeft.mesh = M::createBox(gl, 0.15f, 3.0f, 0.15f);
    m_movingBeamLeft.material = {{1.0f, 0.267f, 0.667f}, {1.0f, 0.2f, 0.5f}, 0.2f, 0.5f, 0.0f};
    m_movingBeamLeft.transform = translate({-3.0f, 1.5f, 1.0f});
    m_movingBeamRight.mesh = M::createBox(gl, 0.15f, 3.0f, 0.15f);
    m_movingBeamRight.material = {{0.267f, 1.0f, 0.667f}, {0.2f, 1.0f, 0.5f}, 0.2f, 0.5f, 0.0f};
    m_movingBeamRight.transform = translate({3.0f, 1.5f, 1.0f});

    // Beam particles
    const int beamCount = 30;
    for (int i = 0; i < beamCount; i++) {
        RenderObject beam;
        beam.mesh = M::createCylinder(gl, 0.04f, 0.08f, 1.2f, 4);
        beam.material = {{1.0f, 0.667f, 0.4f}, {1.0f, 0.267f, 0.133f}, 0.0f, 0.3f, 0.0f};
        glm::mat4 t(1.0f);
        t[3] = glm::vec4(0.0f, -10.0f, 0.0f, 1.0f);
        beam.transform = t;
        m_beamParticles.push_back(beam);
        m_beamParticleLife.push_back(0.0f);
    }

    // Ground rings
    m_groundRingInner.mesh = M::createTorus(gl, 7.0f, 0.04f, 80, 8);
    m_groundRingInner.material = {{0.133f, 0.133f, 0.267f}, {0.05f, 0.05f, 0.133f}, 0.1f, 0.6f, 0.3f};
    m_groundRingInner.transform = translate({0.0f, -0.33f, 0.0f});
    m_groundRingOuter.mesh = M::createTorus(gl, 10.0f, 0.03f, 80, 8);
    m_groundRingOuter.material = {{0.133f, 0.133f, 0.267f}, {0.05f, 0.05f, 0.133f}, 0.1f, 0.6f, 0.3f};
    m_groundRingOuter.transform = translate({0.0f, -0.33f, 0.0f});

    // Lights
    m_dirLight = { {3.0f, 8.0f, 2.0f}, {1.0f, 0.933f, 0.867f}, 0.9f };

    m_pointLights.resize(9);
    const glm::vec3 lightColors[5] = {
        {1.0f, 0.2f, 0.4f}, {0.2f, 1.0f, 0.4f}, {0.2f, 0.667f, 1.0f},
        {1.0f, 0.667f, 0.2f}, {0.867f, 0.2f, 1.0f}
    };
    for (int i = 0; i < 6; i++)
        m_pointLights[i] = { {0.0f, 1.8f, 0.0f}, lightColors[i % 5], 0.5f };
    m_pointLights[6] = { {0.0f, 3.0f, 0.6f}, {1.0f, 0.8f, 0.5f}, 0.9f };
    m_pointLights[7] = { {-3.0f, 2.5f, 1.0f}, {1.0f, 0.3f, 0.6f}, 0.7f };
    m_pointLights[8] = { {3.0f, 2.5f, 1.0f}, {0.3f, 1.0f, 0.6f}, 0.7f };

    // Populate opaque objects list for shadow pass
    m_opaqueObjects.clear();
    m_opaqueObjects.push_back(m_stageBase);
    m_opaqueObjects.push_back(m_stageTop);
    m_opaqueObjects.push_back(m_screenBezel);
    m_opaqueObjects.push_back(m_discoBall);
    for (const auto& p : m_pillars) m_opaqueObjects.push_back(p);
    m_opaqueObjects.push_back(m_edgeStripInner);
    m_opaqueObjects.push_back(m_edgeStripOuter);
    for (const auto& c : m_lightCubes) m_opaqueObjects.push_back(c);
    m_opaqueObjects.push_back(m_figureBody);
    m_opaqueObjects.push_back(m_figureHead);
    m_opaqueObjects.push_back(m_figureHair);
    m_opaqueObjects.push_back(m_figureLeftArm);
    m_opaqueObjects.push_back(m_figureRightArm);
    m_opaqueObjects.push_back(m_figureMicStick);
    m_opaqueObjects.push_back(m_figureMicHead);
    m_opaqueObjects.push_back(m_figureLeftEye);
    m_opaqueObjects.push_back(m_figureRightEye);
    m_opaqueObjects.push_back(m_figureNose);
    m_opaqueObjects.push_back(m_figureMouth);
    m_opaqueObjects.push_back(m_speakerLeft);
    m_opaqueObjects.push_back(m_speakerRight);
    for (const auto& s : m_steps) m_opaqueObjects.push_back(s);
    m_opaqueObjects.push_back(m_movingBeamLeft);
    m_opaqueObjects.push_back(m_movingBeamRight);
    for (const auto& b : m_beamParticles) m_opaqueObjects.push_back(b);
    m_opaqueObjects.push_back(m_groundRingInner);
    m_opaqueObjects.push_back(m_groundRingOuter);
}

// ============================================================
// Cleanup
// ============================================================

void ConcertScene::cleanup(QOpenGLFunctions_4_5_Core* gl) {
    namespace M = MeshUtils;
    for (auto& obj : m_opaqueObjects) M::deleteMesh(gl, obj.mesh);
    M::deleteMesh(gl, m_audienceMesh);
    M::deleteMesh(gl, m_floatingParticlesMesh);
    if (m_screen) m_screen->destroy(gl);
    m_opaqueObjects.clear();
    // Zero out individual member meshes (already deleted from m_opaqueObjects)
    auto zero = [](RenderObject& obj) { obj.mesh = {0,0,0}; };
    zero(m_stageBase); zero(m_stageTop); zero(m_screenBezel); zero(m_discoBall);
    for (auto& p : m_pillars) zero(p);
    zero(m_edgeStripInner); zero(m_edgeStripOuter);
    for (auto& c : m_lightCubes) zero(c);
    zero(m_figureBody); zero(m_figureHead); zero(m_figureHair);
    zero(m_figureLeftArm); zero(m_figureRightArm);
    zero(m_figureMicStick); zero(m_figureMicHead);
    zero(m_figureLeftEye); zero(m_figureRightEye); zero(m_figureNose); zero(m_figureMouth);
    zero(m_speakerLeft); zero(m_speakerRight);
    for (auto& s : m_steps) zero(s);
    zero(m_movingBeamLeft); zero(m_movingBeamRight);
    for (auto& b : m_beamParticles) zero(b);
    zero(m_groundRingInner); zero(m_groundRingOuter);
}

void ConcertScene::cleanupMeshes(MeshUtils::GL*) {
    // cleanup() handles everything
}

// ============================================================
// Update
// ============================================================

void ConcertScene::update(float dt, VideoManager* decoder) {
    m_time = dt;

    // Pull frame for screen
    if (m_screen && decoder) {
        m_screen->updateFromDecoder(decoder);
    }

    updateAnimations(dt);
}

void ConcertScene::updateAnimations(float t) {
    // Disco ball spin
    m_discoBall.transform = glm::rotate(translate({0.0f, 5.5f, 0.0f}), t * 0.01f, {0,1,0});

    // Edge strip breathing
    float breath = sin(t * 3.0f) * 0.5f + 0.5f;
    m_edgeStripInner.material.emissiveIntensity = 0.3f + breath * 0.3f;
    m_edgeStripOuter.material.emissiveIntensity = 0.25f + breath * 0.35f;
    float hue = 0.8f + breath * 0.2f;
    m_edgeStripInner.material.emissive = {1.0f, hue * 0.5f, hue * 0.7f};

    // Animated point lights (first 6)
    for (size_t i = 0; i < 6; i++) {
        float angle = t * 0.8f + float(i);
        float radius = 4.2f;
        m_pointLights[i].position.x = cos(angle) * radius;
        m_pointLights[i].position.z = sin(angle) * radius;
        m_pointLights[i].position.y = 1.8f + sin(t * 2.0f + float(i)) * 1.2f;
        m_pointLights[i].intensity = 0.4f + sin(t * 5.0f + float(i)) * 0.25f;
        float lhue = fmod(t * 0.3f + float(i), 1.0);
        float r,g,b;
        if (lhue < 0.33f) { r=1; g=lhue*3; b=0; }
        else if (lhue < 0.66f) { r=(0.66f-lhue)*3; g=1; b=(lhue-0.33f)*3; }
        else { r=0; g=(1.0f-lhue)*3; b=1; }
        m_pointLights[i].color = {r,g,b};
    }

    // Floating particles orbital motion
    if (m_floatingParticlesMesh.vbo && m_floatingParticleCount > 0) {
        std::vector<float> data;
        data.reserve(m_floatingParticleCount * 6);
        for (int i = 0; i < m_floatingParticleCount; i++) {
            const auto& base = m_floatingParticleBasePos[i];
            float phase = m_floatingParticlePhase[i];
            float baseAngle = m_floatingParticleBaseAngle[i];
            float baseRadius = m_floatingParticleBaseRadius[i];
            float orbitSpeed = 0.5f + 0.3f * (baseRadius / 15.0f);
            float angle = baseAngle + t * orbitSpeed;
            float x = baseRadius * cos(angle) + sin(t * 0.5f + phase) * 0.2f;
            float z = baseRadius * sin(angle) + sin(t * 0.3f + phase * 0.9f) * 0.2f;
            float y = base.y + sin(t * 0.7f + phase * 1.3f) * 0.3f;
            data.push_back(x); data.push_back(y); data.push_back(z);
            data.push_back(1.0f); data.push_back(0.667f); data.push_back(0.533f);
        }
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, m_floatingParticlesMesh.vbo);
        m_gl->glBufferSubData(GL_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    }

    // Audience orbital motion
    if (m_audienceMesh.vbo && m_audienceCount > 0) {
        std::vector<float> data;
        data.reserve(m_audienceCount * 6);
        for (int i = 0; i < m_audienceCount; i++) {
            const auto& base = m_audienceBasePos[i];
            const auto& col = m_audienceColor[i];
            float phase = m_audiencePhase[i];
            float baseAngle = m_audienceBaseAngle[i];
            float baseRadius = m_audienceBaseRadius[i];
            float orbitAngle = baseAngle + t * 0.15f + phase * 0.05f;
            float x = baseRadius * cos(orbitAngle);
            float z = baseRadius * sin(orbitAngle) - 1.5f;
            float y = base.y + sin(t * 0.5f + phase) * 0.05f;
            data.push_back(x); data.push_back(y); data.push_back(z);
            data.push_back(col.r); data.push_back(col.g); data.push_back(col.b);
        }
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, m_audienceMesh.vbo);
        m_gl->glBufferSubData(GL_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    }

    // Light cubes
    for (size_t i = 0; i < m_lightCubes.size(); i++) {
        const auto& base = m_cubeBasePos[i];
        float phase = m_cubePhase[i];
        float floatOffset = sin(t * 1.2f + phase) * m_cubeFloatAmp[i];
        glm::vec3 pos = base;
        pos.y += floatOffset;
        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), t * m_cubeRotSpeed[i].x, {1,0,0});
        rot = glm::rotate(rot, t * m_cubeRotSpeed[i].y, {0,1,0});
        rot = glm::rotate(rot, t * m_cubeRotSpeed[i].z, {0,0,1});
        m_lightCubes[i].transform = translate(pos) * rot;
        float cubeHue = fmod(t * 0.1f + phase * 0.5f, 1.0f);
        float r,g,b;
        if (cubeHue < 0.33f) { r=1; g=cubeHue*3; b=0; }
        else if (cubeHue < 0.66f) { r=(0.66f-cubeHue)*3; g=1; b=(cubeHue-0.33f)*3; }
        else { r=0; g=(1.0f-cubeHue)*3; b=1; }
        m_lightCubes[i].material.color = {r,g,b};
        m_lightCubes[i].material.emissive = {r,g,b};
        float b2 = sin(t * 2.5f + phase) * 0.25f + 0.75f;
        m_lightCubes[i].material.emissiveIntensity = b2 * 0.5f;
    }

    // Figure animation
    glm::vec3 figPos = m_figurePos;
    figPos.y += sin(t * 4.0f) * 0.03f;
    m_figureBody.transform = translate(figPos + glm::vec3(0,0.6,0));
    m_figureHead.transform = translate(figPos + glm::vec3(0,1.2,0));
    m_figureHair.transform = translate(figPos + glm::vec3(0,1.45,0));
    glm::vec3 headPos = figPos + glm::vec3(0,1.2,0);
    m_figureLeftEye.transform = translate(headPos + glm::vec3(-0.13f, 0.06f, 0.43f));
    m_figureRightEye.transform = translate(headPos + glm::vec3(0.13f, 0.06f, 0.43f));
    m_figureNose.transform = translate(headPos + glm::vec3(0.0f, -0.02f, 0.455f));
    m_figureMouth.transform = glm::rotate(
        translate(headPos + glm::vec3(0.0f, -0.14f, 0.43f)),
        glm::radians(90.0f), glm::vec3(1,0,0));
    float armSwing = sin(t * 8.0f) * 0.45f;
    m_figureLeftArm.transform = translate(figPos + glm::vec3(-0.65,1.05,0));
    m_figureLeftArm.transform = glm::rotate(m_figureLeftArm.transform, 0.4f + armSwing * 0.3f, {0,0,1});
    m_figureRightArm.transform = translate(figPos + glm::vec3(0.65,1.05,0));
    m_figureRightArm.transform = glm::rotate(m_figureRightArm.transform, -0.4f - armSwing * 0.3f, {0,0,1});
    m_figureMicStick.transform = translate(figPos + glm::vec3(0.5,0.95,0.45));
    m_figureMicStick.transform = glm::rotate(m_figureMicStick.transform, 0.3f, {0,0,1});
    m_figureMicHead.transform = translate(figPos + glm::vec3(0.5,1.18,0.48));

    // Light flicker
    m_dirLight.intensity = 0.5f + sin(t * 24.0f) * 0.2f;
    for (int i = 0; i < 6; ++i) {
        float flicker = s_dist(s_rng);
        m_pointLights[i].intensity += (flicker - 0.5f) * 0.1f;
        m_pointLights[i].intensity = std::clamp(m_pointLights[i].intensity, 0.1f, 1.0f);
    }
    m_pointLights[6].position = glm::vec3(figPos.x, 3.0f, figPos.z + 0.1f);
    m_pointLights[6].intensity = 0.7f + sin(t * 6.0f) * 0.2f;

    // Moving beams
    float sweepPos = sin(t * 0.6f) * 3.0f;
    m_movingBeamLeft.transform = translate({-3.0f + sweepPos * 0.3f, 1.5f, 1.0f + sin(t*0.8f)*1.5f});
    m_movingBeamRight.transform = translate({3.0f - sweepPos * 0.3f, 1.5f, 1.0f + cos(t*0.7f)*1.5f});
    m_pointLights[7].position = {-3.0f + sweepPos*0.3f, 2.5f, 1.0f + sin(t*0.8f)*1.5f};
    m_pointLights[8].position = {3.0f - sweepPos*0.3f, 2.5f, 1.0f + cos(t*0.7f)*1.5f};

    float beamPulseL = 0.5f + sin(t * 5.0f) * 0.5f;
    m_movingBeamLeft.material.emissiveIntensity = 0.1f + beamPulseL * 0.3f;
    m_pointLights[7].intensity = 0.5f + beamPulseL * 0.3f;
    m_pointLights[7].color = {1.0f, 0.3f + beamPulseL * 0.2f, 0.6f};

    float beamPulseR = 0.5f + sin(t * 5.0f + 2.0f) * 0.5f;
    m_movingBeamRight.material.emissiveIntensity = 0.1f + beamPulseR * 0.3f;
    m_pointLights[8].intensity = 0.5f + beamPulseR * 0.3f;
    m_pointLights[8].color = {0.3f + beamPulseR*0.2f, 1.0f, 0.6f};

    // Beam particles
    for (size_t i = 0; i < m_beamParticles.size(); i++) {
        m_beamParticleLife[i] -= 0.016f;
        if (m_beamParticleLife[i] <= 0.0f) {
            if (s_dist(s_rng) > 0.97f) {
                float bx = (s_dist(s_rng) - 0.5f) * 8.0f;
                float by = s_dist(s_rng) * 2.5f;
                float bz = (s_dist(s_rng) - 0.5f) * 5.0f + 0.5f;
                m_beamParticles[i].transform = translate({bx, by, bz});
                m_beamParticles[i].material.emissiveIntensity = 0.35f;
                m_beamParticleLife[i] = 0.15f + s_dist(s_rng) * 0.2f;
            }
        } else {
            float lifeRatio = m_beamParticleLife[i] / 0.3f;
            m_beamParticles[i].material.emissiveIntensity = lifeRatio * 0.35f;
            if (lifeRatio < 0.05f) {
                glm::mat4 trans(1.0f);
                trans[3] = glm::vec4(0, -10, 0, 1);
                m_beamParticles[i].transform = trans;
            }
        }
    }
}

// ============================================================
// Render
// ============================================================

void ConcertScene::render(QOpenGLFunctions_4_5_Core* gl,
                           QOpenGLShaderProgram* phongShader,
                           QOpenGLShaderProgram* videoShader,
                           QOpenGLShaderProgram* particleShader,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& cameraPos)
{
    renderPhongObjects(phongShader, view, projection, cameraPos);

    if (m_screen) {
        m_screen->render(gl, videoShader, view, projection);
    }

    renderLightCubes(phongShader, view, projection, cameraPos);
    renderGlowObjects(phongShader, view, projection, cameraPos);

    // Particles (additive blend, depth-write off)
    if (!particleShader || !particleShader->isLinked()) return;
    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_gl->glEnable(GL_PROGRAM_POINT_SIZE);
    m_gl->glDepthMask(GL_FALSE);

    particleShader->bind();
    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };
    particleShader->setUniformValue("view", qm(view));
    particleShader->setUniformValue("projection", qm(projection));

    if (m_floatingParticlesMesh.vao) {
        particleShader->setUniformValue("model", qm(glm::mat4(1.0f)));
        particleShader->setUniformValue("pointSize", 3.0f);
        particleShader->setUniformValue("particleAlpha", 0.35f);
        particleShader->setUniformValue("petalMode", 0);
        m_gl->glBindVertexArray(m_floatingParticlesMesh.vao);
        m_gl->glDrawArrays(GL_POINTS, 0, m_floatingParticlesMesh.vertexCount);
    }

    if (m_audienceMesh.vao) {
        particleShader->setUniformValue("model", qm(glm::mat4(1.0f)));
        particleShader->setUniformValue("pointSize", 5.0f);
        particleShader->setUniformValue("particleAlpha", 0.5f);
        particleShader->setUniformValue("petalMode", 1);
        m_gl->glBindVertexArray(m_audienceMesh.vao);
        m_gl->glDrawArrays(GL_POINTS, 0, m_audienceMesh.vertexCount);
    }

    m_gl->glBindVertexArray(0);
    particleShader->release();
    m_gl->glDepthMask(GL_TRUE);
    m_gl->glDisable(GL_BLEND);
}

void ConcertScene::renderPhongObjects(QOpenGLShaderProgram* shader,
                                       const glm::mat4& view,
                                       const glm::mat4& proj,
                                       const glm::vec3& cameraPos)
{
    if (!shader || !shader->isLinked()) return;

    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    m_gl->glDisable(GL_BLEND);
    shader->bind();
    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("viewPos",
        QVector3D(cameraPos.x, cameraPos.y, cameraPos.z));

    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", m_dirLight.intensity);
    shader->setUniformValue("ambientColor", QVector3D(0.133f, 0.133f, 0.133f));
    shader->setUniformValue("ambientIntensity", 1.0f);

    shader->setUniformValue("numPointLights", std::min(static_cast<int>(m_pointLights.size()), 9));
    for (size_t i = 0; i < m_pointLights.size() && i < 9; i++) {
        QString prefix = QString("pointLightPos[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(),
            QVector3D(m_pointLights[i].position.x, m_pointLights[i].position.y, m_pointLights[i].position.z));
        prefix = QString("pointLightColor[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(),
            QVector3D(m_pointLights[i].color.x, m_pointLights[i].color.y, m_pointLights[i].color.z));
        prefix = QString("pointLightIntensity[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(), m_pointLights[i].intensity);
    }

    auto draw = [&](const RenderObject& obj) {
        shader->setUniformValue("model", qm(obj.transform));
        shader->setUniformValue("color",
            QVector3D(obj.material.color.x, obj.material.color.y, obj.material.color.z));
        shader->setUniformValue("emissive",
            QVector3D(obj.material.emissive.x, obj.material.emissive.y, obj.material.emissive.z));
        shader->setUniformValue("emissiveIntensity", obj.material.emissiveIntensity);
        shader->setUniformValue("roughness", obj.material.roughness);
        shader->setUniformValue("metalness", obj.material.metalness);
        m_gl->glBindVertexArray(obj.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
        m_gl->glBindVertexArray(0);
    };

    float ambientPulse = 0.06f + sin(m_time * 18.0f) * 0.03f;
    shader->setUniformValue("ambientColor", QVector3D(ambientPulse, ambientPulse, ambientPulse));
    shader->setUniformValue("ambientIntensity", 0.8f);

    draw(m_stageBase);
    draw(m_stageTop);
    draw(m_screenBezel);
    draw(m_discoBall);
    for (const auto& p : m_pillars) draw(p);
    draw(m_edgeStripInner);
    draw(m_edgeStripOuter);
    draw(m_figureBody);
    draw(m_figureHead);
    draw(m_figureHair);
    draw(m_figureLeftArm);
    draw(m_figureRightArm);
    draw(m_figureMicStick);
    draw(m_figureMicHead);
    draw(m_figureLeftEye);
    draw(m_figureRightEye);
    draw(m_figureNose);
    draw(m_figureMouth);
    draw(m_speakerLeft);
    draw(m_speakerRight);
    for (const auto& s : m_steps) draw(s);
    draw(m_groundRingInner);
    draw(m_groundRingOuter);

    shader->release();
}

void ConcertScene::renderLightCubes(QOpenGLShaderProgram* shader,
                                     const glm::mat4& view,
                                     const glm::mat4& proj,
                                     const glm::vec3& cameraPos)
{
    if (!shader || !shader->isLinked() || m_lightCubes.empty()) return;

    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    m_gl->glDisable(GL_BLEND);
    shader->bind();
    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("viewPos", QVector3D(cameraPos.x, cameraPos.y, cameraPos.z));
    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", m_dirLight.intensity);
    shader->setUniformValue("ambientColor", QVector3D(0.133f, 0.133f, 0.133f));
    shader->setUniformValue("ambientIntensity", 1.0f);
    shader->setUniformValue("numPointLights", std::min(static_cast<int>(m_pointLights.size()), 9));
    for (size_t i = 0; i < m_pointLights.size() && i < 9; i++) {
        QString prefix = QString("pointLightPos[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(),
            QVector3D(m_pointLights[i].position.x, m_pointLights[i].position.y, m_pointLights[i].position.z));
        prefix = QString("pointLightColor[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(),
            QVector3D(m_pointLights[i].color.x, m_pointLights[i].color.y, m_pointLights[i].color.z));
        prefix = QString("pointLightIntensity[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(), m_pointLights[i].intensity);
    }

    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (const auto& c : m_lightCubes) {
        shader->setUniformValue("model", qm(c.transform));
        shader->setUniformValue("color",
            QVector3D(c.material.color.x, c.material.color.y, c.material.color.z));
        shader->setUniformValue("emissive",
            QVector3D(c.material.emissive.x, c.material.emissive.y, c.material.emissive.z));
        shader->setUniformValue("emissiveIntensity", c.material.emissiveIntensity);
        shader->setUniformValue("roughness", c.material.roughness);
        shader->setUniformValue("metalness", c.material.metalness);
        m_gl->glBindVertexArray(c.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, c.mesh.vertexCount);
        m_gl->glBindVertexArray(0);
    }
    m_gl->glDisable(GL_BLEND);
    shader->release();
}

void ConcertScene::renderGlowObjects(QOpenGLShaderProgram* shader,
                                      const glm::mat4& view,
                                      const glm::mat4& proj,
                                      const glm::vec3& cameraPos)
{
    if (!shader || !shader->isLinked()) return;

    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    m_gl->glDisable(GL_BLEND);
    shader->bind();
    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("viewPos", QVector3D(cameraPos.x, cameraPos.y, cameraPos.z));
    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", m_dirLight.intensity);
    shader->setUniformValue("numPointLights", std::min(static_cast<int>(m_pointLights.size()), 9));
    for (size_t i = 0; i < m_pointLights.size() && i < 9; i++) {
        QString prefix = QString("pointLightPos[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(),
            QVector3D(m_pointLights[i].position.x, m_pointLights[i].position.y, m_pointLights[i].position.z));
        prefix = QString("pointLightColor[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(),
            QVector3D(m_pointLights[i].color.x, m_pointLights[i].color.y, m_pointLights[i].color.z));
        prefix = QString("pointLightIntensity[%1]").arg(i);
        shader->setUniformValue(prefix.toUtf8().constData(), m_pointLights[i].intensity);
    }
    float ambientPulse = 0.1f + sin(m_time * 18.0f) * 0.05f;
    shader->setUniformValue("ambientColor", QVector3D(ambientPulse, ambientPulse, ambientPulse));
    shader->setUniformValue("ambientIntensity", 0.8f);

    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    auto draw = [&](const RenderObject& obj) {
        shader->setUniformValue("model", qm(obj.transform));
        shader->setUniformValue("color",
            QVector3D(obj.material.color.x, obj.material.color.y, obj.material.color.z));
        shader->setUniformValue("emissive",
            QVector3D(obj.material.emissive.x, obj.material.emissive.y, obj.material.emissive.z));
        shader->setUniformValue("emissiveIntensity", obj.material.emissiveIntensity);
        shader->setUniformValue("roughness", obj.material.roughness);
        shader->setUniformValue("metalness", obj.material.metalness);
        m_gl->glBindVertexArray(obj.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
        m_gl->glBindVertexArray(0);
    };
    draw(m_movingBeamLeft);
    draw(m_movingBeamRight);
    for (const auto& b : m_beamParticles) draw(b);
    m_gl->glDisable(GL_BLEND);
    shader->release();
}

std::vector<ScreenModel*> ConcertScene::screens() {
    if (m_screen) return {m_screen.get()};
    return {};
}
