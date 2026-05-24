#include "TechPlazaScene.h"
#include "core/VideoManager.h"
#include <cstdlib>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// Shorthands
// ============================================================

static glm::mat4 T(const glm::vec3& v) { return glm::translate(glm::mat4(1.0f), v); }
static glm::mat4 Rx(float a) { return glm::rotate(glm::mat4(1.0f), a, {1,0,0}); }
static glm::mat4 Ry(float a) { return glm::rotate(glm::mat4(1.0f), a, {0,1,0}); }
static glm::mat4 Rz(float a) { return glm::rotate(glm::mat4(1.0f), a, {0,0,1}); }

static void push3(std::vector<float>& v, float x, float y, float z) {
    v.push_back(x); v.push_back(y); v.push_back(z);
}

// Box with top tapering — for building bodies
static void addTaperedBoxVerts(std::vector<float>& v, float w, float h, float d, float cx, float cy, float cz, float taper) {
    float hw = w*0.5f, hh = h*0.5f, hd = d*0.5f;
    // 8 corners
    float bx0 = cx-hw, bx1 = cx+hw;
    float bz0 = cz-hd, bz1 = cz+hd;
    float byBot = cy-hh, byTop = cy+hh;
    // Top corners are tapered inward
    float tx0 = cx - hw*(1.0f-taper), tx1 = cx + hw*(1.0f-taper);
    float tz0 = cz - hd*(1.0f-taper), tz1 = cz + hd*(1.0f-taper);

    auto tri = [&](float x0,float y0,float z0, float x1,float y1,float z1, float x2,float y2,float z2, float nx,float ny,float nz) {
        push3(v,x0,y0,z0); push3(v,nx,ny,nz);
        push3(v,x1,y1,z1); push3(v,nx,ny,nz);
        push3(v,x2,y2,z2); push3(v,nx,ny,nz);
    };

    // Bottom face
    tri(bx0,byBot,bz0, bx1,byBot,bz1, bx0,byBot,bz1, 0,-1,0);
    tri(bx0,byBot,bz0, bx1,byBot,bz0, bx1,byBot,bz1, 0,-1,0);
    // Top face (tapered)
    tri(tx0,byTop,tz0, tx0,byTop,tz1, tx1,byTop,tz1, 0,1,0);
    tri(tx0,byTop,tz0, tx1,byTop,tz1, tx1,byTop,tz0, 0,1,0);
    // Front face (+Z)
    tri(tx0,byTop,tz1, bx0,byBot,bz1, bx1,byBot,bz1, 0,0,1);
    tri(tx0,byTop,tz1, bx1,byBot,bz1, tx1,byTop,tz1, 0,0,1);
    // Back face (-Z)
    tri(tx1,byTop,tz0, bx1,byBot,bz0, bx0,byBot,bz0, 0,0,-1);
    tri(tx1,byTop,tz0, bx0,byBot,bz0, tx0,byTop,tz0, 0,0,-1);
    // Right face (+X)
    tri(tx1,byTop,tz1, bx1,byBot,bz1, bx1,byBot,bz0, 1,0,0);
    tri(tx1,byTop,tz1, bx1,byBot,bz0, tx1,byTop,tz0, 1,0,0);
    // Left face (-X)
    tri(tx0,byTop,tz0, bx0,byBot,bz0, bx0,byBot,bz1, -1,0,0);
    tri(tx0,byTop,tz0, bx0,byBot,bz1, tx0,byTop,tz1, -1,0,0);
}

// Cone/cylinder with open ends for light beams
static void addBeamVerts(std::vector<float>& v, float rTop, float rBot, float H, float cx, float cy, float cz) {
    int segs = 16;
    float hh = H * 0.5f;
    for (int i = 0; i < segs; i++) {
        float a0 = 2.0f*M_PI * float(i)/segs;
        float a1 = 2.0f*M_PI * float(i+1)/segs;
        float ca0=cos(a0), sa0=sin(a0), ca1=cos(a1), sa1=sin(a1);
        // Bottom ring
        float bx0=cx+ca0*rBot, bz0=cz+sa0*rBot;
        float bx1=cx+ca1*rBot, bz1=cz+sa1*rBot;
        // Top ring
        float tx0=cx+ca0*rTop, tz0=cz+sa0*rTop;
        float tx1=cx+ca1*rTop, tz1=cz+sa1*rTop;
        float by=cy-hh, ty=cy+hh;
        // Side
        glm::vec3 n0 = glm::normalize(glm::vec3(ca0, (rBot-rTop)/H, sa0));
        glm::vec3 n1 = glm::normalize(glm::vec3(ca1, (rBot-rTop)/H, sa1));
        push3(v,tx0,ty,tz0); push3(v,n0.x,n0.y,n0.z);
        push3(v,bx0,by,bz0); push3(v,n0.x,n0.y,n0.z);
        push3(v,bx1,by,bz1); push3(v,n1.x,n1.y,n1.z);
        push3(v,tx0,ty,tz0); push3(v,n0.x,n0.y,n0.z);
        push3(v,bx1,by,bz1); push3(v,n1.x,n1.y,n1.z);
        push3(v,tx1,ty,tz1); push3(v,n1.x,n1.y,n1.z);
    }
}

// ============================================================
// Constructor / Destructor
// ============================================================

TechPlazaScene::TechPlazaScene() = default;
TechPlazaScene::~TechPlazaScene() = default;

// ============================================================
// Initialize
// ============================================================

void TechPlazaScene::initialize(QOpenGLFunctions_4_5_Core* gl) {
    m_gl = gl;
    m_mainScreen = std::make_unique<ScreenModel>(36.0f, 18.0f);
    m_mainScreen->initialize(gl);
    buildGround(gl);
    buildCore(gl);
    buildBuildings(gl);
    buildScreens(gl);
    buildFloatingStructures(gl);
    buildLightPillars(gl);
    buildDome(gl);
    buildParticles(gl);
    buildDrones(gl);

    // Populate opaque objects for shadow pass
    m_opaqueObjects.clear();
    m_opaqueObjects.push_back(m_ground);
    m_opaqueObjects.push_back(m_plaza);
    m_opaqueObjects.push_back(m_coreBase);
    m_opaqueObjects.push_back(m_coreSphere);
    m_opaqueObjects.push_back(m_coreTip);
    for (const auto& b : m_buildings) {
        m_opaqueObjects.push_back(b.body);
        if (b.roof.mesh.vao) m_opaqueObjects.push_back(b.roof);
    }
    for (const auto& fp : m_floatPlatforms) m_opaqueObjects.push_back(fp.plat);
    for (const auto& d : m_drones) {
        m_opaqueObjects.push_back(d.body);
        for (const auto& a : d.arms) m_opaqueObjects.push_back(a);
    }
    // (secondary screens are flat video planes, no thick frames for shadow pass)

    // Lights
    m_dirLight = {{40.0f, 60.0f, 30.0f}, {0.267f, 0.4f, 0.667f}, 5.5f}; // moon
    m_fillLight = {glm::normalize(glm::vec3(-35.0f, 20.0f, -30.0f)), {0.0f, 0.533f, 0.8f}, 3.0f};
    m_accentLight = {glm::normalize(glm::vec3(25.0f, 15.0f, 40.0f)), {0.4f, 0.0f, 0.8f}, 2.2f};

    m_pointLights.clear();
    m_pointLights.push_back({{0, 2.5f, -28}, {0.0f, 0.533f, 0.867f}, 3.0f});    // core sphere (behind screen)
    m_pointLights.push_back({{0, 14, 3}, {0.553f, 0.78f, 1.0f}, 22.0f});        // screen front spotlight
    m_pointLights.push_back({{28, 6, 6}, {0.0f, 0.4f, 0.733f}, 2.5f});          // right accent
    m_pointLights.push_back({{-28, 6, 6}, {0.0f, 0.4f, 0.733f}, 2.5f});         // left accent
}

