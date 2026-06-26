#ifndef IWINDOW_H
#define IWINDOW_H

#define GL_DEFAULT_MAJOR_LVL 6
#define GL_DEFAULT_MINOR_LVL 4

#include <functional>

#include "ui/UISystem.hpp"
#include "utilities/color.hpp"

#ifdef OPTIKOS_BACKEND_VULKAN
#include <vulkan/vulkan.h>
#endif

#ifdef OPTIKOS_BACKEND_WEBGPU
#include <webgpu/webgpu_cpp.h>
#endif

namespace Optikos
{
class IRenderer;
class IInputSystem;

enum class GraphicsAPI
{
    None,
    OpenGL,
    Vulkan,
    WebGPU,
    // DirectX12
};

struct GraphicsConfig
{
    GraphicsAPI api          = GraphicsAPI::OpenGL;
    int         versionMajor = GL_DEFAULT_MAJOR_LVL;
    int         versionMinor = GL_DEFAULT_MINOR_LVL;
};

struct Window
{
    int height;
    int width;
};

class IWindow
{
   public:
    virtual ~IWindow()                                              = default;
    virtual void          setWindowShouldClose(bool flag)           = 0;
    virtual void          setWindowTitleBar(Color color)            = 0;
    virtual void          makeContextCurrent()                      = 0;
    virtual void          setRenderer(IRenderer* renderer)          = 0;
    virtual void          setInputSystem(IInputSystem* inputSystem) = 0;
    virtual IInputSystem* getInputSystem() const                    = 0;
    virtual void          setUiSystem(UISystem* uiSystem)           = 0;
    virtual UISystem*     getUiSystem() const                       = 0;

    /* used only for vulkan */
    virtual std::vector<const char*> getVulkanExtensions() = 0;
#ifdef OPTIKOS_BACKEND_VULKAN
    virtual void createVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) = 0;
#else
    [[deprecated("Vulkan is not supported on this platform")]]
    virtual void createVulkanSurface(void* instance, void* surface)
    {
        // This is a blank fallback so the compiler stays happy
    }
#endif

#ifdef OPTIKOS_BACKEND_WEBGPU
    virtual void createWebGPUSurface(const char* label, wgpu::Instance instance, wgpu::Surface* surface) = 0;
#else
    [[deprecated("WebGPU is not supported on this platform")]]
    virtual void createWebGPUSurface(void* instance, void* surface)
    {
        // This is a blank fallback so the compiler stays happy
    }
#endif

    virtual void* native_handle()      = 0;
    virtual void  poll_events()        = 0;
    virtual void  wait_events()        = 0;
    virtual bool  should_close() const = 0;
    virtual int   getHeight() const    = 0;
    virtual int   getWidth() const     = 0;

#ifdef __linux__
    virtual bool isX11Active() const = 0;
#endif


   private:
};

}  // namespace Optikos

#endif /* IWINDOW_H */