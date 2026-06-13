#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H

#include <iostream>
#include <vulkan/vulkan.hpp>

#include "platform/IWindow.hpp"
#include "render/IRenderer.hpp"
#include "render/RenderQueue.hpp"
#include "shader/IShader.hpp"
#include "utilities/logger.hpp"

namespace Optikos
{
int constexpr DEFAULT_SHADER = 0;

class VulkanRenderer : public IRenderer
{
   public:
    explicit VulkanRenderer(IWindow* window, std::unique_ptr<IShader> shader);
    ~VulkanRenderer() override;

    void         onWindowResize(int width, int height) override;
    void         beginFrame() override;
    void         endFrame() override;
    void         submit(const DrawCommand&& command) override;
    void         flush() override;
    void         swap_buffer() override;
    unsigned int loadTexture(const std::vector<unsigned char>& data, int width,
                             int height) override;

    void resetToDefault() override;
    void restoreStates() override;

    IRenderQueue& getRenderQueue() override;

   private:
    IWindow*                 m_window;
    std::unique_ptr<IShader> m_shader;
    RenderQueue              m_renderQueue;

    VkInstance               m_instance;
    VkResult                 m_result;

    std::unordered_map<std::string, unsigned int> m_shaderCache;
    unsigned int                                  m_defaultShader = DEFAULT_SHADER;

    std::unordered_map<std::string, unsigned int> m_textureCache;

    bool m_uiStateSet = false;
};

}  // namespace Optikos

#endif /* VULKANRENDERER_H */