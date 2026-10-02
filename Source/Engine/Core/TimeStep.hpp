#pragma once

class Timestep
{
public:
	Timestep() = default;
	Timestep(float time)
		: m_Time(time)
	{
	}

	inline float GetSeconds() const { return m_Time; }
	inline float GetMilliseconds() const { return m_Time * 1000.0f; }

	operator float() const { return m_Time; }

private:
	float m_Time = 0.0f;
};