// ============================================================
// Ground & Plaza
// ============================================================

void TechPlazaScene::buildGround(MeshUtils::GL* gl) {
    // Ground plane — large dark reflective surface
    m_ground.mesh = MeshUtils::createBox(gl, 180.0f, 0.4f, 180.0f);
    m_ground.material = {{0.039f, 0.039f, 0.078f}, {0,0,0}, 0, 0.35f, 0.7f};
    m_ground.transform = T({0, -0.35f, 0});

    // Plaza — raised circular platform
    m_plaza.mesh = MeshUtils::createCylinder(gl, 20.0f, 21.0f, 0.45f, 64);
    m_plaza.material = {{0.067f, 0.067f, 0.133f}, {0,0,0}, 0, 0.25f, 0.85f};
    m_plaza.transform = T({0, 0.05f, 0});

    // Inner glow ring
    m_innerGlowRing.mesh = MeshUtils::createTorus(gl, 12.0f, 0.3f, 48, 160);
    m_innerGlowRing.material = {{0.0f, 0.667f, 0.8f}, {0.0f, 0.533f, 0.733f}, 0.85f, 0.3f, 0.0f};
    m_innerGlowRing.transform = Rx(static_cast<float>(-M_PI/2)) * T({0, 0.4f, 0});

    // Grid circles — inner brighter, outer more transparent
    for (float r = 3.0f; r <= 19.0f; r += 2.5f) {
        RenderObject circle;
        circle.mesh = MeshUtils::createTorus(gl, r, 0.04f, 32, 160);
        float alpha;
        glm::vec3 col;
        if (r <= 12.0f) {
            alpha = 0.65f;
            col = {0.0f, 0.4f, 0.533f};
        } else if (r < 18.0f) {
            alpha = 0.35f;
            col = {0.0f, 0.133f, 0.2f};
        } else {
            alpha = 0.10f;  // outermost — barely visible
            col = {0.0f, 0.067f, 0.133f};
        }
        circle.material = {col, col, alpha, 0.5f, 0.0f};
        circle.transform = Rx(static_cast<float>(-M_PI/2)) * T({0, 0.12f, 0});
        m_gridCircles.push_back(circle);
    }

    // Radial lines
    for (int i = 0; i < 48; i++) {
        float angle = (float(i) / 48.0f) * 2.0f * M_PI;
        RenderObject line;
        line.mesh = MeshUtils::createBox(gl, 0.06f, 0.06f, 18.0f);
        float alpha = (i % 8 == 0) ? 0.55f : 0.25f;
        glm::vec3 col = (i % 8 == 0) ? glm::vec3(0.0f, 0.267f, 0.467f) : glm::vec3(0.0f, 0.067f, 0.133f);
        line.material = {col, col, alpha, 0.5f, 0.0f};
        line.transform = T({0, 0.12f, 0}) * Ry(angle);
        m_radialLines.push_back(line);
    }

    // Center glow — emissive disc
    m_centerGlow.mesh = MeshUtils::createCylinder(gl, 0.01f, 2.0f, 0.05f, 32);
    m_centerGlow.material = {{0.0f, 0.784f, 1.0f}, {0.0f, 0.667f, 0.933f}, 0.9f, 0.2f, 0.0f};
    m_centerGlow.transform = T({0, 0.5f, 0});

    // Plaza dots — small floating glows
    for (int i = 0; i < 80; i++) {
        float angle = float(rand())/RAND_MAX * 2.0f * M_PI;
        float dist = 2.0f + float(rand())/RAND_MAX * 18.0f;
        RenderObject dot;
        dot.mesh = MeshUtils::createSphere(gl, 0.15f, 8, 8);
        dot.material = {{0.0f, 0.706f, 0.863f}, {0.0f, 0.627f, 0.784f}, 0.7f, 0.3f, 0.0f};
        dot.transform = T({cos(angle)*dist, 0.28f, sin(angle)*dist});
        PlazaDot pd;
        pd.obj = dot;
        pd.baseY = 0.28f;
        pd.phase = float(rand())/RAND_MAX * 2.0f * M_PI;
        pd.speed = 0.4f + float(rand())/RAND_MAX * 1.5f;
        m_plazaDots.push_back(pd);
    }
}

// ============================================================
// Energy Core
// ============================================================

void TechPlazaScene::buildCore(MeshUtils::GL* gl) {
    m_coreBase.mesh = MeshUtils::createCylinder(gl, 1.8f, 2.2f, 1.4f, 32);
    m_coreBase.material = {{0.102f, 0.102f, 0.227f}, {0.0f, 0.067f, 0.133f}, 0.7f, 0.2f, 0.95f};
    m_coreBase.transform = T({0, 1.2f, -28});

    m_coreSphere.mesh = MeshUtils::createSphere(gl, 1.1f, 48, 48);
    m_coreSphere.material = {{0.0f, 0.267f, 0.4f}, {0.0f, 0.4f, 0.667f}, 2.8f, 0.08f, 0.3f};
    m_coreSphere.transform = T({0, 2.3f, -28});

    // 3 rotating rings
    float radii[3] = {1.8f, 2.5f, 3.2f};
    for (int i = 0; i < 3; i++) {
        RenderObject ring;
        ring.mesh = MeshUtils::createTorus(gl, radii[i], 0.12f, 48, 140);
        float b = 0.8f - i * 0.2f;
        ring.material = {{0.0f, b, 1.0f}, {0.0f, b*0.7f, 1.0f}, 0.5f, 0.3f, 0.0f};
        ring.transform = T({0, 1.6f + i*0.9f, -28});
        m_coreRings.push_back(ring);
    }
    m_coreRingRotSpeeds = {
        {0.8f, 0.7f, 0.5f},
        {1.4f, 1.6f, 1.1f},
        {2.0f, 2.5f, 1.8f}
    };

    // Tip cone
    std::vector<float> tipVerts;
    addBeamVerts(tipVerts, 0.01f, 0.35f, 1.8f, 0, 3.8f, -28);
    m_coreTip.mesh = MeshUtils::uploadMesh(gl, tipVerts, 6);
    m_coreTip.material = {{0.533f, 0.8f, 1.0f}, {0.267f, 0.533f, 0.8f}, 3.2f, 0.1f, 0.2f};
    m_coreTip.transform = glm::mat4(1.0f);

    // Glows
    m_coreGlow.mesh = MeshUtils::createCylinder(gl, 0.01f, 3.5f, 0.05f, 32);
    m_coreGlow.material = {{0.0f, 0.706f, 1.0f}, {0.0f, 0.627f, 0.933f}, 0.6f, 0.2f, 0.0f};
    m_coreGlow.transform = T({0, 2.3f, -28});

    m_tipGlow.mesh = MeshUtils::createCylinder(gl, 0.01f, 1.2f, 0.05f, 16);
    m_tipGlow.material = {{0.588f, 0.863f, 1.0f}, {0.471f, 0.749f, 1.0f}, 0.9f, 0.2f, 0.0f};
    m_tipGlow.transform = T({0, 4.6f, -28});
}

// ============================================================
// Buildings
// ============================================================

