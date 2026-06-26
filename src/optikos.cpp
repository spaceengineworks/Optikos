#include "optikos.hpp"

#ifdef OPTIKOS_PLATFORM_GLWF
#include "platform/glfw/GLFWWindow.hpp"
#endif

#ifdef OPTIKOS_BACKEND_OPENGL
#include "render/opengl/OpenGLRenderer.hpp"
#include "shader/GLSL/GLShader.hpp"
#endif

#ifdef OPTIKOS_BACKEND_VULKAN
#include "render/vulkan/VulkanRenderer.hpp"
#include "shader/SPIRV/VkShader.hpp"
#endif

#ifdef OPTIKOS_BACKEND_WEBGPU
#include "render/webgpu/WebGPURenderer.hpp"
#include "shader/WGSL/WGSLShader.hpp"
#endif

#ifdef OPTIKOS_INPUT_GLWF
#include "input/glfw/GLFWInputSystem.hpp"
#endif

#ifndef OPTIKOS_DEFAULT_FONT_PATH
#define OPTIKOS_DEFAULT_FONT_PATH "res/fonts/"
#endif

#include "ui/UISystem.hpp"

namespace Optikos
{
Optikos::Optikos(std::string_view title, unsigned int width, unsigned int height)
{
#ifdef OPTIKOS_PLATFORM_GLWF
#ifdef OPTIKOS_BACKEND_OPENGL
    m_window = std::make_unique<GLFWWindow>(width, height, title,
                                            GraphicsConfig{GraphicsAPI::OpenGL, 4, 6});
#elif defined(OPTIKOS_BACKEND_VULKAN)
    m_window = std::make_unique<GLFWWindow>(width, height, title,
                                            GraphicsConfig{GraphicsAPI::Vulkan, 1, 3});
#else
    m_window = std::make_unique<GLFWWindow>(width, height, title,
                                            GraphicsConfig{GraphicsAPI::None, -1, -1});
#endif
#endif

#ifdef OPTIKOS_BACKEND_OPENGL
    auto shader = std::make_unique<GLShader>();
    m_renderer  = std::make_unique<OpenGLRenderer>(m_window.get(), std::move(shader));
#endif

#ifdef OPTIKOS_BACKEND_WEBGPU
    auto shader = std::make_unique<WGSLShader>();
    m_renderer = std::make_unique<WebGPURenderer>(m_window.get(), std::move(shader));
#endif

#ifdef OPTIKOS_BACKEND_VULKAN
    auto shader = std::make_unique<VkShader>();
    m_renderer  = std::make_unique<VulkanRenderer>(m_window.get(), std::move(shader));
#endif

#ifdef OPTIKOS_INPUT_GLWF
    m_inputSystem = std::make_unique<GLFWInputSystem>((GLFWwindow*) m_window->native_handle());
#endif

    m_uiSystem = std::make_unique<UISystem>();

    m_window->setRenderer(m_renderer.get());
    m_window->setInputSystem(m_inputSystem.get());
    m_window->setUiSystem(m_uiSystem.get());

    pushFont(std::string(OPTIKOS_DEFAULT_FONT_PATH) + "Titillium-Light.otf");
}

bool Optikos::should_close()
{
    return m_window->should_close();
}

void Optikos::begin()
{
    m_renderer->beginFrame();
    m_uiSystem->render(m_renderer->getRenderQueue());
}

void Optikos::end()
{
    m_renderer->endFrame();
    m_renderer->swap_buffer();
    m_window->poll_events();
}

void Optikos::pushFont(std::string_view path, std::string fontName, float fontSize)
{
    auto& font = TextFont::getInstance();
    font.loadFont(path, fontName, fontSize);
    unsigned int id = m_renderer->loadTexture(
        font.getAtlasData(fontName), font.getAtlasSize(fontName), font.getAtlasSize(fontName));
    font.setAtlasTextureId(id, fontName);
}

void Optikos::setWindowTitleBar(Color color)
{
    m_window->setWindowTitleBar(color);
}

bool Optikos::removeWidget(uint32_t id)
{
    return m_uiSystem->rem_widget(id);
}

}  // namespace Optikos