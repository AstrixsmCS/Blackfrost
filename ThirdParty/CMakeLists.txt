# ==== spdlog ====
add_subdirectory(spdlog)

# ==== SDL ====

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST_LIBRARY OFF)
set(SDL_TESTS OFF)
set(SDL_EXAMPLES OFF)
set(SDL_INSTALL OFF)
set(SDL_INSTALL_DOCS OFF)

set(SDL_AUDIO OFF)
set(SDL_CAMERA OFF)
set(SDL_SENSOR OFF)
set(SDL_POWER OFF)
set(SDL_DIALOG ON)
set(SDL_TRAY OFF)
set(SDL_GPU OFF)
set(SDL_RENDER ON)
set(SDL_OPENGL OFF)
set(SDL_OPENGLES OFF)
set(SDL_KMSDRM OFF)
set(SDL_HAPTIC OFF)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/SDL3 SYSTEM)

target_compile_definitions(SDL3-static INTERFACE SDL_MAIN_HANDLED)

# ==== GLM (header-only) ====

add_library(glm INTERFACE)
add_library(glm::glm ALIAS glm)

target_include_directories(glm SYSTEM INTERFACE ${CMAKE_CURRENT_LIST_DIR}/glm)
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_ENABLE_EXPERIMENTAL)

# ==== Slang ====
add_library(slang INTERFACE)
add_library(slang::slang ALIAS slang)

find_path(SLANG_INCLUDE_DIR slang.h HINTS "$ENV{VULKAN_SDK}/Include/slang" "$ENV{VULKAN_SDK}/include/slang" REQUIRED)
find_library(SLANG_LIB NAMES slang HINTS "$ENV{VULKAN_SDK}/Lib" "$ENV{VULKAN_SDK}/lib" REQUIRED)

target_include_directories(slang SYSTEM INTERFACE ${SLANG_INCLUDE_DIR})
target_link_libraries(slang INTERFACE ${SLANG_LIB})

# EnTT (single header)
add_library(EnTT INTERFACE)
target_include_directories(EnTT SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/entt)
add_library(EnTT::EnTT ALIAS EnTT)

# ==== fastgltf ====
set(FASTGLTF_COMPILE_AS_CPP20 ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/fastgltf SYSTEM)

# ==== ImGui (Docking branch) ====

add_library(imgui STATIC
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_sdl3.cpp
		${CMAKE_CURRENT_LIST_DIR}/imgui/backends/imgui_impl_vulkan.cpp
)

add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/imgui ${CMAKE_CURRENT_LIST_DIR}/imgui/backends)

target_compile_features(imgui PUBLIC cxx_std_17)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

target_link_libraries(imgui PUBLIC SDL3-static Vulkan::volk)

# ==== stb (Single header libraries) ====

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_LIST_DIR}/stb)
add_library(stb::stb ALIAS stb)

# ==== meshoptimizer ====
set(MESHOPT_INSTALL OFF)

add_subdirectory(meshoptimizer)
