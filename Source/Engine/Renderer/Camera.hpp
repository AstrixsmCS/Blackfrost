#pragma once

#include "Core/TimeStep.hpp"

#include <glm/glm.hpp>

class Camera
{
public:
	Camera() = default;
	Camera(float fov, float aspectRatio, float nearClip, float farClip);

	void OnUpdate(Timestep ts);

	// Reversed-Z infinite far plane farClip is kept for callers but not used by the projection.
	void SetPerspective(float fov, float aspectRatio, float nearClip, float farClip);

	void SetPosition(const glm::vec3& position);

	const glm::mat4& GetProjection() const { return m_Projection; }
	const glm::mat4& GetView() const { return m_View; }

	glm::mat4 GetViewProjection() const { return m_Projection * m_View; }

	const glm::vec3& GetPosition() const { return m_Position; }

	glm::vec3 GetForwardDirection() const;
	glm::vec3 GetRightDirection() const;
	glm::vec3 GetUpDirection() const;

private:
	void UpdateView();

private:
	glm::mat4 m_Projection{ 1.0f };
	glm::mat4 m_View{ 1.0f };

	glm::vec3 m_Position{ 0.0f, 0.0f, 3.0f };

	float m_Pitch = 0.0f;
	float m_Yaw = -90.0f;

	float m_MoveSpeed = 4.0f;
	float m_MouseSensitivity = 0.1f;

	bool m_FirstMouse = true;

	float m_LastMouseX = 0.0f;
	float m_LastMouseY = 0.0f;
};
