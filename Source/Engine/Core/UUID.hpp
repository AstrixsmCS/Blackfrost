#pragma once

#include <cstdint>
#include <functional>

// Randomly generated 64-bit identifier.
//
// This is not a true 128-bit UUID, but its collision probability is
// sufficiently low for Blackfrost's current asset and entity systems.
class UUID
{
public:
	UUID();
	explicit UUID(uint64_t value);

	UUID(const UUID&)            = default;
	UUID& operator=(const UUID&) = default;

	operator uint64_t() const noexcept
	{
		return m_Value;
	}

	bool operator==(const UUID&) const = default;

private:
	uint64_t m_Value = 0;
};

// Randomly generated 32-bit identifier.
//
// Use only where the smaller identifier space is specifically required.
class UUID32
{
public:
	UUID32();
	explicit UUID32(uint32_t value);

	UUID32(const UUID32&)            = default;
	UUID32& operator=(const UUID32&) = default;

	operator uint32_t() const noexcept
	{
		return m_Value;
	}

	bool operator==(const UUID32&) const = default;

private:
	uint32_t m_Value = 0;
};

namespace std
{
	template<>
	struct hash<UUID>
	{
		size_t operator()(const UUID& uuid) const noexcept
		{
			return hash<uint64_t>{}(
				static_cast<uint64_t>(uuid));
		}
	};

	template<>
	struct hash<UUID32>
	{
		size_t operator()(const UUID32& uuid) const noexcept
		{
			return hash<uint32_t>{}(
				static_cast<uint32_t>(uuid));
		}
	};
} // namespace std