void TechPlazaScene::addBuilding(MeshUtils::GL* gl, float x, float z, float h, float w, float d,
                                  const glm::vec3& bodyColor, const glm::vec3& glowColor, RoofStyle style) {
    Building b;

    // Tapered body
    std::vector<float> bodyVerts;
    addTaperedBoxVerts(bodyVerts, w, h, d, 0, h/2, 0, 0.25f);
    b.body.mesh = MeshUtils::uploadMesh(gl, bodyVerts, 6);
    b.body.material = {bodyColor, {0,0,0}, 0, 0.3f, 0.85f};
    b.body.transform = T({x, 0, z});

    // Window stripes (emissive bands)
    int stripeCount = std::max(1, static_cast<int>(h / 2.5f));
    for (int s = 0; s < stripeCount; s++) {
        RenderObject stripe;
        stripe.mesh = MeshUtils::createBox(gl, w + 0.2f, 0.25f, d + 0.2f);
        float alpha = 0.5f + float(rand())/RAND_MAX * 0.4f;
        stripe.material = {glowColor, glowColor, alpha, 0.3f, 0.0f};
        stripe.transform = T({x, 1.8f + s * 2.5f + float(rand())/RAND_MAX * 0.8f, z});
        b.stripes.push_back(stripe);
    }

    // Top edge glow
    b.topEdge.mesh = MeshUtils::createBox(gl, w + 0.4f, 0.3f, d + 0.4f);
    b.topEdge.material = {glowColor, glowColor, 0.75f, 0.3f, 0.0f};
    b.topEdge.transform = T({x, h, z});

    // Roof ornament
    switch (style) {
    case RoofStyle::Antenna: {
        float antH = h * 0.28f;
        b.roof.mesh = MeshUtils::createCylinder(gl, 0.18f, 0.35f, antH, 8);
        b.roof.material = {{0.2f, 0.267f, 0.333f}, {0.067f, 0.133f, 0.2f}, 1.8f, 0.2f, 0.9f};
        b.roof.transform = T({x, h + antH*0.5f, z});

        b.roofGlow.mesh = MeshUtils::createSphere(gl, 0.4f, 8, 8);
        b.roofGlow.material = {{1.0f, 0.314f, 0.314f}, {1.0f, 0.2f, 0.2f}, 0.85f, 0.3f, 0.0f};
        b.roofGlow.transform = T({x, h + antH + 0.3f, z});
        break;
    }
    case RoofStyle::Dome: {
        float radius = std::min(w, d) * 0.4f;
        b.roof.mesh = MeshUtils::createSphere(gl, radius, 16, 12);
        b.roof.material = {{0.15f, 0.2f, 0.3f}, {0.05f, 0.08f, 0.15f}, 0.3f, 0.4f, 0.7f};
        b.roof.transform = T({x, h, z});

        b.roofGlow.mesh = MeshUtils::createTorus(gl, radius + 0.15f, 0.06f, 16, 8);
        b.roofGlow.material = {glowColor, glowColor, 0.5f, 0.3f, 0.0f};
        b.roofGlow.transform = T({x, h + radius * 0.1f, z});
        break;
    }
    case RoofStyle::Crown: {
        float cw = w + 1.2f;
        float cd = d + 1.2f;
        float ch = 0.8f;
        b.roof.mesh = MeshUtils::createBox(gl, cw, ch, cd);
        b.roof.material = {{0.12f, 0.18f, 0.28f}, {0.0f, 0.0f, 0.0f}, 0, 0.3f, 0.85f};
        b.roof.transform = T({x, h + ch*0.5f, z});

        b.roofGlow.mesh = MeshUtils::createBox(gl, cw + 0.3f, 0.12f, cd + 0.3f);
        b.roofGlow.material = {glowColor, glowColor, 0.4f, 0.3f, 0.0f};
        b.roofGlow.transform = T({x, h + ch + 0.06f, z});
        break;
    }
    case RoofStyle::Spire: {
        float baseR = std::min(w, d) * 0.3f;
        float spireH = h * 0.22f;
        b.roof.mesh = MeshUtils::createCylinder(gl, 0.05f, baseR, spireH, 8);
        b.roof.material = {{0.2f, 0.267f, 0.333f}, {0.067f, 0.133f, 0.2f}, 1.5f, 0.2f, 0.9f};
        b.roof.transform = T({x, h + spireH*0.5f, z});

        b.roofGlow.mesh = MeshUtils::createSphere(gl, 0.35f, 8, 8);
        b.roofGlow.material = {{1.0f, 0.314f, 0.314f}, {1.0f, 0.2f, 0.2f}, 0.85f, 0.3f, 0.0f};
        b.roofGlow.transform = T({x, h + spireH + 0.2f, z});
        break;
    }
    }

    m_buildings.push_back(b);
}

void TechPlazaScene::buildBuildings(MeshUtils::GL* gl) {
    // 8 main towers — in a wide ring, none blocking screen front (-Z)
    struct TowerCfg { float x, z, h, w, d; glm::vec3 body, glow; };
    TowerCfg mains[] = {
        { 36,   5, 34, 6,   6,   {0.067f,0.094f,0.157f}, {0.0f,0.4f,0.667f}},   // right
        {-36,   5, 32, 6.5f,6.5f,{0.082f,0.102f,0.165f}, {0.0f,0.467f,0.733f}}, // left
        {-40,  10, 38, 5.5f,7,   {0.063f,0.082f,0.125f}, {0.0f,0.333f,0.6f}},   // back-left (moved from center)
        { 40, -18, 28, 5,   5,   {0.075f,0.094f,0.157f}, {0.0f,0.4f,0.667f}},   // front-right (moved from center)
        { 26,  28, 28, 5.5f,5.5f,{0.078f,0.102f,0.157f}, {0.0f,0.533f,0.8f}},   // right-back
        {-26,  28, 30, 5.5f,5.5f,{0.067f,0.098f,0.157f}, {0.0f,0.467f,0.667f}}, // left-back
        { 30, -15, 26, 5,   5.5f,{0.082f,0.106f,0.165f}, {0.0f,0.4f,0.6f}},     // right-near
        {-30, -15, 27, 5.5f,5,   {0.071f,0.094f,0.157f}, {0.0f,0.533f,0.733f}}, // left-near
    };
    RoofStyle mainStyles[] = {
        RoofStyle::Crown, RoofStyle::Spire, RoofStyle::Dome, RoofStyle::Spire,
        RoofStyle::Crown, RoofStyle::Dome, RoofStyle::Spire, RoofStyle::Crown
    };
    for (int i = 0; i < 8; i++)
        addBuilding(gl, mains[i].x, mains[i].z, mains[i].h, mains[i].w, mains[i].d,
                    mains[i].body, mains[i].glow, mainStyles[i]);

    auto randomRoof = [](float h, float threshold) -> RoofStyle {
        if (h <= threshold) return RoofStyle::Antenna;
        float r = float(rand())/RAND_MAX;
        if (r < 0.33f) return RoofStyle::Dome;
        if (r < 0.66f) return RoofStyle::Crown;
        return RoofStyle::Spire;
    };

    // 16 mid-ring buildings
    for (int i = 0; i < 16; i++) {
        float angle = float(i)/16.0f * 2.0f*M_PI + 0.2f;
        float dist = 55.0f + float(rand())/RAND_MAX * 15.0f;
        float h = 14.0f + float(rand())/RAND_MAX * 22.0f;
        float w = 3.5f + float(rand())/RAND_MAX * 5.0f;
        float d = 3.5f + float(rand())/RAND_MAX * 5.0f;
        float hue = 0.6f + float(rand())/RAND_MAX * 0.08f;
        glm::vec3 body(0.035f+hue*0.08f, 0.04f+hue*0.07f, 0.05f+hue*0.12f);
        glm::vec3 glow(0.0f, 0.267f+hue*0.2f, 0.533f+hue*0.3f);
        addBuilding(gl, cos(angle)*dist, sin(angle)*dist, h, w, d, body, glow, randomRoof(h, 18.0f));
    }

    // 28 outer ring buildings
    for (int i = 0; i < 28; i++) {
        float angle = float(i)/28.0f * 2.0f*M_PI + float(rand())/RAND_MAX*0.3f;
        float dist = 85.0f + float(rand())/RAND_MAX * 30.0f;
        float h = 20.0f + float(rand())/RAND_MAX * 45.0f;
        float w = 4.0f + float(rand())/RAND_MAX * 8.0f;
        float d = 4.0f + float(rand())/RAND_MAX * 8.0f;
        float hue = 0.62f + float(rand())/RAND_MAX * 0.06f;
        glm::vec3 body(0.02f+hue*0.06f, 0.025f+hue*0.06f, 0.03f+hue*0.08f);
        glm::vec3 glow(0.0f, 0.267f+hue*0.2f, 0.467f+hue*0.25f);
        addBuilding(gl, cos(angle)*dist, sin(angle)*dist, h, w, d, body, glow, randomRoof(h, 25.0f));
    }

    // 22 skyline (far distance)
    for (int i = 0; i < 22; i++) {
        float angle = float(i)/22.0f * 2.0f*M_PI + 0.1f;
        float dist = 130.0f + float(rand())/RAND_MAX * 40.0f;
        float h = 30.0f + float(rand())/RAND_MAX * 70.0f;
        float w = 5.0f + float(rand())/RAND_MAX * 10.0f;
        float d = 5.0f + float(rand())/RAND_MAX * 10.0f;
        float hue = 0.63f;
        glm::vec3 body(0.02f+hue*0.05f, 0.022f+hue*0.05f, 0.025f+hue*0.06f);
        glm::vec3 glow(0.0f, 0.2f+hue*0.15f, 0.4f+hue*0.2f);
        addBuilding(gl, cos(angle)*dist, sin(angle)*dist, h, w, d, body, glow, randomRoof(h, 35.0f));
    }
}

