#include "GLFWWindow.hpp"

#include "render/IRenderer.hpp"

namespace Optikos
{
GLFWWindow::GLFWWindow(const int w, const int h, std::string_view title, GraphicsConfig config)
    : m_window(nullptr),
      m_renderer(nullptr),
      m_inputSystem(nullptr),
      m_config(config),
      m_windowSize({w, h})
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) throw std::runtime_error("glfwInit failed");

    if (m_config.api == GraphicsAPI::OpenGL)
    {
        LOG_TRACE("Config minor: " + std::to_string(m_config.versionMinor) +
                      " Major: " + std::to_string(m_config.versionMajor),
                  "log");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, m_config.versionMajor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, m_config.versionMinor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }
    else
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_window = glfwCreateWindow(w, h, title.data(), NULL, NULL);

    if (!m_window)
    {
        glfwTerminate();
        throw std::runtime_error("window creation failed");
    }

    LOG_TRACE("Window opened", "log");

    if (m_config.api == GraphicsAPI::OpenGL)
    {
        glfwMakeContextCurrent(m_window);
    }

    glfwSetWindowUserPointer(m_window, this);
}

GLFWWindow::~GLFWWindow()
{
    m_renderer = nullptr;

    if (m_window)
    {
        if (m_config.api == GraphicsAPI::OpenGL) glfwMakeContextCurrent(nullptr);

        glfwDestroyWindow(m_window);
        glfwTerminate();
        LOG_TRACE("Window closed", "log");
    }
}

void GLFWWindow::setWindowShouldClose(bool flag)
{
    glfwSetWindowShouldClose(m_window, flag);
}

void GLFWWindow::setWindowTitleBar(Color color)
{
#ifdef PLATFORM_WINDOWS
    HWND hwnd    = glfwGetWin32Window(m_window);
    BOOL useDark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

    COLORREF captionColor = RGB(color.r, color.g, color.b);
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));

#else
    // linux
    (void) color;
#endif
}

void GLFWWindow::makeContextCurrent()
{
    glfwMakeContextCurrent(m_window);
}

std::vector<const char*> GLFWWindow::getVulkanExtensions()
{
    uint32_t     count = 0;
    const char** ext   = glfwGetRequiredInstanceExtensions(&count);

    std::vector<const char*> extensions(ext, ext + count);

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    return extensions;
}

void GLFWWindow::setRenderer(IRenderer* renderer)
{
    m_renderer = renderer;

    if (m_window && m_renderer)
    {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        m_windowSize.width  = width;
        m_windowSize.height = height;
        m_renderer->onWindowResize(width, height);

        glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);

        LOG_TRACE("[setRenderer] render inside window set", "log");
    }
}

void GLFWWindow::setInputSystem(IInputSystem* inputSystem)
{
    m_inputSystem = inputSystem;
}

IInputSystem* GLFWWindow::getInputSystem() const
{
    return m_inputSystem;
}

void* GLFWWindow::native_handle()
{
    return m_window;
}

void GLFWWindow::poll_events()
{
    // glfwWaitEvents();
    glfwPollEvents();
}

bool GLFWWindow::should_close() const
{
    return glfwWindowShouldClose(m_window);
}

void GLFWWindow::error_callback(int error, const char* description)
{
    fprintf(stderr, "Error [%d]: %s\n", error, description);
}

void GLFWWindow::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    auto* windowPtr = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));

    if (!windowPtr)
    {
        LOG_DEBUG("windowPtr not initialized", "log");
        return;
    }

    windowPtr->m_windowSize.width  = width;
    windowPtr->m_windowSize.height = height;
    if (!windowPtr->m_renderer)
    {
        LOG_DEBUG("windowPtr->m_renderer not initialized", "log");
        return;
    }
    windowPtr->m_renderer->onWindowResize(width, height);

    if (!windowPtr->m_uiSystem)
    {
        LOG_DEBUG("windowPtr->m_uiSystem not initialized", "log");
        return;
    }
    windowPtr->m_uiSystem->expandWidgets(width, height);

    windowPtr->m_renderer->beginFrame();
    windowPtr->m_uiSystem->render(windowPtr->m_renderer->getRenderQueue());
    windowPtr->m_renderer->endFrame();
    windowPtr->m_renderer->swap_buffer();
}

int GLFWWindow::getHeight() const
{
    return m_windowSize.height;
}
int GLFWWindow::getWidth() const
{
    return m_windowSize.width;
}

UISystem* GLFWWindow::getUiSystem() const
{
    return m_uiSystem;
}

void GLFWWindow::setUiSystem(UISystem* uiSystem)
{
    m_uiSystem = uiSystem;
    if (m_uiSystem)
    {
        m_uiSystem->expandWidgets(m_windowSize.width, m_windowSize.height);
    }
}

}  // namespace Optikos