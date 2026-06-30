#include "OpenGLWidget.h"
#include "Scene.h"
#include "ScreenModel.h"
#include "core/VideoManager.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <cmath>
#include <cstdio>

// ============================================================
// Shader Sources
// ============================================================

static const char* videoVertexSrc = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char* videoFragmentSrc = R"(
#version 450 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;
uniform float videoAspect;
uniform float screenAspect;

void main() {
    float scaleX, scaleY;
    if (videoAspect > screenAspect) {
        scaleX = 1.0;
        scaleY = screenAspect / videoAspect;
    } else {
        scaleX = videoAspect / screenAspect;
        scaleY = 1.0;
    }
    vec2 uv;
    uv.x = (TexCoord.x - 0.5) * scaleX + 0.5;
    uv.y = (TexCoord.y - 0.5) * scaleY + 0.5;
    vec2 flipped = vec2(uv.x, 1.0 - uv.y);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float y = texture(yTexture, flipped).r;
    float u = texture(uTexture, flipped).r - 0.5;
    float v = texture(vTexture, flipped).r - 0.5;
    vec3 rgb;
    rgb.r = y + 1.402 * v;
    rgb.g = y - 0.344 * u - 0.714 * v;
    rgb.b = y + 1.772 * u;
    FragColor = vec4(rgb, 1.0);
}
)";

// ---- Depth-only shader (shadow pass) ----
static const char* depthVertexSrc = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;
void main() {
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)";

static const char* depthFragmentSrc = R"(
#version 450 core
void main() {
    // No color output; depth is written automatically
}
)";

// ---- Phong shader (upgraded with shadow, fog, ACES) ----
static const char* phongVertexSrc = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec3 vFragPos;
out vec3 vNormal;
out vec4 vFragPosLightSpace;

void main() {
    vFragPos = vec3(model * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    vFragPosLightSpace = lightSpaceMatrix * vec4(vFragPos, 1.0);
    gl_Position = projection * view * vec4(vFragPos, 1.0);
}
)";

static const char* phongFragmentSrc = R"(
#version 450 core
in vec3 vFragPos;
in vec3 vNormal;
in vec4 vFragPosLightSpace;

uniform vec3 viewPos;
uniform vec3 color;
uniform vec3 emissive;
uniform float emissiveIntensity;
uniform float roughness;
uniform float metalness;

#define MAX_POINT_LIGHTS 20
uniform int numPointLights;
uniform vec3 pointLightPos[MAX_POINT_LIGHTS];
uniform vec3 pointLightColor[MAX_POINT_LIGHTS];
uniform float pointLightIntensity[MAX_POINT_LIGHTS];

uniform vec3 dirLightDir;
uniform vec3 dirLightColor;
uniform float dirLightIntensity;

uniform vec3 fillLightDir;
uniform vec3 fillLightColor;
uniform float fillLightIntensity;

uniform vec3 spotLightPos;
uniform vec3 spotLightDir;
uniform vec3 spotLightColor;
uniform float spotLightIntensity;
uniform float spotLightCutOff;
uniform float spotLightOuterCutOff;

uniform vec3 ambientColor;
uniform float ambientIntensity;

uniform sampler2D shadowMap;

uniform vec3 fogColor;
uniform float fogNear;
uniform float fogFar;

uniform float exposure;

out vec4 FragColor;

