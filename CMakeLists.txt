cmake_minimum_required(VERSION 3.30)
project(vkexperiments)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
FetchContent_Declare(glfw3
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)

FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
)

FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1
)

FetchContent_Declare(stb
        GIT_REPOSITORY    https://github.com/nothings/stb.git
        GIT_TAG           master
        CONFIGURE_COMMAND ""
        BUILD_COMMAND     ""
)

FetchContent_Declare(VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        GIT_TAG        v3.1.0
)


FetchContent_MakeAvailable(glfw3 glm spdlog stb VulkanMemoryAllocator)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

add_library(stb::stb ALIAS stb)

add_executable(vkexperiments src/main.cpp
        src/vke/app.cpp
        src/vke/app.hpp
        src/vke/render_context.cpp
        src/vke/render_context.hpp
        src/vke/state_track.cpp
        src/vke/state_track.hpp
        src/vke/renderer.cpp
        src/vke/renderer.hpp
        src/vke/mesh.cpp
        src/vke/mesh.hpp
        src/vke/util.hpp)
target_include_directories(vkexperiments PRIVATE src)
target_link_libraries(vkexperiments PRIVATE glfw glm::glm spdlog::spdlog stb::stb GPUOpen::VulkanMemoryAllocator Vulkan::Vulkan Vulkan::shaderc_combined)
target_compile_definitions(vkexperiments PRIVATE GLM_FORCE_RADIANS GLM_ENABLE_EXPERIMENTAL GLFW_INCLUDE_NONE GLFW_INCLUDE_VULKAN VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1)