// ============================================================
// Screens
// ============================================================

void TechPlazaScene::buildScreens(MeshUtils::GL* gl) {
    // Main screen — center stage, elevated, faces +Z toward audience
    m_mainScreen->setTransform(T({0, 15, 0}));

    // Secondary screens — flanking the main screen, none blocking front view
    struct SecCfg { glm::vec3 pos; float rotY; float w, h; };
    SecCfg secs[] = {
        {{ 28,    11,-2},     static_cast<float>(-M_PI/2), 12, 6},   // right flank
        {{-28,    11,-2},     static_cast<float>(M_PI/2),  12, 6},   // left flank
        {{ 18,    10, 16},    static_cast<float>(-M_PI*0.7f), 10, 5}, // right-back
        {{-18,    10, 16},    static_cast<float>(M_PI*0.7f),  10, 5}, // left-back
        {{ 0,     16, 22},    static_cast<float>(M_PI),     14, 7},   // far back above
        {{ -5,     14, -18},   static_cast<float>(M_PI),     14, 8},   // front center
    };
    for (auto& sc : secs) {
        SecScreen ss;
        // Video-playing screen
        ss.screenModel = std::make_unique<ScreenModel>(sc.w, sc.h);
        ss.screenModel->initialize(m_gl);
        ss.screenModel->setTransform(T(sc.pos) * Ry(sc.rotY) * Ry(static_cast<float>(M_PI)));
        m_secondaryScreens.push_back(std::move(ss));
    }

    // Holographic screens (8 in a wide semicircle behind the main screen)
    for (int i = 0; i < 8; i++) {
        HoloScreen hs;
        float angle = float(i)/8.0f * M_PI + M_PI/2.0f;  // semicircle in +Z half
        float dist = 18.0f;
        hs.screen.mesh = MeshUtils::createQuad(gl, 5.0f, 5.0f);
        hs.screen.material = {{0.0f, 0.533f, 0.733f}, {0.0f, 0.4f, 0.6f}, 0.6f, 0.3f, 0.0f};
        hs.screen.transform = T({cos(angle)*dist, 12.0f, sin(angle)*dist});
        glm::vec3 toCenter = glm::normalize(glm::vec3(0, 12.0f, 0) - glm::vec3(cos(angle)*dist, 12.0f, sin(angle)*dist));
        float yAngle = atan2(toCenter.x, toCenter.z);
        hs.screen.transform = T({cos(angle)*dist, 12.0f, sin(angle)*dist}) * Ry(yAngle);

        hs.glow.mesh = MeshUtils::createCylinder(gl, 0.01f, 3.0f, 0.05f, 24);
        hs.glow.material = {{0.0f, 0.706f, 0.863f}, {0.0f, 0.627f, 0.784f}, 0.4f, 0.2f, 0.0f};
        hs.glow.transform = T({cos(angle)*dist, 12.0f, sin(angle)*dist});
        hs.baseY = 12.0f;
        hs.phase = float(i) * M_PI / 4.0f;
        m_holoScreens.push_back(hs);
    }
}

// ============================================================
// Floating Structures
// ============================================================

void TechPlazaScene::buildFloatingStructures(MeshUtils::GL* gl) {
    // Grand rings — high above, framing the screen from the sky
    {
        RenderObject ring1;
        ring1.mesh = MeshUtils::createTorus(gl, 20.0f, 0.6f, 48, 180);
        ring1.material = {{0.0f, 0.533f, 0.733f}, {0.0f, 0.4f, 0.6f}, 0.35f, 0.3f, 0.0f};
        ring1.transform = T({0, 50, 0});
        m_floatingRings.push_back(ring1);
        m_floatingRingSpeeds.push_back(0.25f);
    }
    {
        RenderObject ring2;
        ring2.mesh = MeshUtils::createTorus(gl, 24.0f, 0.4f, 48, 180);
        ring2.material = {{0.0f, 0.4f, 0.6f}, {0.0f, 0.267f, 0.467f}, 0.3f, 0.3f, 0.0f};
        ring2.transform = Rx(0.35f) * T({0, 60, 0});
        m_floatingRings.push_back(ring2);
        m_floatingRingSpeeds.push_back(-0.2f);
    }
    {
        RenderObject ring3;
        ring3.mesh = MeshUtils::createTorus(gl, 18.0f, 0.35f, 48, 180);
        ring3.material = {{0.0f, 0.6f, 0.8f}, {0.0f, 0.467f, 0.667f}, 0.5f, 0.2f, 0.0f};
        ring3.transform = Rz(0.5f) * T({0, 52, 0});
        m_floatingRings.push_back(ring3);
        m_floatingRingSpeeds.push_back(0.18f);
    }

    // Floating platforms — further out, higher up
    for (int i = 0; i < 10; i++) {
        float angle = float(i)/10.0f * 2.0f*M_PI;
        float dist = 18.0f;
        FloatPlatform fp;
        fp.plat.mesh = MeshUtils::createCylinder(gl, 1.4f, 1.6f, 0.5f, 24);
        fp.plat.material = {{0.102f, 0.165f, 0.227f}, {0.0f, 0.067f, 0.133f}, 0.6f, 0.2f, 0.9f};
        fp.plat.transform = T({cos(angle)*dist, 22.0f + float(rand())/RAND_MAX*6.0f, sin(angle)*dist});
        fp.glow.mesh = MeshUtils::createCylinder(gl, 0.01f, 1.6f, 0.05f, 20);
        fp.glow.material = {{0.0f, 0.588f, 0.784f}, {0.0f, 0.471f, 0.667f}, 0.5f, 0.2f, 0.0f};
        fp.glow.transform = T({cos(angle)*dist, fp.plat.transform[3].y + 0.4f, sin(angle)*dist});
        fp.baseY = fp.plat.transform[3].y;
        m_floatPlatforms.push_back(fp);
    }
}