float shadowPCF(vec4 fragPosLightSpace) {
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float pcfDepth = texture(shadowMap, proj.xy + vec2(x,y) * texelSize).r;
            shadow += (proj.z - 0.002 > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

vec3 acesToneMap(vec3 x) {
    // Narkowicz ACES fit
    return (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
}

vec3 calcDirectionalLight(vec3 N, vec3 V, vec3 L, vec3 lightColor, float intensity, float shadow, float specPower) {
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    vec3 diffuse = NdotL * lightColor * intensity * (1.0 - shadow);
    vec3 specular = pow(NdotH, specPower) * lightColor * intensity * (1.0 - shadow);
    return diffuse + specular;
}

void main() {
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(viewPos - vFragPos);

    vec3 ambient = ambientColor * color * ambientIntensity;
    float specPower = 32.0 / (roughness + 0.01);
    float shadow = shadowPCF(vFragPosLightSpace);

    vec3 result = ambient;

    // Key directional light (with shadow)
    vec3 keyContrib = calcDirectionalLight(N, V, normalize(dirLightDir), dirLightColor, dirLightIntensity, shadow, specPower);
    result += keyContrib * color;
    result += keyContrib * mix(vec3(1.0), color, metalness) * (specPower / 32.0); // approximate spec

    // More accurate specular for key light
    {
        vec3 L = normalize(dirLightDir);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        result += pow(NdotH, specPower) * dirLightColor * dirLightIntensity * (1.0 - shadow) * mix(vec3(1.0), color, metalness);
        // Remove the approximate spec added above (undo and redo properly)
        // Actually just redo properly:
    }
    // Start fresh calculation
    result = ambient;
    {
        vec3 L = normalize(dirLightDir);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        vec3 diff = NdotL * dirLightColor * dirLightIntensity * (1.0 - shadow);
        vec3 spec = pow(NdotH, specPower) * dirLightColor * dirLightIntensity * (1.0 - shadow);
        result += diff * color;
        result += spec * mix(vec3(1.0), color, metalness);
    }

    // Fill directional light (no shadow)
    {
        vec3 L = normalize(fillLightDir);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        vec3 diff = NdotL * fillLightColor * fillLightIntensity;
        vec3 spec = pow(NdotH, specPower) * fillLightColor * fillLightIntensity;
        result += diff * color;
        result += spec * mix(vec3(1.0), color, metalness);
    }

    // Spotlight (screen light)
    {
        vec3 lightDir = spotLightPos - vFragPos;
        float dist = length(lightDir);
        vec3 L = normalize(lightDir);
        float theta = dot(L, normalize(-spotLightDir));
        float epsilon = spotLightCutOff - spotLightOuterCutOff;
        float spotFactor = clamp((theta - spotLightOuterCutOff) / epsilon, 0.0, 1.0);
        float attenuation = spotFactor / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        vec3 diff = NdotL * spotLightColor * spotLightIntensity * attenuation;
        vec3 spec = pow(NdotH, specPower) * spotLightColor * spotLightIntensity * attenuation;
        result += diff * color;
        result += spec * mix(vec3(1.0), color, metalness);
    }

    // Point lights
    for (int i = 0; i < numPointLights; i++) {
        vec3 lightDir = pointLightPos[i] - vFragPos;
        float dist = length(lightDir);
        float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        vec3 L = normalize(lightDir);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        vec3 diff = NdotL * pointLightColor[i] * pointLightIntensity[i] * attenuation;
        vec3 spec = pow(NdotH, specPower) * pointLightColor[i] * pointLightIntensity[i] * attenuation;
        result += diff * color;
        result += spec * mix(vec3(1.0), color, metalness);
    }

    result += emissive * emissiveIntensity;

    // Fog (linear, matches THREE.Fog)
    float fogDist = length(vFragPos - viewPos);
    float fogFactor = clamp((fogFar - fogDist) / (fogFar - fogNear), 0.0, 1.0);
    result = mix(fogColor, result, fogFactor);

    // ACES filmic tone mapping with exposure
    result = acesToneMap(result * exposure);

    FragColor = vec4(result, 1.0);
}
)";

// ---- Particle shader ----
static const char* particleVertexSrc = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float pointSize;

out vec3 vColor;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = pointSize;
    vColor = aColor;
}
)";

static const char* particleFragmentSrc = R"(
#version 450 core
in vec3 vColor;
out vec4 FragColor;

uniform float particleAlpha;
uniform int petalMode;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float r = length(coord);
    if (r > 0.5) discard;
    float alpha;
    if (petalMode == 1) {
        float theta = atan(coord.y, coord.x);
        float petalFactor = abs(cos(theta * 2.5));
        float maxR = 0.5 * (0.5 + 0.5 * petalFactor);
        if (r > maxR) discard;
        alpha = 1.0 - smoothstep(maxR * 0.6, maxR, r);
    } else {
        alpha = 1.0 - smoothstep(0.35, 0.5, r);
    }
    FragColor = vec4(vColor, alpha * particleAlpha);
}
)";

