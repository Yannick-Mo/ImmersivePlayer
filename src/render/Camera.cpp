#include "Camera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

Camera::Camera(
    const glm::vec3& position,
    const glm::vec3& up,
    float fov,
    float nearPlane,
    float farPlane
)
    : m_position(position)
    , m_front(0.0f, 0.0f, -1.0f)
    , m_up(up)
    , m_right(glm::normalize(glm::cross(m_front, m_up)))
    , m_worldUp(up)
    , m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_fov(fov)
    , m_aspectRatio(16.0f / 9.0f)
    , m_nearPlane(nearPlane)
    , m_farPlane(farPlane)
{
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
    return glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::moveForward(float distance)
{
    m_position += m_front * distance;
}

void Camera::moveRight(float distance)
{
    m_position += m_right * distance;
}

void Camera::moveUp(float distance)
{
    // 使用当前相机的上方向（俯仰后依然垂直于视线方向）
    m_position += m_up * distance;
}

void Camera::setOrbit(const glm::vec3& target, float yawDeg, float pitchDeg, float radius) {
    m_yaw = yawDeg;
    m_pitch = pitchDeg;
    float ry = glm::radians(yawDeg);
    float rp = glm::radians(pitchDeg);
    m_position = target + glm::vec3(
        radius * cos(rp) * sin(ry),
        radius * sin(rp),
        radius * cos(rp) * cos(ry)
    );
    m_front = glm::normalize(target - m_position);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

void Camera::rotate(float yaw, float pitch)
{
    m_yaw += yaw;
    m_pitch += pitch;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    updateVectors();
}

void Camera::setPosition(const glm::vec3& pos)
{
    m_position = pos;
}

void Camera::setFov(float fov)
{
    m_fov = fov;
}

void Camera::setAspectRatio(float ratio)
{
    m_aspectRatio = ratio;
}

void Camera::setNearPlane(float nearPlane)
{
    m_nearPlane = nearPlane;
}

void Camera::setFarPlane(float farPlane)
{
    m_farPlane = farPlane;
}

void Camera::updateVectors()
{
    glm::vec3 direction;
    direction.x = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    direction.y = sin(glm::radians(m_pitch));
    direction.z = -cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(direction);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}