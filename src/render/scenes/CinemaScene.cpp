#include "CinemaScene.h"
#include "core/VideoManager.h"
#include "render/MeshUtils.h"
#include <cstdlib>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// Helpers
// ============================================================

static glm::mat4 T(const glm::vec3& v) { return glm::translate(glm::mat4(1.0f), v); }
static glm::mat4 Rx(float a) { return glm::rotate(glm::mat4(1.0f), a, {1,0,0}); }
static glm::mat4 Ry(float a) { return glm::rotate(glm::mat4(1.0f), a, {0,1,0}); }
static glm::mat4 Rz(float a) { return glm::rotate(glm::mat4(1.0f), a, {0,0,1}); }

static void push3(std::vector<float>& v, float x, float y, float z) {
    v.push_back(x); v.push_back(y); v.push_back(z);
}

static void addBoxVerts(std::vector<float>& v, float w, float h, float d,
                        float cx, float cy, float cz) {
    float hw = w*0.5f, hh = h*0.5f, hd = d*0.5f;
    auto face = [&](float ox, float oy, float oz,
                    float ex, float ey, float ez,
                    float fx, float fy, float fz,
                    float nx, float ny, float nz) {
        auto p = [&](float x, float y, float z) {
            v.push_back(cx+x); v.push_back(cy+y); v.push_back(cz+z);
            v.push_back(nx); v.push_back(ny); v.push_back(nz);
        };
        p(ox, oy, oz); p(ox+ex, oy+ey, oz+ez); p(ox+ex+fx, oy+ey+fy, oz+ez+fz);
        p(ox, oy, oz); p(ox+ex+fx, oy+ey+fy, oz+ez+fz); p(ox+fx, oy+fy, oz+fz);
    };
    face(-hw, -hh, hd,  w,0,0,  0,h,0,  0,0,1);
    face( hw, -hh,-hd, -w,0,0,  0,h,0,  0,0,-1);
    face( hw, -hh,-hd,  0,0,d,  0,h,0,  1,0,0);
    face(-hw, -hh, hd,  0,0,-d, 0,h,0,  -1,0,0);
    face(-hw,  hh,-hd,  w,0,0,  0,0,d,  0,1,0);
    face(-hw, -hh, hd,  w,0,0,  0,0,-d, 0,-1,0);
}

static void addCylinderVerts(std::vector<float>& v, float rTop, float rBot, float H,
                             float cx, float cy, float cz,
                             float rx = 0, float ry = 0, float rz = 0,
                             int sectors = 14) {
    float hh = H * 0.5f;
    std::vector<float> ring;
    for (int i = 0; i <= sectors; i++) {
        float a = 2.0f * static_cast<float>(M_PI) * float(i) / sectors;
        float ca = cos(a), sa = sin(a);
        push3(ring, rBot * ca, -hh, rBot * sa);
        push3(ring, ca, 0, sa);
        push3(ring, rTop * ca, hh, rTop * sa);
        push3(ring, ca, 0, sa);
    }
    // Build triangles with transform
    glm::mat4 xform = T({cx, cy, cz}) * Rz(rz) * Ry(ry) * Rx(rx);
    for (int i = 0; i < sectors; i++) {
        int a = i * 2, b = a + 1, c = a + 2, d = a + 3;
        for (int k : {a, b, c, c, b, d}) {
            int base = k * 6;
            glm::vec4 pos(ring[base+0], ring[base+1], ring[base+2], 1.0f);
            glm::vec4 nrm(ring[base+3], ring[base+4], ring[base+5], 0.0f);
            pos = xform * pos;
            nrm = xform * nrm;
            push3(v, pos.x, pos.y, pos.z);
            push3(v, nrm.x, nrm.y, nrm.z);
        }
    }
}

// ============================================================
// Half-torus mesh (semicircle arch in XY plane)
// ============================================================

struct Mesh CinemaScene::createHalfTorus(MeshUtils::GL* gl, float R, float r, int sectors, int sides) {
    std::vector<float> verts;
    int halfSectors = sectors / 2;
    for (int i = 0; i <= halfSectors; i++) {
        float u = static_cast<float>(M_PI) * static_cast<float>(i) / halfSectors;
        float cu = cos(u), su = sin(u);
        for (int j = 0; j <= sides; j++) {
            float v = 2.0f * static_cast<float>(M_PI) * static_cast<float>(j) / sides;
            float cv = cos(v), sv = sin(v);
            float x = (R + r * cv) * cu;
            float y = (R + r * cv) * su;
            float z = r * sv;
            float nx = cv * cu, ny = cv * su, nz = sv;
            push3(verts, x, y, z);
            push3(verts, nx, ny, nz);
        }
    }
    std::vector<float> indexed;
    for (int i = 0; i < halfSectors; i++) {
        for (int j = 0; j < sides; j++) {
            int a = i * (sides + 1) + j;
            int b = a + sides + 1;
            int c = a + 1;
            int d = b + 1;
            for (int k : {a, b, c, c, b, d}) {
                int base = k * 6;
                for (int m = 0; m < 6; m++) indexed.push_back(verts[base + m]);
            }
        }
    }
    return MeshUtils::uploadMesh(gl, indexed, 6);
}

// ============================================================
// Object factory helpers
// ============================================================

RenderObject CinemaScene::makeBox(MeshUtils::GL* gl, float W, float H, float D,
                                   float x, float y, float z,
                                   const glm::vec3& color,
                                   const glm::vec3& emissive,
                                   float ei, float rough, float metal) {
    RenderObject obj;
    obj.mesh = MeshUtils::createBox(gl, W, H, D);
    obj.material = {color, emissive, ei, rough, metal};
    obj.transform = T({x, y, z});
    return obj;
}