// ============================================================
// OpenGLWidget Implementation
// ============================================================

OpenGLWidget::OpenGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat format;
    format.setVersion(4, 5);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    setFormat(format);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

OpenGLWidget::~OpenGLWidget() {
    if (!context() || !isValid()) {
        m_shadowFBO = 0;
        m_shadowMap = 0;
        delete m_videoShader;
        delete m_phongShader;
        delete m_particleShader;
        delete m_depthShader;
        m_videoShader = nullptr;
        m_phongShader = nullptr;
        m_particleShader = nullptr;
        m_depthShader = nullptr;
        m_scene = nullptr;
        return;
    }
    makeCurrent();
    if (m_scene) {
        m_scene->cleanup(this);
        m_scene.reset();
    }
    if (m_fallbackScreen) {
        m_fallbackScreen->destroy(this);
        m_fallbackScreen.reset();
    }
    if (m_shadowFBO) glDeleteFramebuffers(1, &m_shadowFBO);
    if (m_shadowMap) glDeleteTextures(1, &m_shadowMap);
    delete m_videoShader;
    delete m_phongShader;
    delete m_particleShader;
    delete m_depthShader;
    doneCurrent();
}

void OpenGLWidget::setVideoDecoder(std::shared_ptr<VideoManager> manager) {
    m_decoder = manager;
}

void OpenGLWidget::setScene(std::unique_ptr<Scene> scene) {
    if (m_glReady) {
        makeCurrent();
        if (m_scene) m_scene->cleanup(this);
        m_scene = std::move(scene);
        if (m_scene) {
            m_scene->initialize(this);
            m_target = m_scene->defaultTarget();
            m_yaw = m_scene->defaultYaw();
            m_pitch = m_scene->defaultPitch();
            m_radius = m_scene->defaultRadius();
        }
        doneCurrent();
    } else {
        m_scene = std::move(scene);
    }
}

void OpenGLWidget::initializeGL() {
    initializeOpenGLFunctions();

    const char* versionStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (versionStr) {
        int major = 0, minor = 0;
        if (sscanf(versionStr, "%d.%d", &major, &minor) >= 2) {
            if (major < 4 || (major == 4 && minor < 5)) {
                qWarning() << "OpenGL version < 4.5 detected, may cause rendering issues:" << versionStr;
            }
        }
    }

    qDebug() << "OpenGL:" << reinterpret_cast<const char*>(glGetString(GL_VERSION));
    qDebug() << "GLSL:" << reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    glClearColor(0.02f, 0.027f, 0.051f, 1.0f); // fog color #05070d
    glEnable(GL_DEPTH_TEST);
    // Face culling disabled — all meshes must render both front and back faces
    // to avoid see-through objects. Culling is re-enabled only in the shadow pass.
    glDisable(GL_CULL_FACE);

    m_camera.setFov(55.0f);
    m_camera.setFarPlane(1000.0f);

    setupShaders();
    setupShadowFBO();

    m_glReady = true;

    if (m_scene) {
        m_scene->initialize(this);
        m_target = m_scene->defaultTarget();
        m_yaw = m_scene->defaultYaw();
        m_pitch = m_scene->defaultPitch();
        m_radius = m_scene->defaultRadius();
    }

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() { update(); });
    m_refreshTimer->start(24);

    m_frameTimer.start();
}

void OpenGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    if (h > 0) m_aspectRatio = static_cast<float>(w) / h;
}

void OpenGLWidget::setupShaders() {
    auto setup = [](QOpenGLShaderProgram*& prog,
                    const char* vs, const char* fs, const char* name) {
        prog = new QOpenGLShaderProgram();
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vs))
            qWarning() << name << "VS:" << prog->log();
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fs))
            qWarning() << name << "FS:" << prog->log();
        if (!prog->link())
            qWarning() << name << "link:" << prog->log();
    };

    setup(m_videoShader,    videoVertexSrc,     videoFragmentSrc,     "Video");
    setup(m_phongShader,    phongVertexSrc,     phongFragmentSrc,     "Phong");
    setup(m_particleShader, particleVertexSrc,  particleFragmentSrc,  "Particle");
    setup(m_depthShader,    depthVertexSrc,     depthFragmentSrc,     "Depth");
}

