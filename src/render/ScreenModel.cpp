#include "ScreenModel.h"
#include "core/VideoManager.h"
#include <glm/gtc/type_ptr.hpp>

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

ScreenModel::ScreenModel(float width, float height)
    : m_width(width)
    , m_height(height)
{}

ScreenModel::~ScreenModel() {
    // FFmpeg 资源可在析构函数释放（无需 GL context）
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_convertedFrame) {
        av_frame_free(&m_convertedFrame);
    }
}

void ScreenModel::initialize(QOpenGLFunctions_4_5_Core* gl) {
    float hw = m_width * 0.5f, hh = m_height * 0.5f;
    std::vector<float> verts;
    auto add = [&](float x, float y, float z, float tx, float ty) {
        verts.push_back(x); verts.push_back(y); verts.push_back(z);
        verts.push_back(tx); verts.push_back(ty);
    };
    add(-hw, -hh, 0, 0, 0);
    add( hw,  hh, 0, 1, 1);
    add( hw, -hh, 0, 1, 0);
    add(-hw, -hh, 0, 0, 0);
    add(-hw,  hh, 0, 0, 1);
    add( hw,  hh, 0, 1, 1);

    m_vertexCount = static_cast<int>(verts.size()) / 5;
    gl->glGenVertexArrays(1, &m_vao);
    gl->glGenBuffers(1, &m_vbo);
    gl->glBindVertexArray(m_vao);
    gl->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                     verts.data(), GL_STATIC_DRAW);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void*)(3 * sizeof(float)));
    gl->glEnableVertexAttribArray(1);
    gl->glBindVertexArray(0);

    auto createTex = [gl](GLuint& tex) {
        gl->glGenTextures(1, &tex);
        gl->glBindTexture(GL_TEXTURE_2D, tex);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    };
    createTex(m_yTexture);
    createTex(m_uTexture);
    createTex(m_vTexture);

    unsigned char black = 0;
    gl->glBindTexture(GL_TEXTURE_2D, m_yTexture);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &black);
    gl->glBindTexture(GL_TEXTURE_2D, m_uTexture);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &black);
    gl->glBindTexture(GL_TEXTURE_2D, m_vTexture);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &black);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
}

void ScreenModel::destroy(QOpenGLFunctions_4_5_Core* gl) {
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_convertedFrame) {
        av_frame_free(&m_convertedFrame);
    }
    if (m_vao) gl->glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) gl->glDeleteBuffers(1, &m_vbo);
    if (m_yTexture) gl->glDeleteTextures(1, &m_yTexture);
    if (m_uTexture) gl->glDeleteTextures(1, &m_uTexture);
    if (m_vTexture) gl->glDeleteTextures(1, &m_vTexture);
    m_vao = 0; m_vbo = 0;
    m_yTexture = m_uTexture = m_vTexture = 0;
    m_videoWidth = m_videoHeight = 0;
}

void ScreenModel::updateFromDecoder(VideoManager* decoder) {
    if (!decoder || !decoder->isRunning()) return;
    FramePtr frame;
    if (decoder->getCurrentFrame(frame, 0) && frame && frame->data[0]) {
        // We need the GL context to update textures, but this is called from
        // Scene::update() which runs before render(). The context should be
        // current at that point since paintGL() is called with context active.
        // Textures are updated in render() instead to ensure context is current.
        m_pendingFrame = frame;
    }
}

