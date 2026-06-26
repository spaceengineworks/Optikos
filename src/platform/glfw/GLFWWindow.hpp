#ifndef GLFWWINDOW_H
#define GLFWWINDOW_H

#include <iostream>

#include "platform/IWindow.hpp"
#include "utilities/logger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef PLATFORM_WINDOWS
#include <dwmapi.h>
#include <windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
// linux includes

#endif

namespace Optikos
{
class GLFWWindow : public IWindow
{
   public:
    explicit GLFWWindow(const int w, const int h, std::string_view title, GraphicsConfig config);
    ~GLFWWindow() override;

    void setWindowShouldClose(bool flag) override;
    void setWindowTitleBar(Color color) override;
    void setRenderer(IRenderer* renderer) override;

    void          setInputSystem(IInputSystem* inputSystem) override;
    IInputSystem* getInputSystem() const override;

    UISystem* getUiSystem() const override;
    void      setUiSystem(UISystem* uiSystem) override;

    /* used only for vulkan */
    std::vector<const char*> getVulkanExtensions() override;
#ifdef OPTIKOS_BACKEND_VULKAN
    void createVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) override;
#endif

#ifdef OPTIKOS_BACKEND_WEBGPU
    virtual void createWebGPUSurface(const char* label, wgpu::Instance instance, wgpu::Surface* surface) override;
#endif

    void* native_handle() override;
    void  poll_events() override;
    void  wait_events() override;
    bool  should_close() const override;
    int   getHeight() const override;
    int   getWidth() const override;
    
#ifdef __linux__
    bool isX11Active() const override;
#endif

    void makeContextCurrent();

   private:
    GLFWwindow*    m_window      = nullptr;
    IRenderer*     m_renderer    = nullptr;
    IInputSystem*  m_inputSystem = nullptr;
    UISystem*      m_uiSystem    = nullptr;
    Window         m_windowSize;
    GraphicsConfig m_config;

    std::function<void()> m_resizeCallback;

    static void error_callback(int error, const char* description);
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
};

}  // namespace Optikos

#endif /* GLFWWINDOW_H */