// ============================================================
// Light Pillars
// ============================================================

void TechPlazaScene::buildLightPillars(MeshUtils::GL* gl) {
    for (int i = 0; i < 16; i++) {
        float angle = float(i)/16.0f * 2.0f*M_PI;
        float dist = 30.0f;
        float h = 24.0f + float(rand())/RAND_MAX * 15.0f;
        glm::vec3 col = (i % 3 == 0) ? glm::vec3(0.0f, 0.533f, 0.8f) : glm::vec3(0.0f, 0.267f, 0.467f);

        // Inner beam
        std::vector<float> innerVerts;
        addBeamVerts(innerVerts, 0.2f, 0.4f, h, cos(angle)*dist, h/2, sin(angle)*dist);
        RenderObject inner;
        inner.mesh = MeshUtils::uploadMesh(gl, innerVerts, 6);
        inner.material = {col, col, 0.25f, 0.3f, 0.0f};
        inner.transform = glm::mat4(1.0f);
        m_lightPillarsInner.push_back(inner);

        // Outer beam
        std::vector<float> outerVerts;
        addBeamVerts(outerVerts, 0.5f, 0.8f, h, cos(angle)*dist, h/2, sin(angle)*dist);
        RenderObject outer;
        outer.mesh = MeshUtils::uploadMesh(gl, outerVerts, 6);
        outer.material = {col, col, 0.08f, 0.3f, 0.0f};
        outer.transform = glm::mat4(1.0f);
        m_lightPillarsOuter.push_back(outer);

        m_lightPillarPhases.push_back(float(i) * 0.7f);
    }
}

void TechPlazaScene::buildDome(MeshUtils::GL* gl) {
    // 10 half-torus arches — wider dome
    for (int i = 0; i < 10; i++) {
        float angle = float(i)/10.0f * M_PI;
        RenderObject arch;
        arch.mesh = MeshUtils::createTorus(gl, 30.0f, 0.15f, 48, 100);
        arch.material = {{0.0f, 0.267f, 0.4f}, {0.0f, 0.2f, 0.333f}, 0.6f, 0.3f, 0.0f};
        arch.transform = T({0, 0.5f, 0}) * Ry(angle) * Rx(static_cast<float>(-M_PI/2));
        m_domeArches.push_back(arch);
    }

    m_domeTopRing.mesh = MeshUtils::createTorus(gl, 8.0f, 0.35f, 48, 120);
    m_domeTopRing.material = {{0.0f, 0.333f, 0.467f}, {0.0f, 0.267f, 0.4f}, 0.55f, 0.3f, 0.0f};
    m_domeTopRing.transform = T({0, 20, 0});
}

// ============================================================
// Particles
// ============================================================

void TechPlazaScene::buildParticles(MeshUtils::GL* gl) {
    // Data particles (7000) — orbiting around center
    m_dataParticleCount = 7000;
    m_dataParticles.reserve(m_dataParticleCount);
    std::vector<float> dataVerts;
    dataVerts.reserve(m_dataParticleCount * 6);
    for (int i = 0; i < m_dataParticleCount; i++) {
        float angle = float(rand())/RAND_MAX * 2.0f*M_PI;
        float dist = 8.0f + float(rand())/RAND_MAX * 50.0f;
        float h = float(rand())/RAND_MAX * 50.0f;
        float x = cos(angle)*dist, z = sin(angle)*dist;
        dataVerts.push_back(x); dataVerts.push_back(h); dataVerts.push_back(z);
        float hue = 0.55f + float(rand())/RAND_MAX*0.1f;
        float r, g, b;
        if (hue < 0.33f) { r=1; g=hue*3; b=0; }
        else if (hue < 0.66f) { r=(0.66f-hue)*3; g=1; b=(hue-0.33f)*3; }
        else { r=0; g=(1.0f-hue)*3; b=1; }
        dataVerts.push_back(r*0.7f); dataVerts.push_back(g*0.7f); dataVerts.push_back(b*0.8f);

        DataParticle dp;
        dp.baseAngle = angle; dp.baseDist = dist; dp.baseHeight = h;
        dp.speed = 0.15f + float(rand())/RAND_MAX * 1.8f;
        dp.vertSpeed = (float(rand())/RAND_MAX - 0.5f) * 1.0f;
        dp.phase = float(rand())/RAND_MAX * 2.0f*M_PI;
        dp.amplitude = 0.5f + float(rand())/RAND_MAX * 4.0f;
        m_dataParticles.push_back(dp);
    }
    m_dataParticlesMesh = MeshUtils::createParticleMesh(gl, dataVerts);

    // Sky particles (4096) — spherical orbit
    m_skyParticleCount = 4096;
    m_skyParticles.reserve(m_skyParticleCount);
    std::vector<float> skyVerts;
    skyVerts.reserve(m_skyParticleCount * 6);
    for (int i = 0; i < m_skyParticleCount; i++) {
        float theta = float(rand())/RAND_MAX * 2.0f*M_PI;
        float phi = acos(2.0f * float(rand())/RAND_MAX - 1.0f);
        float radius = 65.0f + float(rand())/RAND_MAX * 30.0f;
        float x = radius * sin(phi) * cos(theta);
        float y = radius * sin(phi) * sin(theta) + 10.0f;
        float z = radius * cos(phi);
        skyVerts.push_back(x); skyVerts.push_back(y); skyVerts.push_back(z);
        float hue = 0.55f + float(rand())/RAND_MAX*0.1f;
        float r,g,b;
        if (hue < 0.33f) { r=1; g=hue*3; b=0; }
        else if (hue < 0.66f) { r=(0.66f-hue)*3; g=1; b=(hue-0.33f)*3; }
        else { r=0; g=(1.0f-hue)*3; b=1; }
        float brightness = 0.5f + float(rand())/RAND_MAX*0.5f;
        skyVerts.push_back(r*brightness); skyVerts.push_back(g*brightness); skyVerts.push_back(b*brightness);

        SkyParticle sp;
        sp.radius = radius; sp.theta = theta; sp.phi = phi;
        sp.speedTheta = 0.05f + float(rand())/RAND_MAX * 0.25f;
        sp.speedPhi = (float(rand())/RAND_MAX - 0.5f) * 0.1f;
        sp.baseYOffset = 10.0f;
        m_skyParticles.push_back(sp);
    }
    m_skyParticlesMesh = MeshUtils::createParticleMesh(gl, skyVerts);

    // Stars (4000) — distant static starfield
    m_starsCount = 4000;
    std::vector<float> starVerts;
    starVerts.reserve(m_starsCount * 6);
    for (int i = 0; i < m_starsCount; i++) {
        float theta = float(rand())/RAND_MAX * 2.0f*M_PI;
        float phi = acos(2.0f * float(rand())/RAND_MAX - 1.0f);
        float r = 90.0f + float(rand())/RAND_MAX * 50.0f;
        starVerts.push_back(r*sin(phi)*cos(theta));
        starVerts.push_back(r*sin(phi)*sin(theta));
        starVerts.push_back(r*cos(phi));
        float bright = 0.5f + float(rand())/RAND_MAX*0.5f;
        starVerts.push_back(0.6f*bright); starVerts.push_back(0.75f*bright); starVerts.push_back(1.0f*bright);
    }
    m_starsMesh = MeshUtils::createParticleMesh(gl, starVerts);
}