RenderObject CinemaScene::makeCyl(MeshUtils::GL* gl, float rTop, float rBot, float H,
                                   float x, float y, float z,
                                   const glm::vec3& color,
                                   const glm::vec3& emissive,
                                   float ei, float rough, float metal) {
    RenderObject obj;
    obj.mesh = MeshUtils::createCylinder(gl, rTop, rBot, H, 14);
    obj.material = {color, emissive, ei, rough, metal};
    obj.transform = T({x, y, z});
    return obj;
}

RenderObject CinemaScene::makeSphere_(MeshUtils::GL* gl, float R, int sec, int stk,
                                      float x, float y, float z,
                                      const glm::vec3& color,
                                      const glm::vec3& emissive,
                                      float ei, float rough, float metal) {
    RenderObject obj;
    obj.mesh = MeshUtils::createSphere(gl, R, sec, stk);
    obj.material = {color, emissive, ei, rough, metal};
    obj.transform = T({x, y, z});
    return obj;
}

RenderObject CinemaScene::makeTorus_(MeshUtils::GL* gl, float R, float r, int sec, int sides,
                                     float x, float y, float z, float rx, float ry, float rz,
                                     const glm::vec3& color,
                                     const glm::vec3& emissive,
                                     float ei, float rough, float metal) {
    RenderObject obj;
    obj.mesh = MeshUtils::createTorus(gl, R, r, sec, sides);
    obj.material = {color, emissive, ei, rough, metal};
    obj.transform = T({x, y, z}) * Rz(rz) * Ry(ry) * Rx(rx);
    return obj;
}

// ============================================================
// Constructor / Destructor
// ============================================================

CinemaScene::CinemaScene() = default;
CinemaScene::~CinemaScene() = default;

// ============================================================
// Initialize
// ============================================================

void CinemaScene::initialize(QOpenGLFunctions_4_5_Core* gl) {
    m_gl = gl;
    m_screen = std::make_unique<ScreenModel>(86.0f, 34.0f);
    m_screen->initialize(gl);
    buildScene(gl);
    buildSeats(gl);
    buildLobby(gl);
    buildParticles(gl);
}

// ============================================================
// Material colors (exact hex→linear from preview.html)
// ============================================================

#define MC(field)  static const glm::vec3 field

MC(cFloor)        {0.086f, 0.094f, 0.129f};   // #161821
MC(cWall)         {0.055f, 0.067f, 0.102f};   // #0e111a
MC(cTrim)         {0.106f, 0.141f, 0.251f};   // #1b2440
MC(cGold)         {0.722f, 0.573f, 0.302f};   // #b8924d
MC(cRed)          {0.431f, 0.078f, 0.129f};   // #6e1421
MC(cBlue)         {0.071f, 0.133f, 0.290f};   // #12224a
MC(cDarkGlass)    {0.031f, 0.067f, 0.122f};   // #08111f
MC(cSculptureB)   {0.047f, 0.059f, 0.086f};   // #0c0f16
MC(cCeiling)      {0.039f, 0.051f, 0.078f};   // #0a0d14
MC(cStageBase)    {0.067f, 0.075f, 0.102f};   // #11131a
MC(cFrameOuter)   {0.039f, 0.051f, 0.075f};   // #0a0d13
MC(cFrameInner)   {0.008f, 0.016f, 0.024f};   // #020406
MC(cCarpet)       {0.302f, 0.071f, 0.102f};   // #4d121a
MC(cCounter)      {0.090f, 0.106f, 0.145f};   // #171b25
MC(cLobbyFloor)   {0.071f, 0.086f, 0.133f};   // #121622
MC(cMachine)      {0.125f, 0.157f, 0.227f};   // #20283a
MC(cArch)         {0.114f, 0.153f, 0.255f};   // #1d2741
MC(cColumn)       {0.063f, 0.090f, 0.153f};   // #101727
MC(cColBand)      {0.498f, 0.588f, 0.780f};   // #7f96c7
MC(cCable)        {0.267f, 0.294f, 0.369f};   // #444b5e
MC(cBulb)         {0.863f, 0.910f, 1.000f};   // #dce8ff
MC(cSeatCush)     {0.490f, 0.122f, 0.165f};   // #7d1f2a
MC(cSeatBack)     {0.290f, 0.059f, 0.102f};   // #4a0f1a
MC(cSeatLeg)      {0.227f, 0.247f, 0.290f};   // #3a3f4a
MC(cRibAlt)       {0.067f, 0.094f, 0.165f};   // #11182a
MC(cKiosk)        {0.051f, 0.063f, 0.094f};   // #0d1018
MC(cStepEven)     {0.051f, 0.063f, 0.094f};   // #0d1018
MC(cStepOdd)      {0.067f, 0.082f, 0.122f};   // #11151f
MC(cDecoA)        {0.200f, 0.290f, 0.486f};   // #334a7c
MC(cDecoB)        {0.518f, 0.820f, 1.000f};   // #84d1ff
MC(cDecoEm)       {0.067f, 0.200f, 0.333f};   // #113355
MC(cNeonBase)     {0.063f, 0.075f, 0.110f};   // #10131c
MC(cEntryBase)    {0.063f, 0.075f, 0.110f};   // #10131c (same as cNeonBase)
MC(cAisleBase)    {0.063f, 0.075f, 0.110f};
MC(cKioskBase)    {0.063f, 0.075f, 0.110f};
MC(cMachineBase)  {0.063f, 0.075f, 0.110f};

