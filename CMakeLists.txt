# ==== Project ====

cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)
project(Blackfrost VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# === Language ===

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === Vulkan ===
find_package(Vulkan REQUIRED COMPONENTS volk)

# === Target ===

add_executable(Blackfrost)

file(GLOB_RECURSE BLACKFROST_SOURCES CONFIGURE_DEPENDS
		Source/*.cpp
		Source/*.hpp
)

target_sources(Blackfrost PRIVATE ${BLACKFROST_SOURCES})
target_include_directories(Blackfrost PRIVATE Source/Engine Source)

# == Build configuration ==

target_compile_definitions(Blackfrost PRIVATE
		$<$<CONFIG:Debug>:BF_DEBUG>
		$<$<CONFIG:Release>:BF_RELEASE>

		NOMINMAX
		WIN32_LEAN_AND_MEAN
		VK_NO_PROTOTYPES
)

# == Compiler options ==

if (MSVC)
	target_compile_options(Blackfrost PRIVATE
			/MP
			/Gy
			/Zc:preprocessor
	)
else ()
	target_compile_options(Blackfrost PRIVATE
			-Wall
			-Wextra
			-Wpedantic
	)
endif ()

# === Dependencies ===

add_subdirectory(ThirdParty)

target_link_libraries(Blackfrost PRIVATE
		Vulkan::Headers
		Vulkan::volk
		slang::slang

		spdlog::spdlog
		SDL3::SDL3-static
		glm::glm
		EnTT::EnTT
		imgui::imgui
		fastgltf::fastgltf
		stb::stb
		meshoptimizer
)

# === Precompiled header ===

target_precompile_headers(Blackfrost PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine/pch.hpp>)

set_source_files_properties(
		Source/Engine/ThirdParty/stb_impl.cpp
		Source/Engine/ThirdParty/vma_impl.cpp
		Source/Engine/ThirdParty/volk_impl.cpp
		PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)
