#include "UUID.hpp"

#include <random>

static std::random_device                      s_RandomDevice;
static std::mt19937_64                         eng(s_RandomDevice());
static std::uniform_int_distribution<uint64_t> s_UniformDistribution(1, std::numeric_limits<uint64_t>::max());

static std::mt19937                            eng32(s_RandomDevice());
static std::uniform_int_distribution<uint32_t> s_UniformDistribution32(1, std::numeric_limits<uint32_t>::max());

UUID::UUID()
	: m_Value(s_UniformDistribution(eng))
{
}

UUID::UUID(uint64_t uuid)
	: m_Value(uuid)
{
}

UUID32::UUID32()
	: m_Value(s_UniformDistribution32(eng32))
{
}

UUID32::UUID32(uint32_t uuid)
	: m_Value(uuid)
{
}
