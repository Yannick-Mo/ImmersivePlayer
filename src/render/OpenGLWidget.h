#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLShaderProgram>
#include <QTimer>
#include <QElapsedTimer>
#include <QPoint>
#include <memory>
#include "Camera.h"

class VideoManager;
class Scene;
class ScreenModel;

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_5_Core
{
    Q_OBJECT

public:
    explicit OpenGLWidget(QWidget* parent = nullptr);
    ~OpenGLWidget();

    void setVideoDecoder(std::shared_ptr<VideoManager> manager);
    void setScene(std::unique_ptr<Scene> scene);
    Scene* scene() const { return m_scene.get(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void setupShaders();
    void setupShadowFBO();
    void renderShadowPass();
    void renderFallbackVideo();
    glm::mat4 computeLightSpaceMatrix() const;

    QOpenGLShaderProgram* m_videoShader = nullptr;
    QOpenGLShaderProgram* m_phongShader = nullptr;
    QOpenGLShaderProgram* m_particleShader = nullptr;
    QOpenGLShaderProgram* m_depthShader = nullptr;

    // Shadow FBO
    GLuint m_shadowFBO = 0;
    GLuint m_shadowMap = 0;
    static constexpr int kShadowSize = 2048;

    std::unique_ptr<Scene> m_scene;
    std::shared_ptr<VideoManager> m_decoder;

    Camera m_camera;

    glm::vec3 m_target{0.0f, 2.0f, 0.0f};
    float m_yaw = -30.0f;
    float m_pitch = 20.0f;
    float m_radius = 14.0f;
    float m_aspectRatio = 16.0f / 9.0f;

    bool m_mouseCaptured = false;
    QPoint m_lastMousePos;
    Qt::MouseButton m_activeButton = Qt::NoButton;

    QTimer* m_refreshTimer = nullptr;
    QElapsedTimer m_frameTimer;
    float m_elapsedTime = 0.0f;

    bool m_glReady = false;
    std::unique_ptr<ScreenModel> m_fallbackScreen;

    // Camera drift animation (matches preview.html)
    glm::vec3 m_driftOffset{0.0f};
    float m_driftTime = 0.0f;
};

#endif // OPENGLWIDGET_H
