cmake_minimum_required(VERSION 3.20)
project(Optikos LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OPTIKOS_RENDERER "OPENGL" CACHE STRING "Render backend")
set_property(CACHE OPTIKOS_RENDERER PROPERTY STRINGS "OPENGL" "VULKAN" "NONE")

set(OPTIKOS_PLATFORM "GLFW" CACHE STRING "Platform backend")
set_property(CACHE OPTIKOS_PLATFORM PROPERTY STRINGS "GLFW" "WIN32" "NONE")

set(OPTIKOS_INPUT "GLFW" CACHE STRING "Input backend")
set_property(CACHE OPTIKOS_INPUT PROPERTY STRINGS "GLFW" "WIN32" "NONE")

find_package(Freetype REQUIRED)
if(OPTIKOS_PLATFORM STREQUAL "GLFW" OR OPTIKOS_INPUT STREQUAL "GLFW")
    find_package(glfw3 CONFIG REQUIRED)
endif()
if(OPTIKOS_RENDERER STREQUAL "OPENGL")
    find_package(glad CONFIG REQUIRED)
endif()
if(OPTIKOS_RENDERER STREQUAL "VULKAN")
    find_package(Vulkan REQUIRED MODULE) 
    find_package(unofficial-shaderc CONFIG REQUIRED)
endif()

set(COMMON_SOURCES
    src/optikos.cpp
    src/utilities/logger.cpp
    src/ui/UISystem.cpp
    src/ui/sdk/container.cpp
    src/ui/sdk/label.cpp
    src/ui/sdk/button.cpp
    src/ui/sdk/textBox.cpp
    src/ui/sdk/slider.cpp
    src/render/RenderQueue.cpp
    src/ui/text/TextFont.cpp
    src/ui/sdk/scrollContainer.cpp
    src/ui/sdk/image.cpp
)

add_library(Optikos STATIC ${COMMON_SOURCES})
add_library(Optikos::Optikos ALIAS Optikos)

target_compile_definitions(Optikos PRIVATE
    OPTIKOS_DEFAULT_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/res/fonts/"
)

if(OPTIKOS_RENDERER STREQUAL "OPENGL")
    target_sources(Optikos PRIVATE 
        src/render/opengl/OpenGLRenderer.cpp
        src/shader/GLSL/GLShader.cpp
    )
    target_compile_definitions(Optikos PUBLIC OPTIKOS_BACKEND_OPENGL)
    target_compile_definitions(Optikos PRIVATE
        OPTIKOS_SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/"
    )
    target_link_libraries(Optikos PUBLIC glad::glad)
endif()

if(OPTIKOS_RENDERER STREQUAL "VULKAN")
    target_sources(Optikos PRIVATE 
        src/render/vulkan/VulkanRenderer.cpp
    )
    target_compile_definitions(Optikos PUBLIC OPTIKOS_BACKEND_VULKAN)
    target_compile_definitions(Optikos PRIVATE
        OPTIKOS_SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/"
    )

    target_link_libraries(Optikos PUBLIC Vulkan::Vulkan)
    target_link_libraries(Optikos PRIVATE unofficial::shaderc::shaderc)
endif()

if(OPTIKOS_PLATFORM STREQUAL "GLFW")
    target_sources(Optikos PRIVATE src/platform/glfw/GLFWWindow.cpp)
    target_compile_definitions(Optikos PUBLIC OPTIKOS_PLATFORM_GLWF)
    target_link_libraries(Optikos PUBLIC glfw)
endif()

if(OPTIKOS_INPUT STREQUAL "GLFW")
    target_sources(Optikos PRIVATE src/input/glfw/GLFWInputSystem.cpp)
    target_compile_definitions(Optikos PUBLIC OPTIKOS_INPUT_GLWF)
endif()

if(WIN32)
    target_link_libraries(Optikos PRIVATE dwmapi)
    target_compile_definitions(Optikos PUBLIC PLATFORM_WINDOWS)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(Optikos PUBLIC PLATFORM_LINUX)
endif()

target_include_directories(Optikos PUBLIC 
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(Optikos PUBLIC Freetype::Freetype)

function(optikos_copy_resources TARGET)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/res"
            "$<TARGET_FILE_DIR:${TARGET}>/res"
        COMMENT "Copying Optikos resources to ${TARGET}..."
    )
endfunction()

option(DEV "Enable strict developer warnings" OFF)
if(DEV)
    target_compile_options(Optikos PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Werror>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Werror>
    )
else()
    target_compile_options(Optikos PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W3>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
    )
endif()