// ============================================================
// Drones
// ============================================================

void TechPlazaScene::buildDrones(MeshUtils::GL* gl) {
    for (int i = 0; i < 20; i++) {
        Drone drone;
        drone.body.mesh = MeshUtils::createSphere(gl, 0.3f, 8, 8);
        drone.body.material = {{0.2f, 0.267f, 0.333f}, {0.0f, 0.067f, 0.133f}, 1.2f, 0.2f, 0.9f};

        for (int a = 0; a < 4; a++) {
            RenderObject arm;
            arm.mesh = MeshUtils::createCylinder(gl, 0.04f, 0.04f, 0.9f, 8);
            arm.material = {{0.333f, 0.4f, 0.467f}, {0,0,0}, 0, 0.3f, 0.8f};
            float aa = float(a)/4.0f * 2.0f*M_PI;
            arm.transform = T({cos(aa)*0.4f, 0, sin(aa)*0.4f}) * Rz(static_cast<float>(M_PI/2)) * Ry(aa);
            drone.arms.push_back(arm);
        }

        drone.glow.mesh = MeshUtils::createSphere(gl, 0.5f, 8, 8);
        drone.glow.material = {{0.0f, 0.784f, 1.0f}, {0.0f, 0.667f, 0.933f}, 0.7f, 0.2f, 0.0f};

        float startAngle = float(rand())/RAND_MAX * 2.0f*M_PI;
        float startDist = 22.0f + float(rand())/RAND_MAX * 35.0f;
        float startHeight = 8.0f + float(rand())/RAND_MAX * 30.0f;
        drone.baseAngle = startAngle; drone.baseDist = startDist; drone.baseHeight = startHeight;
        drone.orbitSpeed = 0.1f + float(rand())/RAND_MAX * 0.6f;
        drone.vertSpeed = 0.2f + float(rand())/RAND_MAX * 0.9f;
        drone.vertAmp = 1.5f + float(rand())/RAND_MAX * 4.0f;
        drone.phase = float(rand())/RAND_MAX * 2.0f*M_PI;
        drone.radiusVar = float(rand())/RAND_MAX * 6.0f;
        m_drones.push_back(drone);
    }
}

// ============================================================
// Cleanup
// ============================================================

void TechPlazaScene::cleanup(QOpenGLFunctions_4_5_Core* gl) {
    namespace M = MeshUtils;
    for (auto& obj : m_opaqueObjects) M::deleteMesh(gl, obj.mesh);
    for (auto& obj : m_emissiveObjects) M::deleteMesh(gl, obj.mesh);
    // Zero originals that were cleaned via opaqueObjects/emissiveObjects copies
    m_ground.mesh = {};
    m_plaza.mesh = {};
    m_coreBase.mesh = {};
    m_coreSphere.mesh = {};
    m_coreTip.mesh = {};
    for (auto& b : m_buildings) {
        b.body.mesh = {};
        if (b.roof.mesh.vao) b.roof.mesh = {};
    }
    for (auto& fp : m_floatPlatforms) fp.plat.mesh = {};
    for (auto& d : m_drones) {
        d.body.mesh = {};
        for (auto& a : d.arms) a.mesh = {};
    }
    M::deleteMesh(gl, m_ground.mesh);
    M::deleteMesh(gl, m_plaza.mesh);
    M::deleteMesh(gl, m_innerGlowRing.mesh);
    for (auto& c : m_gridCircles) M::deleteMesh(gl, c.mesh);
    for (auto& l : m_radialLines) M::deleteMesh(gl, l.mesh);
    M::deleteMesh(gl, m_centerGlow.mesh);
    for (auto& pd : m_plazaDots) M::deleteMesh(gl, pd.obj.mesh);
    M::deleteMesh(gl, m_coreBase.mesh);
    M::deleteMesh(gl, m_coreSphere.mesh);
    M::deleteMesh(gl, m_coreTip.mesh);
    M::deleteMesh(gl, m_coreGlow.mesh);
    M::deleteMesh(gl, m_tipGlow.mesh);
    for (auto& r : m_coreRings) M::deleteMesh(gl, r.mesh);
    for (auto& b : m_buildings) {
        M::deleteMesh(gl, b.body.mesh);
        for (auto& s : b.stripes) M::deleteMesh(gl, s.mesh);
        M::deleteMesh(gl, b.topEdge.mesh);
        if (b.roof.mesh.vao) M::deleteMesh(gl, b.roof.mesh);
        if (b.roofGlow.mesh.vao) M::deleteMesh(gl, b.roofGlow.mesh);
    }
    for (auto& ss : m_secondaryScreens) {
        if (ss.screenModel) ss.screenModel->destroy(gl);
    }
    for (auto& hs : m_holoScreens) {
        M::deleteMesh(gl, hs.screen.mesh);
        M::deleteMesh(gl, hs.glow.mesh);
    }
    for (auto& r : m_floatingRings) M::deleteMesh(gl, r.mesh);
    for (auto& fp : m_floatPlatforms) {
        M::deleteMesh(gl, fp.plat.mesh);
        M::deleteMesh(gl, fp.glow.mesh);
    }
    for (auto& p : m_lightPillarsInner) M::deleteMesh(gl, p.mesh);
    for (auto& p : m_lightPillarsOuter) M::deleteMesh(gl, p.mesh);
    for (auto& a : m_domeArches) M::deleteMesh(gl, a.mesh);
    M::deleteMesh(gl, m_domeTopRing.mesh);
    M::deleteMesh(gl, m_dataParticlesMesh);
    M::deleteMesh(gl, m_skyParticlesMesh);
    M::deleteMesh(gl, m_starsMesh);
    for (auto& d : m_drones) {
        M::deleteMesh(gl, d.body.mesh);
        for (auto& a : d.arms) M::deleteMesh(gl, a.mesh);
        M::deleteMesh(gl, d.glow.mesh);
    }
    if (m_mainScreen) m_mainScreen->destroy(gl);
    m_opaqueObjects.clear();
    m_emissiveObjects.clear();
    m_buildings.clear();
    m_secondaryScreens.clear();
    m_holoScreens.clear();
    m_floatPlatforms.clear();
    m_lightPillarsInner.clear();
    m_lightPillarsOuter.clear();
    m_domeArches.clear();
    m_drones.clear();
    m_dataParticles.clear();
    m_skyParticles.clear();
}

// ============================================================
// Update
// ============================================================