// Emissive colors
MC(eEntryGlow)    {0.400f, 0.667f, 1.000f};   // #66aaff
MC(eAisleGlow)    {0.384f, 0.722f, 1.000f};   // #62b8ff
MC(eScreenGlow)   {0.553f, 0.780f, 1.000f};   // #8dc7ff
MC(eSideGlow)     {0.400f, 0.761f, 1.000f};   // #66c2ff
MC(eTopGlow)      {0.471f, 0.749f, 1.000f};   // #78bfff
MC(eBulbEm)       {0.624f, 0.816f, 1.000f};   // #9fd0ff
MC(eParticle)     {0.624f, 0.816f, 1.000f};   // #9fd0ff
MC(eKioskEven)    {0.373f, 0.765f, 1.000f};   // #5fc3ff
MC(eKioskOdd)     {1.000f, 0.835f, 0.416f};   // #ffd56a
MC(eMachine0)     {1.000f, 0.702f, 0.278f};   // #ffb347
MC(eMachine1)     {0.345f, 0.780f, 1.000f};   // #58c7ff
MC(eScreen)       {0.553f, 0.780f, 1.000f};   // #8dc7ff (for screen)

// Neon bar colors
MC(nc0)           {0.392f, 0.843f, 1.000f};   // #64d7ff
MC(nc1)           {1.000f, 0.478f, 0.851f};   // #ff7ad9
MC(nc2)           {1.000f, 0.827f, 0.416f};   // #ffd36a
MC(nc3)           {0.447f, 1.000f, 0.690f};   // #72ffb0

// Light colors
MC(lFill)         {0.357f, 0.773f, 1.000f};   // #5bc5ff
MC(lScreen)       {0.655f, 0.843f, 1.000f};   // #a7d7ff
MC(lFloor)        {0.184f, 0.561f, 1.000f};   // #2f8fff
MC(lAmbient)      {0.725f, 0.784f, 1.000f};   // #b9c8ff
MC(lBulb)         {0.624f, 0.816f, 1.000f};   // #9fd0ff

// ============================================================
// buildScene — Architecture & decorations
// ============================================================