void ScreenModel::render(QOpenGLFunctions_4_5_Core* gl, QOpenGLShaderProgram* shader,
                          const glm::mat4& view, const glm::mat4& proj) {
    if (!m_vao || !shader || !shader->isLinked()) return;

    if (m_pendingFrame) {
        updateYUVTextures(gl, m_pendingFrame);
        m_pendingFrame.reset();
    }

    shader->bind();

    // glm is column-major, QMatrix4x4(const float*) expects row-major,
    // so we must copy element-by-element to avoid an implicit transpose.
    auto toQMatrix = [](const glm::mat4& m) {
        QMatrix4x4 q;
        const float* d = glm::value_ptr(m);
        for (int i = 0; i < 16; ++i) q.data()[i] = d[i];
        return q;
    };
    QMatrix4x4 qModel = toQMatrix(m_transform);
    QMatrix4x4 qView  = toQMatrix(view);
    QMatrix4x4 qProj  = toQMatrix(proj);

    shader->setUniformValue("model", qModel);
    shader->setUniformValue("view", qView);
    shader->setUniformValue("projection", qProj);

    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, m_yTexture);
    shader->setUniformValue("yTexture", 0);
    gl->glActiveTexture(GL_TEXTURE1);
    gl->glBindTexture(GL_TEXTURE_2D, m_uTexture);
    shader->setUniformValue("uTexture", 1);
    gl->glActiveTexture(GL_TEXTURE2);
    gl->glBindTexture(GL_TEXTURE_2D, m_vTexture);
    shader->setUniformValue("vTexture", 2);

    float va = (m_videoWidth > 0 && m_videoHeight > 0)
        ? static_cast<float>(m_videoWidth) / m_videoHeight
        : 16.0f / 9.0f;
    shader->setUniformValue("videoAspect", va);

    gl->glBindVertexArray(m_vao);
    gl->glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    gl->glBindVertexArray(0);
    shader->release();
}

void ScreenModel::updateYUVTextures(QOpenGLFunctions_4_5_Core* gl, const FramePtr& frame) {
    if (!frame || !frame->data[0]) return;
    int width = frame->width;
    int height = frame->height;
    if (width <= 0 || height <= 0) return;

    // 如果输入不是 YUV420P，使用 libswscale 转换
    const AVFrame* srcFrame = frame.get();
    bool needsConversion = (frame->format != AV_PIX_FMT_YUV420P);

    if (needsConversion) {
        // 为转换后的帧分配内存
        if (!m_convertedFrame) {
            m_convertedFrame = av_frame_alloc();
        }
        if (!m_convertedFrame) return;

        av_frame_unref(m_convertedFrame);
        m_convertedFrame->format = AV_PIX_FMT_YUV420P;
        m_convertedFrame->width = width;
        m_convertedFrame->height = height;

        if (av_frame_get_buffer(m_convertedFrame, 0) < 0) {
            return;
        }

        // 创建或重新创建 SWS 上下文
        m_swsCtx = sws_getCachedContext(m_swsCtx,
            width, height, static_cast<AVPixelFormat>(frame->format),
            width, height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (m_swsCtx) {
            // 执行转换
            sws_scale(m_swsCtx,
                frame->data, frame->linesize,
                0, height,
                m_convertedFrame->data, m_convertedFrame->linesize);
        }

        srcFrame = m_convertedFrame;
    } else {
        // 不需要转换时释放旧转换帧
        if (m_convertedFrame) {
            av_frame_unref(m_convertedFrame);
        }
    }

    bool sizeChanged = (width != m_videoWidth || height != m_videoHeight);
    if (m_videoWidth == 0 || m_videoHeight == 0) sizeChanged = true;

    GLint rowLen, align;
    gl->glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLen);
    gl->glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);

    gl->glBindTexture(GL_TEXTURE_2D, m_yTexture);
    if (sizeChanged)
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, srcFrame->linesize[0]);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, srcFrame->data[0]);

    int uvW = (width + 1) / 2, uvH = (height + 1) / 2;
    gl->glBindTexture(GL_TEXTURE_2D, m_uTexture);
    if (sizeChanged)
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, uvW, uvH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, srcFrame->linesize[1]);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uvW, uvH, GL_RED, GL_UNSIGNED_BYTE, srcFrame->data[1]);

    gl->glBindTexture(GL_TEXTURE_2D, m_vTexture);
    if (sizeChanged)
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, uvW, uvH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, srcFrame->linesize[2]);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, uvW, uvH, GL_RED, GL_UNSIGNED_BYTE, srcFrame->data[2]);

    gl->glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLen);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, align);
    gl->glBindTexture(GL_TEXTURE_2D, 0);

    if (sizeChanged) {
        m_videoWidth = width;
        m_videoHeight = height;
    }
}
