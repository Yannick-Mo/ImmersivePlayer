#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
    explicit Camera(
        const glm::vec3& position = glm::vec3(0.0f, 0.0f, 3.0f),
        const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
        float fov = 45.0f,
        float nearPlane = 0.1f,
        float farPlane = 100.0f
    );

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

    void moveForward(float distance);
    void moveRight(float distance);
    void moveUp(float distance);

    void rotate(float yaw, float pitch);
    void setOrbit(const glm::vec3& target, float yawDeg, float pitchDeg, float radius);

    void setPosition(const glm::vec3& pos);
    void setFov(float fov);
    void setAspectRatio(float ratio);
    void setNearPlane(float nearPlane);
    void setFarPlane(float farPlane);

    const glm::vec3& getPosition() const { return m_position; }
    float getFov() const { return m_fov; }
    float getAspectRatio() const { return m_aspectRatio; }
    float getNearPlane() const { return m_nearPlane; }
    float getFarPlane() const { return m_farPlane; }

private:
    void updateVectors();

    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    float m_yaw;
    float m_pitch;

    float m_fov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;
};