void CinemaScene::buildScene(MeshUtils::GL* gl) {
    auto add = [&](float W, float H, float D, float x, float y, float z,
                   const glm::vec3& color, const glm::vec3& emissive = {0,0,0},
                   float ei = 0, float rough = 0.7f, float metal = 0.08f) {
        m_objects.push_back(makeBox(gl, W, H, D, x, y, z, color, emissive, ei, rough, metal));
    };
    auto addEm = [&](float W, float H, float D, float x, float y, float z,
                     const glm::vec3& color, const glm::vec3& emissive,
                     float ei, float rough = 0.35f, float metal = 0.2f) {
        m_emissiveObjects.push_back(makeBox(gl, W, H, D, x, y, z, color, emissive, ei, rough, metal));
    };

    // ===== Floor =====
    add(520.0f, 0.5f, 320.0f, 0, -0.25f, 0, cFloor, {0,0,0}, 0, 0.95f, 0.02f);

    // ===== Base platform =====
    add(250.0f, 2.0f, 190.0f, 0, 1, -20, cSculptureB, {0,0,0}, 0, 0.95f, 0.0f);

    // ===== Walls =====
    // Side walls
    add(8.0f, 56.0f, 192.0f, -124, 28, -18, cWall);    // left
    add(8.0f, 56.0f, 192.0f,  124, 28, -18, cWall);    // right
    // Entrance columns
    add(4.0f, 56.0f, 8.0f, -122, 28, 78, cWall);       // left
    add(4.0f, 56.0f, 8.0f,  122, 28, 78, cWall);       // right
    // Top beam
    add(256.0f, 10.0f, 8.0f, 0, 50, 78, cWall);
    // Back wall
    add(256.0f, 56.0f, 8.0f, 0, 28, -114, cWall);
    // Ceiling
    add(256.0f, 3.0f, 196.0f, 0, 57, -18, cCeiling, {0,0,0}, 0, 1.0f, 0.0f);

    // ===== Entrance glow strips =====
    addEm(1.0f, 50.0f, 0.8f, -121, 25, 78.4f, cEntryBase, eEntryGlow, 1.5f);
    addEm(1.0f, 50.0f, 0.8f,  121, 25, 78.4f, cEntryBase, eEntryGlow, 1.5f);
    addEm(244.0f, 1.2f, 0.8f, 0, 46, 78.4f, cEntryBase, eEntryGlow, 1.6f);

    // ===== Wall Panels (2 sides × 4 heights × 7 columns) =====
    for (int y = 10; y <= 42; y += 8) {
        for (int side = -1; side <= 1; side += 2) {
            for (int i = 0; i < 7; i++) {
                float z = 55.0f - i * 18.0f;
                auto& c = (i % 2 == 0) ? cBlue : cWall;
                add(8.2f, 10.0f, 24.0f, side * 120.0f, y, z, c,
                    {0,0,0}, 0, 0.7f, 0.05f);
            }
        }
    }

    // ===== Ceiling Ribs (12, alternating) =====
    for (int i = 0; i < 12; i++) {
        float z = 70.0f - i * 14.0f;
        auto& c = (i % 2 == 0) ? cTrim : cRibAlt;
        float met = (i % 2 == 0) ? 0.65f : 0.7f;
        add(250.0f, 0.8f, 1.4f, 0, 55.7f, z, c, {0,0,0}, 0, 0.5f, met);
    }

    // ===== Screen Stage (at z = -88) =====
    float stageZ = -88.0f;

    add(82.0f, 4.0f, 18.0f, 0, 2, stageZ, cStageBase, {0,0,0}, 0, 0.9f, 0.0f);
    add(96.0f, 44.0f, 3.5f, 0, 26, stageZ - 1.5f, cFrameOuter, {0,0,0}, 0, 0.35f, 0.85f);
    add(91.0f, 39.0f, 1.4f, 0, 26, stageZ + 0.5f, cFrameInner, {0,0,0}, 0, 0.2f, 0.9f);

    // Video screen (positioned inside the inner frame)
    m_screen->setTransform(T({0, 26, stageZ + 1.4f}));

    // Side glow cylinders
    {
        PhongMaterial glowMat = {eSideGlow, eSideGlow, 1.8f, 0.3f, 0.0f};
        for (int side = -1; side <= 1; side += 2) {
            RenderObject bar;
            bar.mesh = MeshUtils::createCylinder(gl, 0.45f, 0.45f, 42.0f, 14);
            bar.material = glowMat;
            bar.transform = T({side * 48.3f, 25, stageZ}) * Rx(static_cast<float>(M_PI / 2));
            m_emissiveObjects.push_back(bar);
        }
    }
    // Top glow bar
    addEm(100.0f, 1.2f, 1.2f, 0, 46, stageZ, eTopGlow, eTopGlow, 1.6f);

    // ===== Neon Bars (28) =====
    {
        const glm::vec3 neonCols[4] = {nc0, nc1, nc2, nc3};
        for (int i = 0; i < 28; i++) {
            int ci = i % 4;
            float side = (i % 2 == 0) ? -1.0f : 1.0f;
            float x = side * 112.0f;
            float y = 44.0f - fmodf(i * 1.2f, 18.0f);
            float z = 65.0f - i * 7.2f;
            RenderObject bar = makeBox(gl, 18.0f, 0.45f, 0.45f, x, y, z, cNeonBase, neonCols[ci], 1.15f, 0.35f, 0.2f);
            bar.transform = T({x, y, z}) * Rz(side * 0.03f);
            m_emissiveObjects.push_back(bar);
        }
    }

    // ===== Hanging Lamps (i=-8..8, 17 lamps) =====
    for (int i = -8; i <= 8; i++) {
        float lampZ = -8.0f + (i % 2) * 4.0f;
        float lampX = i * 14.0f;

        // Cable
        m_objects.push_back(
            makeCyl(gl, 0.2f, 0.35f, 5.0f, lampX, 52.5f, lampZ, cCable, {0,0,0}, 0, 0.5f, 0.6f));
        // Bulb
        m_emissiveObjects.push_back(
            makeSphere_(gl, 0.55f, 16, 16, lampX, 49.9f, lampZ, cBulb, eBulbEm, 1.35f, 0.2f, 0.05f));
        // Point light
        PointLight pl;
        pl.position = {lampX, 49.8f, lampZ};
        pl.color = lBulb;
        pl.intensity = 1.6f;
        m_pointLights.push_back(pl);
    }

    // ===== Decorative Sculptures (torus stacks, 2 sides) =====
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 18; i++) {
            bool em = (i % 3 == 0);
            // preview.html: i % 2 ? cDecoA : cDecoB (odd→cDecoA, even→cDecoB)
            glm::vec3 col = (i % 2) ? cDecoA : cDecoB;
            glm::vec3 emCol = em ? cDecoEm : glm::vec3(0,0,0);
            float emI = em ? 0.55f : 0.0f;
            // TorusGeometry(radius, tube, radialSegments=8, tubularSegments=24)
            m_objects.push_back(
                makeTorus_(gl, 2.0f + i * 0.15f, 0.18f, 24, 8,
                           side * (94.0f + i * 0.65f), 9.0f + i * 1.6f, -64.0f + i * 3.2f,
                           0, 0, 0,
                           col, emCol, emI, 0.2f, 0.9f));
        }
    }

    // ===== Lights =====
    m_dirLight = {{5, 50, 20}, {0.78f, 0.88f, 1.0f}, 0.55f};
    // Fill light — directional (matches preview.html DirectionalLight)
    m_fillLight = {glm::normalize(glm::vec3(40.0f, 26.0f, 40.0f)), lFill, 0.95f};
    // Screen spotlight (matches preview.html SpotLight: angle=PI/7.5≈24°, penumbra=0.45)
    // inner cone = 24° * (1-0.45) ≈ 13.2°, outer cone = 24°
    m_screenSpot = {{0, 28, -72}, glm::normalize(glm::vec3(0.0f, 18.0f, -86.0f) - glm::vec3(0.0f, 28.0f, -72.0f)), lScreen, 18.0f, cos(glm::radians(13.2f)), cos(glm::radians(24.0f))};
    // Floor glow (point light)
    m_pointLights.push_back({{0, 4, 8}, lFloor, 2.1f});
    // Exterior facade
    m_pointLights.push_back({{-80, 12, 76}, {0.9f, 0.7f, 0.5f}, 0.5f});
    m_pointLights.push_back({{80, 12, 76}, {0.9f, 0.7f, 0.5f}, 0.5f});
}

// ============================================================
// buildSeats — 312 detailed seats as combined meshes by material
// ============================================================