void OpenGLWidget::setupShadowFBO() {
    glGenTextures(1, &m_shadowMap);
    glBindTexture(GL_TEXTURE_2D, m_shadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 kShadowSize, kShadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // PCF comparison not needed with manual sampling

    glGenFramebuffers(1, &m_shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        qWarning() << "Shadow FBO incomplete!";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::mat4 OpenGLWidget::computeLightSpaceMatrix() const {
    // Directional light orthographic projection
    glm::vec3 lightDir = glm::normalize(glm::vec3(-24.0f, 46.0f, 72.0f));
    glm::vec3 lightPos = lightDir * 160.0f; // pull back far enough
    glm::vec3 center = m_target;

    glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(lightDir, up)) > 0.999f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);

    float halfSize = 120.0f;
    glm::mat4 lightProj = glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 1.0f, 400.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, center, up);
    return lightProj * lightView;
}

void OpenGLWidget::renderShadowPass() {
    if (!m_depthShader || !m_depthShader->isLinked()) return;
    if (!m_scene) return;

    // Save current viewport and framebuffer
    GLint savedViewport[4];
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    GLint savedFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedFBO);

    glViewport(0, 0, kShadowSize, kShadowSize);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // reduce peter-panning

    glm::mat4 lightSpace = computeLightSpaceMatrix();

    m_depthShader->bind();

    // Render all scene opaque geometry
    auto qm = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };

    // We'll set lightSpace once, model per-object
    // Scene provides a helper to iterate all opaque meshes
    const auto& objects = m_scene->opaqueObjects();
    for (const auto& obj : objects) {
        if (!obj.mesh.vao) continue;
        m_depthShader->setUniformValue("model", qm(obj.transform));
        m_depthShader->setUniformValue("lightSpaceMatrix", qm(lightSpace));
        glBindVertexArray(obj.mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, obj.mesh.vertexCount);
    }

    // Render shadow-only meshes (e.g. combined seat meshes) with identity transform
    QMatrix4x4 identityModel;
    identityModel.setToIdentity();
    m_depthShader->setUniformValue("model", identityModel);
    for (const auto& mesh : m_scene->shadowOnlyMeshes()) {
        if (!mesh.vao) continue;
        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
    }

    glBindVertexArray(0);
    m_depthShader->release();

    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, savedFBO);
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

