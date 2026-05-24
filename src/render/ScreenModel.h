#pragma once
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLShaderProgram>
#include <glm/glm.hpp>
#include "video/FrameQueue.h"

class VideoManager;
struct SwsContext;

class ScreenModel {
public:
    ScreenModel(float width, float height);
    ~ScreenModel();

    void initialize(QOpenGLFunctions_4_5_Core* gl);
    void destroy(QOpenGLFunctions_4_5_Core* gl);

    void updateFromDecoder(VideoManager* decoder);
    void render(QOpenGLFunctions_4_5_Core* gl, QOpenGLShaderProgram* shader,
                const glm::mat4& view, const glm::mat4& proj);

    void setTransform(const glm::mat4& transform) { m_transform = transform; }
    const glm::mat4& transform() const { return m_transform; }

    void setScreenSize(float width, float height) { m_width = width; m_height = height; }

private:
    void updateYUVTextures(QOpenGLFunctions_4_5_Core* gl, const FramePtr& frame);

    GLuint m_vao = 0, m_vbo = 0;
    int m_vertexCount = 0;
    glm::mat4 m_transform{1.0f};
    float m_width, m_height;

    GLuint m_yTexture = 0, m_uTexture = 0, m_vTexture = 0;
    int m_videoWidth = 0, m_videoHeight = 0;

    FramePtr m_pendingFrame;

    SwsContext* m_swsCtx = nullptr;
    AVFrame* m_convertedFrame = nullptr;
};