void TechPlazaScene::update(float dt, VideoManager* decoder) {
    m_time = dt;

    if (decoder) {
        if (m_mainScreen) m_mainScreen->updateFromDecoder(decoder);
        for (auto& ss : m_secondaryScreens) {
            if (ss.screenModel) ss.screenModel->updateFromDecoder(decoder);
        }
    }

    // Core rings rotation
    for (size_t i = 0; i < m_coreRings.size() && i < m_coreRingRotSpeeds.size(); i++) {
        m_coreRings[i].transform = T({0, 1.6f + i*0.9f, -28}) *
            Rx(dt * m_coreRingRotSpeeds[i].x) *
            Ry(dt * m_coreRingRotSpeeds[i].y) *
            Rz(dt * m_coreRingRotSpeeds[i].z);
    }

    // Core sphere pulse
    float pulse = 1.0f + sin(dt * 2.5f) * 0.1f;
    m_coreSphere.transform = T({0, 2.3f, -28}) * glm::scale(glm::mat4(1.0f), glm::vec3(pulse));
    m_coreSphere.material.emissiveIntensity = 2.8f + sin(dt * 3.0f) * 0.9f;

    // Floating rings
    m_floatingRings[0].transform = T({0, 50, 0}) * Ry(dt * m_floatingRingSpeeds[0]) * Rx(dt * 0.12f);
    m_floatingRings[1].transform = Rx(0.35f) * T({0, 60.0f + sin(dt * 0.7f) * 2.0f, 0}) * Ry(dt * m_floatingRingSpeeds[1]);
    m_floatingRings[2].transform = Rz(0.5f) * T({0, 52.0f + sin(dt * 0.5f) * 1.5f, 0}) * Ry(dt * m_floatingRingSpeeds[2]);

    // Holo screens float + rotate
    for (auto& hs : m_holoScreens) {
        float newY = hs.baseY + sin(dt * 1.2f + hs.phase) * 1.8f;
        glm::vec3 pos = glm::vec3(hs.screen.transform[3]);
        float angle = atan2(pos.x, pos.z);
        float dist = sqrt(pos.x*pos.x + pos.z*pos.z);
        glm::vec3 toCenter = glm::normalize(glm::vec3(0, newY, 0) - glm::vec3(cos(angle)*dist, newY, sin(angle)*dist));
        float yAngle = atan2(toCenter.x, toCenter.z);
        hs.screen.transform = T({cos(angle)*dist, newY, sin(angle)*dist}) * Ry(yAngle + dt * 0.35f);
        hs.glow.transform = T({cos(angle)*dist, newY, sin(angle)*dist});
    }

    // Data particles
    if (m_dataParticlesMesh.vbo && m_dataParticleCount > 0) {
        std::vector<float> data;
        data.reserve(m_dataParticleCount * 6);
        for (int i = 0; i < m_dataParticleCount; i++) {
            auto& dp = m_dataParticles[i];
            float newAngle = dp.baseAngle + dt * dp.speed * 0.3f;
            float newDist = dp.baseDist + sin(dt * dp.speed + dp.phase) * dp.amplitude;
            float x = cos(newAngle) * newDist;
            float z = sin(newAngle) * newDist;
            float y = dp.baseHeight + sin(dt * dp.vertSpeed + dp.phase) * 2.5f;
            data.push_back(x); data.push_back(y); data.push_back(z);
            // Recalculate color
            data.push_back(0.0f); data.push_back(0.75f); data.push_back(0.9f);
        }
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, m_dataParticlesMesh.vbo);
        m_gl->glBufferSubData(GL_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(data.size() * sizeof(float)), data.data());
    }

    // Sky particles
    if (m_skyParticlesMesh.vbo && m_skyParticleCount > 0) {
        std::vector<float> skyData;
        skyData.reserve(m_skyParticleCount * 6);
        for (int i = 0; i < m_skyParticleCount; i++) {
            auto& sp = m_skyParticles[i];
            sp.theta += 0.016f * sp.speedTheta;
            sp.phi += 0.016f * sp.speedPhi;
            float x = sp.radius * sin(sp.phi) * cos(sp.theta);
            float y = sp.radius * sin(sp.phi) * sin(sp.theta) + sp.baseYOffset;
            float z = sp.radius * cos(sp.phi);
            skyData.push_back(x); skyData.push_back(y); skyData.push_back(z);
            skyData.push_back(0.0f); skyData.push_back(0.75f); skyData.push_back(0.95f);
        }
        m_gl->glBindBuffer(GL_ARRAY_BUFFER, m_skyParticlesMesh.vbo);
        m_gl->glBufferSubData(GL_ARRAY_BUFFER, 0,
            static_cast<GLsizeiptr>(skyData.size() * sizeof(float)), skyData.data());
    }

    // Stars rotation (slow)
    // (stars are static in VBO, but we rotate the whole group via model matrix in render)

    // Plaza dots float
    for (auto& pd : m_plazaDots) {
        float newY = pd.baseY + sin(dt * pd.speed + pd.phase) * 0.18f;
        glm::vec3 pos(pd.obj.transform[3]);
        pd.obj.transform = T({pos.x, newY, pos.z});
    }

    // Light pillars pulse
    for (size_t i = 0; i < m_lightPillarsInner.size(); i++) {
        float s = 1.0f + sin(dt * 1.8f + m_lightPillarPhases[i]) * 0.15f;
        m_lightPillarsInner[i].transform = glm::scale(glm::mat4(1.0f), glm::vec3(1, s, 1));
        m_lightPillarsOuter[i].transform = glm::scale(glm::mat4(1.0f), glm::vec3(1, s, 1));
    }

    // Dome ring
    m_domeTopRing.transform = T({0, 20, 0}) * Ry(dt * 0.4f) * Rx(dt * 0.18f);

    // Drones
    for (auto& drone : m_drones) {
        float newAngle = drone.baseAngle + dt * drone.orbitSpeed;
        float newDist = drone.baseDist + sin(dt * 0.5f + drone.phase) * drone.radiusVar;
        float x = cos(newAngle)*newDist, z = sin(newAngle)*newDist;
        float y = drone.baseHeight + sin(dt*drone.vertSpeed + drone.phase) * drone.vertAmp;
        drone.body.transform = T({x, y, z});
        drone.glow.transform = T({x, y, z});
        for (size_t a = 0; a < drone.arms.size(); a++) {
            float aa = float(a)/float(drone.arms.size()) * 2.0f*M_PI;
            drone.arms[a].transform = T({x+cos(aa)*0.4f, y, z+sin(aa)*0.4f}) * Rz(static_cast<float>(M_PI/2)) * Ry(aa);
        }
    }

    // Floating platforms
    for (auto& fp : m_floatPlatforms) {
        float newY = fp.baseY + sin(dt * 1.5f + fp.plat.transform[3].x) * 0.3f;
        fp.plat.transform = T({fp.plat.transform[3].x, newY, fp.plat.transform[3].z});
        fp.plat.transform = glm::rotate(fp.plat.transform, dt * 0.25f, {0,1,0});
        fp.glow.transform = T({fp.plat.transform[3].x, newY + 0.4f, fp.plat.transform[3].z});
    }

    // Update screen spotlight
    m_pointLights[1].intensity = 20.0f + sin(dt * 1.8f) * 3.0f;
}

// ============================================================
// Render
// ============================================================

void TechPlazaScene::render(QOpenGLFunctions_4_5_Core* gl,
                             QOpenGLShaderProgram* phongShader,
                             QOpenGLShaderProgram* videoShader,
                             QOpenGLShaderProgram* particleShader,
                             const glm::mat4& view,
                             const glm::mat4& projection,
                             const glm::vec3& cameraPos)
{
    renderOpaque(phongShader, view, projection, cameraPos);

    if (m_mainScreen) {
        m_mainScreen->render(gl, videoShader, view, projection);
    }
    for (auto& ss : m_secondaryScreens) {
        if (ss.screenModel) ss.screenModel->render(gl, videoShader, view, projection);
    }

    renderEmissive(phongShader, view, projection, cameraPos);
    renderParticleSystems(particleShader, view, projection);
}

