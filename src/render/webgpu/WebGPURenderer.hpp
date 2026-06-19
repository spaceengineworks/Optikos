#ifndef WEBGPU_RENDERER_H
#define WEBGPU_RENDERER_H

#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "platform/IWindow.hpp"
#include "render/IRenderer.hpp"
#include "render/RenderQueue.hpp"
#include "shader/IShader.hpp"

namespace Optikos
{
int constexpr DEFAULT_WEBGPU_SHADER = 0;

constexpr const char* APP_NAME = "Optikos";

class WebGPURenderer : public IRenderer
{
public:
    explicit WebGPURenderer(IWindow* window, std::unique_ptr<IShader> shader);
    ~WebGPURenderer() override;

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
    const uint64_t m_maxVertices = 10000;
    const uint64_t m_maxIndices = 15000;
    const char* m_wgsl_vertex_entrypoint = "vs_main";
    const char* m_wgsl_fragment_entrypoint = "fs_main";

    IWindow*                 m_window;
    std::unique_ptr<IShader> m_shader;
    RenderQueue              m_renderQueue;

    wgpu::Instance m_instance = nullptr;
    wgpu::Surface  m_surface  = nullptr;
    wgpu::Adapter  m_adapter  = nullptr;
    wgpu::Device   m_device   = nullptr;
    wgpu::Queue    m_queue    = nullptr;

    wgpu::RenderPipeline m_pipeline       = nullptr;
    wgpu::CommandEncoder m_currentEncoder = nullptr;
    wgpu::RenderPassEncoder m_passEncoder = nullptr;

    wgpu::Buffer m_vertexBuffer  = nullptr;
    wgpu::Buffer m_indexBuffer   = nullptr;
    wgpu::Buffer m_uniformBuffer = nullptr;

    std::unordered_map<unsigned int, wgpu::TextureView> m_textures;
    std::unordered_map<std::string, unsigned int>       m_textureCache;
    unsigned int                                        m_nextTextureId = 1;

    wgpu::Sampler m_defaultSampler = nullptr;
    unsigned int m_defaultShader;
    std::unordered_map<WGPUTextureView, wgpu::BindGroup> m_bindGroupCache;

    void createRenderPipeline();
    void createBuffers();
    void createInstance();
    void createDevice();
    void createAdapter();
    void createSurface();
    void createQueue();
    wgpu::BindGroup getOrCreateBindGroupForTexture(wgpu::TextureView textureView);
};

}  // namespace Optikos

#endif /* WEBGPU_RENDERER_H */
