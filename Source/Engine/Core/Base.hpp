#pragma once

#define BIT(x) (1u << (x))

// ==== Platform Detection ====

#if defined(_WIN64) || defined(_WIN32)
#define BF_PLATFORM_WINDOWS
#elif defined(__linux__)
#define BF_PLATFORM_LINUX
#else
#error "Unsupported platform! Blackfrost supports Windows and Linux."
#endif

// ==== Compiler Detection ====

#if defined(__clang__)
#define BF_COMPILER_CLANG
#elif defined(__GNUC__)
#define BF_COMPILER_GCC
#elif defined(_MSC_VER)
#define BF_COMPILER_MSVC
#else
#error "Unknown compiler! Blackfrost only supports MSVC, GCC, and Clang."
#endif