void TechPlazaScene::renderOpaque(QOpenGLShaderProgram* shader,
                                   const glm::mat4& view, const glm::mat4& proj,
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

    // Key directional light (moon)
    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", m_dirLight.intensity);

    shader->setUniformValue("ambientColor",
        QVector3D(m_ambientColor.x, m_ambientColor.y, m_ambientColor.z));
    shader->setUniformValue("ambientIntensity", m_ambientIntensity);

    // Fill light
    shader->setUniformValue("fillLightDir",
        QVector3D(m_fillLight.direction.x, m_fillLight.direction.y, m_fillLight.direction.z));
    shader->setUniformValue("fillLightColor",
        QVector3D(m_fillLight.color.x, m_fillLight.color.y, m_fillLight.color.z));
    shader->setUniformValue("fillLightIntensity", m_fillLight.intensity);

    // Accent lights as point lights
    int numPL = std::min(static_cast<int>(m_pointLights.size()), 20);
    shader->setUniformValue("numPointLights", numPL);
    for (int i = 0; i < numPL; i++) {
        QString p = QString("pointLightPos[%1]").arg(i);
        shader->setUniformValue(p.toUtf8().constData(),
            QVector3D(m_pointLights[i].position.x, m_pointLights[i].position.y, m_pointLights[i].position.z));
        p = QString("pointLightColor[%1]").arg(i);
        shader->setUniformValue(p.toUtf8().constData(),
            QVector3D(m_pointLights[i].color.x, m_pointLights[i].color.y, m_pointLights[i].color.z));
        p = QString("pointLightIntensity[%1]").arg(i);
        shader->setUniformValue(p.toUtf8().constData(), m_pointLights[i].intensity);
    }

    auto drawObj = [&](const RenderObject& obj) {
        if (!obj.mesh.vao) return;
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
    };

    // Ground & plaza
    drawObj(m_ground);
    drawObj(m_plaza);
    drawObj(m_coreBase);
    drawObj(m_coreSphere);
    drawObj(m_coreTip);

    // Buildings
    for (const auto& b : m_buildings) {
        drawObj(b.body);
        if (b.roof.mesh.vao) drawObj(b.roof);
    }

    // Floating platforms
    for (const auto& fp : m_floatPlatforms) drawObj(fp.plat);

    // Drones
    for (const auto& drone : m_drones) {
        drawObj(drone.body);
        for (const auto& a : drone.arms) drawObj(a);
    }

    // (secondary screens are flat video planes, rendered via video shader - no frames)

    m_gl->glBindVertexArray(0);
    shader->release();
}

void TechPlazaScene::renderEmissive(QOpenGLShaderProgram* shader,
                                     const glm::mat4& view, const glm::mat4& proj,
                                     const glm::vec3& cameraPos)
{
    if (!shader || !shader->isLinked()) return;

    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_gl->glDepthMask(GL_FALSE);

    shader->bind();
    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("viewPos", QVector3D(cameraPos.x, cameraPos.y, cameraPos.z));
    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", 0.0f);
    shader->setUniformValue("ambientColor",
        QVector3D(m_ambientColor.x, m_ambientColor.y, m_ambientColor.z));
    shader->setUniformValue("ambientIntensity", 0.0f);
    shader->setUniformValue("fillLightDir", QVector3D(0, -1, 0));
    shader->setUniformValue("fillLightColor", QVector3D(0, 0, 0));
    shader->setUniformValue("fillLightIntensity", 0.0f);
    shader->setUniformValue("numPointLights", 0);

    auto drawEm = [&](const RenderObject& obj) {
        if (!obj.mesh.vao) return;
        shader->setUniformValue("model", qm(obj.transform));
        shader->setUniformValue("color",
            QVector3D(obj.material.color.x, obj.material.color.y, obj.material.color.z));
        shader->setUniformValue("emissive",
            QVector3D(obj.material.emissive.x, obj.material.emissive.y, obj.material.emissive.z));
        shader->setUniformValue("emissiveIntensity", obj.material.emissiveIntensity);
        shader->setUniformValue("roughness", obj.material.roughness);
        shader->setUniformValue("metalness", 0.0f);
        m_gl->glBindVertexArray(obj.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
    };

    // Plaza glow elements
    drawEm(m_centerGlow);
    drawEm(m_innerGlowRing);
    for (const auto& c : m_gridCircles) drawEm(c);
    for (const auto& l : m_radialLines) drawEm(l);
    for (const auto& pd : m_plazaDots) drawEm(pd.obj);

    // Core glow elements
    drawEm(m_coreGlow);
    drawEm(m_tipGlow);
    for (const auto& r : m_coreRings) drawEm(r);

    // Building stripes and edges
    for (const auto& b : m_buildings) {
        for (const auto& s : b.stripes) drawEm(s);
        drawEm(b.topEdge);
        if (b.roofGlow.mesh.vao) drawEm(b.roofGlow);
    }

    // Screens (emissive planes)
    // (secondary screens are video planes, no glow needed)
    for (const auto& hs : m_holoScreens) {
        drawEm(hs.screen);
        drawEm(hs.glow);
    }

    // Floating structures
    for (const auto& r : m_floatingRings) drawEm(r);
    for (const auto& fp : m_floatPlatforms) drawEm(fp.glow);

    // Light pillars
    for (const auto& p : m_lightPillarsOuter) drawEm(p);
    for (const auto& p : m_lightPillarsInner) drawEm(p);

    // Drones glow
    for (const auto& d : m_drones) drawEm(d.glow);

    // Dome
    for (const auto& a : m_domeArches) drawEm(a);
    drawEm(m_domeTopRing);

    m_gl->glBindVertexArray(0);
    m_gl->glDepthMask(GL_TRUE);
    m_gl->glDisable(GL_BLEND);
    shader->release();
}

void TechPlazaScene::renderParticleSystems(QOpenGLShaderProgram* shader,
                                            const glm::mat4& view, const glm::mat4& proj)
{
    if (!shader || !shader->isLinked()) return;

    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_gl->glEnable(GL_PROGRAM_POINT_SIZE);
    m_gl->glDepthMask(GL_FALSE);

    shader->bind();
    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));

    // Data particles
    if (m_dataParticlesMesh.vao) {
        shader->setUniformValue("model", qm(glm::mat4(1.0f)));
        shader->setUniformValue("pointSize", 1.5f);
        shader->setUniformValue("particleAlpha", 0.8f);
        shader->setUniformValue("petalMode", 0);
        m_gl->glBindVertexArray(m_dataParticlesMesh.vao);
        m_gl->glDrawArrays(GL_POINTS, 0, m_dataParticlesMesh.vertexCount);
    }

    // Sky particles
    if (m_skyParticlesMesh.vao) {
        shader->setUniformValue("model", qm(glm::mat4(1.0f)));
        shader->setUniformValue("pointSize", 2.0f);
        shader->setUniformValue("particleAlpha", 0.7f);
        shader->setUniformValue("petalMode", 0);
        m_gl->glBindVertexArray(m_skyParticlesMesh.vao);
        m_gl->glDrawArrays(GL_POINTS, 0, m_skyParticlesMesh.vertexCount);
    }

    // Stars
    if (m_starsMesh.vao) {
        // Slow rotation for stars
        glm::mat4 starsModel = Ry(m_time * 0.012f) * Rx(m_time * 0.006f);
        shader->setUniformValue("model", qm(starsModel));
        shader->setUniformValue("pointSize", 0.8f);
        shader->setUniformValue("particleAlpha", 0.8f);
        shader->setUniformValue("petalMode", 0);
        m_gl->glBindVertexArray(m_starsMesh.vao);
        m_gl->glDrawArrays(GL_POINTS, 0, m_starsMesh.vertexCount);
    }

    m_gl->glBindVertexArray(0);
    shader->release();
    m_gl->glDepthMask(GL_TRUE);
    m_gl->glDisable(GL_BLEND);
}

std::vector<Mesh> TechPlazaScene::shadowOnlyMeshes() const {
    return {};
}

std::vector<ScreenModel*> TechPlazaScene::screens() {
    std::vector<ScreenModel*> result;
    if (m_mainScreen) result.push_back(m_mainScreen.get());
    for (auto& ss : m_secondaryScreens)
        if (ss.screenModel) result.push_back(ss.screenModel.get());
    return result;
}
