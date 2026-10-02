#pragma once

#include "Base.hpp"

#define BF_VERSION "v0.1.0a"

// ==== Build Configuration ====

#ifdef BF_DEBUG
#define BF_BUILD_CONFIG_NAME "Debug"
#elifdef BF_RELEASE
#define BF_BUILD_CONFIG_NAME "Release"
#else
#define BF_BUILD_CONFIG_NAME "Unknown"
#endif

// ==== Build Platform ====

#ifdef BF_PLATFORM_WINDOWS
#define BF_BUILD_PLATFORM_NAME "Windows x64"
#elif defined(BF_PLATFORM_LINUX)
#define BF_BUILD_PLATFORM_NAME "Linux"
#else
#define BF_BUILD_PLATFORM_NAME "Unknown"
#endif

#define BF_VERSION_LONG "Blackfrost " BF_VERSION " (" BF_BUILD_PLATFORM_NAME " " BF_BUILD_CONFIG_NAME ")"

// Stable build version (YEAR.SEASON.MAJOR.MINOR) Season is 1(Winter), 2(Spring), 3(Summer), 4(Fall)