void CinemaScene::buildSeats(MeshUtils::GL* gl) {
    const int rowCount = 12;
    const int seatsPerSide = 13;
    const float rowSpacing = 6.6f;
    const float sideGap = 8.0f;
    const float seatWidthSpacing = 3.2f;

    std::vector<float> baseVerts, cushionVerts, backVerts, armVerts, legVerts;

    for (int row = 0; row < rowCount; row++) {
        float z = -(52.0f - row * rowSpacing);
        float rise = row * 0.45f;
        float yBase = 2.05f + rise;

        for (int side = -1; side <= 1; side += 2) {
            for (int i = 0; i < seatsPerSide; i++) {
                float x = side * (sideGap + i * seatWidthSpacing);

                // Base: Box(2.2, 0.55, 2.2) at (x, yBase+0.3, z)
                addBoxVerts(baseVerts, 2.2f, 0.55f, 2.2f, x, yBase + 0.3f, z);

                // Cushion: Box(2.08, 0.7, 2.05) at (x, yBase+0.9, z)
                addBoxVerts(cushionVerts, 2.08f, 0.7f, 2.05f, x, yBase + 0.9f, z);

                // Back: Box(2.08, 2.25, 0.35) at (x, yBase+2.05, z+0.88)
                addBoxVerts(backVerts, 2.08f, 2.25f, 0.35f, x, yBase + 2.05f, z + 0.88f);

                // Inner arm: Box(0.24, 1.0, 1.85) at (x - side*1.13, yBase+1.05, z-0.1)
                addBoxVerts(armVerts, 0.24f, 1.0f, 1.85f, x - side * 1.13f, yBase + 1.05f, z - 0.1f);
                // Outer arm: Box(0.24, 1.0, 1.85) at (x + side*1.13, yBase+1.05, z-0.1)
                addBoxVerts(armVerts, 0.24f, 1.0f, 1.85f, x + side * 1.13f, yBase + 1.05f, z - 0.1f);

                // 4 Legs: Cylinder(0.08, 0.08, 0.55, 8) at 4 corners
                addCylinderVerts(legVerts, 0.08f, 0.08f, 0.55f,
                                 x + 0.9f, yBase, z - 0.7f);
                addCylinderVerts(legVerts, 0.08f, 0.08f, 0.55f,
                                 x - 0.9f, yBase, z - 0.7f);
                addCylinderVerts(legVerts, 0.08f, 0.08f, 0.55f,
                                 x + 0.9f, yBase, z + 0.7f);
                addCylinderVerts(legVerts, 0.08f, 0.08f, 0.55f,
                                 x - 0.9f, yBase, z + 0.7f);
            }
        }

        // Aisle strip for this row (separate emissive object)
        {
            RenderObject strip;
            strip.mesh = MeshUtils::createBox(gl, 2.2f, 0.08f, 5.4f);
            strip.material = {cAisleBase, eAisleGlow, 0.8f, 0.35f, 0.2f};
            strip.transform = T({0, 0.08f, z + 0.2f});
            m_aisleStrips.push_back(strip);
        }
    }

    m_seatBaseMesh = MeshUtils::uploadMesh(gl, baseVerts, 6);
    m_seatBaseColor = cRed;
    m_seatBaseMat = {cRed, {0,0,0}, 0, 0.92f, 0.02f};

    m_seatCushionMesh = MeshUtils::uploadMesh(gl, cushionVerts, 6);
    m_seatCushionColor = cSeatCush;
    m_seatCushionMat = {cSeatCush, {0,0,0}, 0, 0.95f, 0.0f};

    m_seatBackMesh = MeshUtils::uploadMesh(gl, backVerts, 6);
    m_seatBackColor = cSeatBack;
    m_seatBackMat = {cSeatBack, {0,0,0}, 0, 0.98f, 0.0f};

    m_seatArmMesh = MeshUtils::uploadMesh(gl, armVerts, 6);
    m_seatArmColor = cGold;
    m_seatArmMat = {cGold, {0,0,0}, 0, 0.28f, 0.9f};

    m_seatLegMesh = MeshUtils::uploadMesh(gl, legVerts, 6);
    m_seatLegColor = cSeatLeg;
    m_seatLegMat = {cSeatLeg, {0,0,0}, 0, 0.5f, 0.7f};
}

// ============================================================
// buildLobby — Front lobby at z≈66
// ============================================================

void CinemaScene::buildLobby(MeshUtils::GL* gl) {
    float lobbyZ = 66.0f;

    auto addLobby = [&](float W, float H, float D, float x, float y, float z,
                        const glm::vec3& color, const glm::vec3& emissive = {0,0,0},
                        float ei = 0, float rough = 0.7f, float metal = 0.08f) {
        m_lobbyObjects.push_back(makeBox(gl, W, H, D, x, y, z, color, emissive, ei, rough, metal));
    };
    auto addLobbyEm = [&](float W, float H, float D, float x, float y, float z,
                          const glm::vec3& color, const glm::vec3& emissive,
                          float ei, float rough = 0.35f, float metal = 0.2f) {
        m_lobbyEmissiveObjects.push_back(makeBox(gl, W, H, D, x, y, z, color, emissive, ei, rough, metal));
    };

    // Lobby floor
    addLobby(210.0f, 1.2f, 18.0f, 0, 0.6f, lobbyZ, cLobbyFloor);

    // Arch (half-torus) — TorusGeometry(78, 2.2, 12, 84, PI) => 42 half tubular segments
    {
        RenderObject arch;
        arch.mesh = createHalfTorus(gl, 78.0f, 2.2f, 84, 12);
        arch.material = {cArch, {0,0,0}, 0, 0.3f, 0.7f};
        arch.transform = T({0, 13, lobbyZ + 7.0f});
        m_lobbyObjects.push_back(arch);
    }

    // Counter
    addLobby(64.0f, 5.2f, 14.0f, 0, 2.6f, lobbyZ, cCounter);
    addLobby(66.0f, 1.2f, 16.0f, 0, 5.4f, lobbyZ, cTrim, {0,0,0}, 0, 0.45f, 0.65f);

    // Kiosks (i=-2..2, 5 total)
    for (int i = -2; i <= 2; i++) {
        addLobby(8.0f, 8.0f, 1.2f, i * 12.0f, 7.5f, lobbyZ - 4.6f, cKiosk,
                 {0,0,0}, 0, 0.7f, 0.2f);
        auto& emCol = (i % 2 == 0) ? eKioskOdd : eKioskEven;
        addLobbyEm(6.0f, 4.5f, 0.05f, i * 12.0f, 8.2f, lobbyZ - 4.0f, cKioskBase, emCol, 1.0f);
    }

    // Ticket machines (i=-1..1, 3 total)
    for (int i = -1; i <= 1; i++) {
        addLobby(7.0f, 12.0f, 5.0f, i * 18.0f, 6, lobbyZ - 10.0f, cMachine,
                 {0,0,0}, 0, 0.5f, 0.35f);
        auto& emCol = (i == 0) ? eMachine0 : eMachine1;
        addLobbyEm(6.3f, 10.5f, 0.4f, i * 18.0f, 6, lobbyZ - 7.6f, cMachineBase, emCol, 1.5f);
    }

    // Lobby columns (i=-6..6, 13 total)
    for (int i = -6; i <= 6; i++) {
        float colX = i * 16.0f;
        // Column
        RenderObject col;
        col.mesh = MeshUtils::createCylinder(gl, 1.8f, 2.2f, 20.0f, 16);
        col.material = {cColumn, {0,0,0}, 0, 0.62f, 0.18f};
        col.transform = T({colX, 10.0f, lobbyZ + 15.2f});
        m_lobbyObjects.push_back(col);

        // Band — TorusGeometry(2.4, 0.18, 8, 20) => tubular=20, radial=8
        RenderObject band;
        band.mesh = MeshUtils::createTorus(gl, 2.4f, 0.18f, 20, 8);
        band.material = {cColBand, {0,0,0}, 0, 0.18f, 0.95f};
        band.transform = T({colX, 17.2f, lobbyZ + 15.0f});
        m_lobbyObjects.push_back(band);
    }

    // Carpet & Steps (not part of lobby group — they're in the main auditorium)
    m_objects.push_back(makeBox(gl, 6.5f, 0.12f, 110.0f, 0, 0.08f, 10, cCarpet, {0,0,0}, 0, 0.98f, 0.0f));
    for (int i = 0; i < 18; i++) {
        float zStep = 48.0f - i * 6.0f;
        float yStep = 0.3f + (17 - i) * 0.72f;
        auto& col = (i % 2 == 0) ? cStepEven : cStepOdd;
        m_objects.push_back(makeBox(gl, 8.0f, 0.38f, 4.8f, 0, yStep, zStep, col,
                                    {0,0,0}, 0, 0.95f, 0.0f));
    }
}

