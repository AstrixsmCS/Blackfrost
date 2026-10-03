#include "Camera.hpp"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

Camera::Camera(float fov, float aspectRatio, float nearClip, float farClip)
{
	SetPerspective(fov, aspectRatio, nearClip, farClip);

	UpdateView();
}

void Camera::OnUpdate(Timestep ts)
{
	const bool* keyboard = SDL_GetKeyboardState(nullptr);

	const float velocity = m_MoveSpeed * ts;

	const glm::vec3 forward = GetForwardDirection();

	const glm::vec3 right = GetRightDirection();

	if (keyboard[SDL_SCANCODE_W])
		m_Position += forward * velocity;

	if (keyboard[SDL_SCANCODE_S])
		m_Position -= forward * velocity;

	if (keyboard[SDL_SCANCODE_A])
		m_Position -= right * velocity;

	if (keyboard[SDL_SCANCODE_D])
		m_Position += right * velocity;

	if (keyboard[SDL_SCANCODE_SPACE])
		m_Position.y += velocity;

	if (keyboard[SDL_SCANCODE_LCTRL])
		m_Position.y -= velocity;

	float mouseX = 0.0f;
	float mouseY = 0.0f;

	SDL_GetMouseState(&mouseX, &mouseY);

	if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))
	{
		if (m_FirstMouse)
		{
			m_LastMouseX = mouseX;
			m_LastMouseY = mouseY;

			m_FirstMouse = false;
		}

		const float deltaX = mouseX - m_LastMouseX;
		const float deltaY = m_LastMouseY - mouseY;

		m_LastMouseX = mouseX;
		m_LastMouseY = mouseY;

		m_Yaw += deltaX * m_MouseSensitivity;
		m_Pitch += deltaY * m_MouseSensitivity;

		m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);
	}
	else
	{
		m_FirstMouse = true;
	}

	UpdateView();
}

void Camera::SetPerspective(float fov, float aspectRatio, float nearClip, float /*farClip*/)
{
	const float focalLength = 1.0f / glm::tan(fov * 0.5f);

	m_Projection = glm::mat4(0.0f);
	m_Projection[0][0] = focalLength / aspectRatio;
	m_Projection[1][1] = focalLength;
	m_Projection[2][3] = -1.0f;    // w_clip = -z_view
	m_Projection[3][2] = nearClip; // z_clip = near
}

void Camera::SetPosition(const glm::vec3& position)
{
	m_Position = position;

	UpdateView();
}

glm::vec3 Camera::GetForwardDirection() const
{
	glm::vec3 direction
	{
		glm::cos(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch)),
		glm::sin(glm::radians(m_Pitch)),
		glm::sin(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch))
	};

	return glm::normalize(direction);
}

glm::vec3 Camera::GetRightDirection() const
{
	return glm::normalize(glm::cross(GetForwardDirection(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::GetUpDirection() const
{
	return glm::normalize(glm::cross(GetRightDirection(), GetForwardDirection()));
}

void Camera::UpdateView()
{
	m_View = glm::lookAt(m_Position, m_Position + GetForwardDirection(), glm::vec3(0.0f, 1.0f, 0.0f));
}