void OpenGLWidget::paintGL() {
    float newElapsed = m_frameTimer.elapsed() / 1000.0f;
    float deltaTime = newElapsed - m_elapsedTime;
    m_elapsedTime = newElapsed;

    // Camera drift animation (uses actual delta time)
    m_driftTime += deltaTime;
    m_driftOffset.x += sin(m_driftTime * 0.18f) * 0.0032f - m_driftOffset.x * 0.0006f;
    m_driftOffset.y += cos(m_driftTime * 0.14f) * 0.00192f - m_driftOffset.y * 0.0006f;

    // Clamp drift magnitude to prevent infinite accumulation
    const float MAX_DRIFT = 3.0f;
    float driftMag = std::sqrt(m_driftOffset.x * m_driftOffset.x + m_driftOffset.y * m_driftOffset.y);
    if (driftMag > MAX_DRIFT) {
        m_driftOffset *= (MAX_DRIFT / driftMag);
    }

    m_camera.setAspectRatio(m_aspectRatio);
    m_camera.setOrbit(m_target + m_driftOffset, m_yaw, m_pitch, m_radius);

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, width() * devicePixelRatioF(), height() * devicePixelRatioF());

    if (m_scene) {
        m_scene->update(m_elapsedTime, m_decoder.get());

        // 1. Shadow pass
        renderShadowPass();

        // Restore viewport and framebuffer (shadow pass changes both)
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        glViewport(0, 0, width() * devicePixelRatioF(), height() * devicePixelRatioF());

        // 2. Main pass
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glm::mat4 view = m_camera.getViewMatrix();
        glm::mat4 proj = m_camera.getProjectionMatrix();
        glm::vec3 camPos = m_camera.getPosition();
        glm::mat4 lightSpace = computeLightSpaceMatrix();

        auto qm = [](const glm::mat4& m) {
            QMatrix4x4 q;
            const float* d = glm::value_ptr(m);
            for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
            return q;
        };

        // Bind shadow map to texture unit 5
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, m_shadowMap);

        // Set global uniforms on phong shader before scene render
        m_phongShader->bind();
        m_phongShader->setUniformValue("view", qm(view));
        m_phongShader->setUniformValue("projection", qm(proj));
        m_phongShader->setUniformValue("lightSpaceMatrix", qm(lightSpace));
        m_phongShader->setUniformValue("viewPos",
            QVector3D(camPos.x, camPos.y, camPos.z));
        m_phongShader->setUniformValue("shadowMap", 5);
        m_phongShader->setUniformValue("fogColor",
            QVector3D(0.02f, 0.027f, 0.051f));
        m_phongShader->setUniformValue("fogNear", 60.0f);
        m_phongShader->setUniformValue("fogFar", 260.0f);
        m_phongShader->setUniformValue("exposure", 1.25f);
        m_phongShader->release();

        // Set screenAspect uniform on video shader
        m_videoShader->bind();
        m_videoShader->setUniformValue("screenAspect", m_aspectRatio);
        m_videoShader->release();

        // Scene handles its own lights, per-object materials, and render passes
        m_scene->render(this, m_phongShader, m_videoShader, m_particleShader,
                        view, proj, camPos);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, 0);
    } else if (m_decoder && m_decoder->isRunning()) {
        renderFallbackVideo();
    }
}

void OpenGLWidget::renderFallbackVideo() {
    if (!m_fallbackScreen) {
        m_fallbackScreen = std::make_unique<ScreenModel>(2.0f, 2.0f);
        m_fallbackScreen->initialize(this);
    }
    m_fallbackScreen->updateFromDecoder(m_decoder.get());
    glDisable(GL_DEPTH_TEST);

    // Set screenAspect uniform from actual window aspect ratio
    m_videoShader->bind();
    m_videoShader->setUniformValue("screenAspect", m_aspectRatio);
    m_videoShader->release();

    glm::mat4 identity(1.0f);
    m_fallbackScreen->setTransform(identity);
    m_fallbackScreen->render(this, m_videoShader, identity, identity);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLWidget::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
    m_activeButton = event->button();
    m_mouseCaptured = true;
    event->accept();
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_mouseCaptured) return;

    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_activeButton == Qt::LeftButton) {
        m_yaw += delta.x() * 0.3f;
        m_pitch -= delta.y() * 0.3f;
        float maxP = m_scene ? m_scene->maxPitch() : 85.0f;
        float minP = m_scene ? m_scene->minPitch() : -80.0f;
        m_pitch = std::max(minP, std::min(maxP, m_pitch));
    } else if (m_activeButton == Qt::RightButton) {
        glm::mat4 view = m_camera.getViewMatrix();
        glm::vec3 right(view[0][0], view[0][1], view[0][2]);
        glm::vec3 up(view[1][0], view[1][1], view[1][2]);
        float panSpeed = m_radius * 0.001f;
        m_target -= right * (delta.x() * panSpeed);
        m_target += up * (delta.y() * panSpeed);
    }

    event->accept();
    update();
}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == m_activeButton) {
        m_mouseCaptured = false;
        m_activeButton = Qt::NoButton;
    }
    event->accept();
}

void OpenGLWidget::wheelEvent(QWheelEvent* event) {
    float zoom = event->angleDelta().y() * 0.01f;
    m_radius -= zoom;
    float minR = m_scene ? m_scene->minRadius() : 3.0f;
    float maxR = m_scene ? m_scene->maxRadius() : 200.0f;
    m_radius = std::max(minR, std::min(maxR, m_radius));
    event->accept();
    update();
}