// ============================================================
// buildParticles — 1600 floating particles
// ============================================================

void CinemaScene::buildParticles(MeshUtils::GL* gl) {
    m_particleCount = 1600;
    m_particleBasePos.reserve(m_particleCount);
    std::vector<float> pData;
    pData.reserve(m_particleCount * 6);

    for (int i = 0; i < m_particleCount; i++) {
        float x = (float(rand()) / RAND_MAX - 0.5f) * 320.0f;
        float y = (float(rand()) / RAND_MAX) * 70.0f + 2.0f;
        float z = (float(rand()) / RAND_MAX - 0.5f) * 260.0f;
        m_particleBasePos.emplace_back(x, y, z);
        pData.push_back(x); pData.push_back(y); pData.push_back(z);
        pData.push_back(eParticle.x); pData.push_back(eParticle.y); pData.push_back(eParticle.z);
    }
    m_particleMesh = MeshUtils::createParticleMesh(gl, pData);
}

// ============================================================
// Cleanup
// ============================================================

void CinemaScene::cleanup(QOpenGLFunctions_4_5_Core* gl) {
    namespace M = MeshUtils;
    for (auto& obj : m_objects) M::deleteMesh(gl, obj.mesh);
    for (auto& obj : m_emissiveObjects) M::deleteMesh(gl, obj.mesh);
    for (auto& obj : m_lobbyObjects) M::deleteMesh(gl, obj.mesh);
    for (auto& obj : m_lobbyEmissiveObjects) M::deleteMesh(gl, obj.mesh);
    for (auto& obj : m_aisleStrips) M::deleteMesh(gl, obj.mesh);
    M::deleteMesh(gl, m_seatBaseMesh);
    M::deleteMesh(gl, m_seatCushionMesh);
    M::deleteMesh(gl, m_seatBackMesh);
    M::deleteMesh(gl, m_seatArmMesh);
    M::deleteMesh(gl, m_seatLegMesh);
    M::deleteMesh(gl, m_particleMesh);
    if (m_screen) m_screen->destroy(gl);
    m_objects.clear();
    m_emissiveObjects.clear();
    m_lobbyObjects.clear();
    m_lobbyEmissiveObjects.clear();
    m_aisleStrips.clear();
}

// ============================================================
// Update
// ============================================================

void CinemaScene::update(float dt, VideoManager* decoder) {
    m_time = dt;

    if (m_screen && decoder) {
        m_screen->updateFromDecoder(decoder);
    }

    // Particle group rotation (matches preview.html)
    m_particleRotationY = dt * 0.02f;
    m_particleRotationX = sin(dt * 0.1f) * 0.02f;

    // Lobby rotation animation (matches preview.html)
    m_lobbyRotation = sin(dt * 0.08f) * 0.02f;

    // Pulse screen spotlight
    m_screenSpot.intensity = 16.0f + sin(dt * 1.8f) * 2.5f;

    // Pulse floor glow (index 17 = after 17 hanging lamps)
    int floorIdx = 17; // 17 hanging lamps, then floor glow
    if (floorIdx < static_cast<int>(m_pointLights.size())) {
        m_pointLights[floorIdx].intensity = 1.8f + sin(dt * 1.3f) * 0.3f;
    }
}

// ============================================================
// Render
// ============================================================

void CinemaScene::render(QOpenGLFunctions_4_5_Core* gl,
                          QOpenGLShaderProgram* phongShader,
                          QOpenGLShaderProgram* videoShader,
                          QOpenGLShaderProgram* particleShader,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          const glm::vec3& cameraPos)
{
    // Bind and set scene-specific lights + ambient, then draw geometry
    renderOpaque(phongShader, view, projection, cameraPos);

    if (m_screen) {
        m_screen->render(gl, videoShader, view, projection);
    }

    renderEmissive(phongShader, view, projection, cameraPos);
    renderParticles_(particleShader, view, projection);
}

void CinemaScene::renderOpaque(QOpenGLShaderProgram* shader,
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

    // Lobby pivot for rotation
    glm::vec3 lobbyPivot(0, 0, 66.0f);
    glm::mat4 lobbyRot = Ry(m_lobbyRotation);

    m_gl->glDisable(GL_BLEND);
    shader->bind();

    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("viewPos",
        QVector3D(cameraPos.x, cameraPos.y, cameraPos.z));

    // Scene lights
    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", m_dirLight.intensity);
    shader->setUniformValue("ambientColor",
        QVector3D(m_ambientColor.x, m_ambientColor.y, m_ambientColor.z));
    shader->setUniformValue("ambientIntensity", m_ambientIntensity);

    // Fill directional light
    shader->setUniformValue("fillLightDir",
        QVector3D(m_fillLight.direction.x, m_fillLight.direction.y, m_fillLight.direction.z));
    shader->setUniformValue("fillLightColor",
        QVector3D(m_fillLight.color.x, m_fillLight.color.y, m_fillLight.color.z));
    shader->setUniformValue("fillLightIntensity", m_fillLight.intensity);

    // Screen spotlight
    shader->setUniformValue("spotLightPos",
        QVector3D(m_screenSpot.position.x, m_screenSpot.position.y, m_screenSpot.position.z));
    shader->setUniformValue("spotLightDir",
        QVector3D(m_screenSpot.direction.x, m_screenSpot.direction.y, m_screenSpot.direction.z));
    shader->setUniformValue("spotLightColor",
        QVector3D(m_screenSpot.color.x, m_screenSpot.color.y, m_screenSpot.color.z));
    shader->setUniformValue("spotLightIntensity", m_screenSpot.intensity);
    shader->setUniformValue("spotLightCutOff", m_screenSpot.cutOff);
    shader->setUniformValue("spotLightOuterCutOff", m_screenSpot.outerCutOff);

    int numLights = static_cast<int>(m_pointLights.size());
    shader->setUniformValue("numPointLights", numLights);
    for (int i = 0; i < numLights; i++) {
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

    for (const auto& obj : m_objects) drawObj(obj);

    // Seats (5 combined meshes)
    shader->setUniformValue("model", qm(glm::mat4(1.0f)));

    auto drawSeatPart = [&](const Mesh& mesh, const PhongMaterial& mat) {
        if (!mesh.vao) return;
        shader->setUniformValue("color",
            QVector3D(mat.color.x, mat.color.y, mat.color.z));
        shader->setUniformValue("emissive",
            QVector3D(mat.emissive.x, mat.emissive.y, mat.emissive.z));
        shader->setUniformValue("emissiveIntensity", mat.emissiveIntensity);
        shader->setUniformValue("roughness", mat.roughness);
        shader->setUniformValue("metalness", mat.metalness);
        m_gl->glBindVertexArray(mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
    };

    drawSeatPart(m_seatBaseMesh, m_seatBaseMat);
    drawSeatPart(m_seatCushionMesh, m_seatCushionMat);
    drawSeatPart(m_seatBackMesh, m_seatBackMat);
    drawSeatPart(m_seatArmMesh, m_seatArmMat);
    drawSeatPart(m_seatLegMesh, m_seatLegMat);

    // Lobby objects with group rotation around pivot (0,0,66)
    for (const auto& obj : m_lobbyObjects) {
        glm::mat4 lobbyTransform = T(lobbyPivot) * lobbyRot * T(-lobbyPivot) * obj.transform;
        shader->setUniformValue("model", qm(lobbyTransform));
        shader->setUniformValue("color",
            QVector3D(obj.material.color.x, obj.material.color.y, obj.material.color.z));
        shader->setUniformValue("emissive",
            QVector3D(obj.material.emissive.x, obj.material.emissive.y, obj.material.emissive.z));
        shader->setUniformValue("emissiveIntensity", obj.material.emissiveIntensity);
        shader->setUniformValue("roughness", obj.material.roughness);
        shader->setUniformValue("metalness", obj.material.metalness);
        m_gl->glBindVertexArray(obj.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
    }

    m_gl->glBindVertexArray(0);
    shader->release();
}

void CinemaScene::renderEmissive(QOpenGLShaderProgram* shader,
                                  const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& cameraPos)
{
    if (!shader || !shader->isLinked() || m_emissiveObjects.empty()) return;

    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    glm::vec3 lobbyPivot(0, 0, 66.0f);
    glm::mat4 lobbyRot = Ry(m_lobbyRotation);

    m_gl->glDisable(GL_BLEND);
    shader->bind();

    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("viewPos",
        QVector3D(cameraPos.x, cameraPos.y, cameraPos.z));

    // Re-set lights since the phong shader was released after renderOpaque
    shader->setUniformValue("dirLightDir",
        QVector3D(m_dirLight.position.x, m_dirLight.position.y, m_dirLight.position.z));
    shader->setUniformValue("dirLightColor",
        QVector3D(m_dirLight.color.x, m_dirLight.color.y, m_dirLight.color.z));
    shader->setUniformValue("dirLightIntensity", m_dirLight.intensity);
    shader->setUniformValue("ambientColor",
        QVector3D(m_ambientColor.x, m_ambientColor.y, m_ambientColor.z));
    shader->setUniformValue("ambientIntensity", m_ambientIntensity);

    // Fill directional light
    shader->setUniformValue("fillLightDir",
        QVector3D(m_fillLight.direction.x, m_fillLight.direction.y, m_fillLight.direction.z));
    shader->setUniformValue("fillLightColor",
        QVector3D(m_fillLight.color.x, m_fillLight.color.y, m_fillLight.color.z));
    shader->setUniformValue("fillLightIntensity", m_fillLight.intensity);

    // Screen spotlight
    shader->setUniformValue("spotLightPos",
        QVector3D(m_screenSpot.position.x, m_screenSpot.position.y, m_screenSpot.position.z));
    shader->setUniformValue("spotLightDir",
        QVector3D(m_screenSpot.direction.x, m_screenSpot.direction.y, m_screenSpot.direction.z));
    shader->setUniformValue("spotLightColor",
        QVector3D(m_screenSpot.color.x, m_screenSpot.color.y, m_screenSpot.color.z));
    shader->setUniformValue("spotLightIntensity", m_screenSpot.intensity);
    shader->setUniformValue("spotLightCutOff", m_screenSpot.cutOff);
    shader->setUniformValue("spotLightOuterCutOff", m_screenSpot.outerCutOff);

    int numLights = static_cast<int>(m_pointLights.size());
    shader->setUniformValue("numPointLights", numLights);
    for (int i = 0; i < numLights; i++) {
        QString p = QString("pointLightPos[%1]").arg(i);
        shader->setUniformValue(p.toUtf8().constData(),
            QVector3D(m_pointLights[i].position.x, m_pointLights[i].position.y, m_pointLights[i].position.z));
        p = QString("pointLightColor[%1]").arg(i);
        shader->setUniformValue(p.toUtf8().constData(),
            QVector3D(m_pointLights[i].color.x, m_pointLights[i].color.y, m_pointLights[i].color.z));
        p = QString("pointLightIntensity[%1]").arg(i);
        shader->setUniformValue(p.toUtf8().constData(), m_pointLights[i].intensity);
    }

    m_gl->glEnable(GL_BLEND);
    m_gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& obj : m_emissiveObjects) {
        auto& mat = obj.material;
        shader->setUniformValue("model", qm(obj.transform));
        shader->setUniformValue("color",
            QVector3D(mat.color.x, mat.color.y, mat.color.z));
        shader->setUniformValue("emissive",
            QVector3D(mat.emissive.x, mat.emissive.y, mat.emissive.z));
        shader->setUniformValue("emissiveIntensity", mat.emissiveIntensity);
        shader->setUniformValue("roughness", mat.roughness);
        shader->setUniformValue("metalness", mat.metalness);
        m_gl->glBindVertexArray(obj.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
    }

    // Lobby emissive objects with group rotation
    for (const auto& obj : m_lobbyEmissiveObjects) {
        auto& mat = obj.material;
        glm::mat4 lobbyTransform = T(lobbyPivot) * lobbyRot * T(-lobbyPivot) * obj.transform;
        shader->setUniformValue("model", qm(lobbyTransform));
        shader->setUniformValue("color",
            QVector3D(mat.color.x, mat.color.y, mat.color.z));
        shader->setUniformValue("emissive",
            QVector3D(mat.emissive.x, mat.emissive.y, mat.emissive.z));
        shader->setUniformValue("emissiveIntensity", mat.emissiveIntensity);
        shader->setUniformValue("roughness", mat.roughness);
        shader->setUniformValue("metalness", mat.metalness);
        m_gl->glBindVertexArray(obj.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
    }

    // Aisle strips (blue emissive glow between seat rows)
    for (const auto& strip : m_aisleStrips) {
        auto& mat = strip.material;
        shader->setUniformValue("model", qm(strip.transform));
        shader->setUniformValue("color",
            QVector3D(mat.color.x, mat.color.y, mat.color.z));
        shader->setUniformValue("emissive",
            QVector3D(mat.emissive.x, mat.emissive.y, mat.emissive.z));
        shader->setUniformValue("emissiveIntensity", mat.emissiveIntensity);
        shader->setUniformValue("roughness", mat.roughness);
        shader->setUniformValue("metalness", mat.metalness);
        m_gl->glBindVertexArray(strip.mesh.vao);
        m_gl->glDrawArrays(GL_TRIANGLES, 0, strip.mesh.vertexCount);
    }

    m_gl->glBindVertexArray(0);
    m_gl->glDisable(GL_BLEND);
    shader->release();
}

void CinemaScene::renderParticles_(QOpenGLShaderProgram* shader,
                                    const glm::mat4& view, const glm::mat4& proj)
{
    if (!shader || !shader->isLinked() || !m_particleMesh.vao) return;

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
    // Particle group rotation (matches preview.html: particles.rotation.y and .x)
    glm::mat4 particleModel = Ry(m_particleRotationY) * Rx(m_particleRotationX);
    shader->setUniformValue("model", qm(particleModel));
    shader->setUniformValue("view", qm(view));
    shader->setUniformValue("projection", qm(proj));
    shader->setUniformValue("pointSize", 2.5f);
    shader->setUniformValue("particleAlpha", 0.72f);
    shader->setUniformValue("petalMode", 0);

    m_gl->glBindVertexArray(m_particleMesh.vao);
    m_gl->glDrawArrays(GL_POINTS, 0, m_particleMesh.vertexCount);
    m_gl->glBindVertexArray(0);

    shader->release();
    m_gl->glDepthMask(GL_TRUE);
    m_gl->glDisable(GL_BLEND);
}

std::vector<Mesh> CinemaScene::shadowOnlyMeshes() const {
    return {m_seatBaseMesh, m_seatCushionMesh, m_seatBackMesh, m_seatArmMesh, m_seatLegMesh};
}

std::vector<ScreenModel*> CinemaScene::screens() {
    if (m_screen) return {m_screen.get()};
    return {};
}
