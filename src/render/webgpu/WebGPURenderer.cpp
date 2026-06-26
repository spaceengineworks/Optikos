#include "WebGPURenderer.hpp"

#include <dawn/native/DawnNative.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include <shader/WGSL/WGSLShader.hpp>
#include <utility>

#include "render/IRenderQueue.hpp"

#if defined(_WIN32)
#include <windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

namespace Optikos
{

struct OptikosVertex
{
    float position[2];    // x, y
    float color[4];       // r, g, b, a (normalized 0.0-1.0)
    float texCoord[2];    // u, v
    float fontParams[4];  // fw, tw, fh, th
};

struct RenderUniform
{
    float   uScreenSize[2];
    int32_t uHasTexture;
    int32_t padding;
};

WebGPURenderer::~WebGPURenderer() = default;

WebGPURenderer::WebGPURenderer(IWindow* window, std::unique_ptr<IShader> shader)
    : m_window(window), m_shader(std::move(shader))
{
    createInstance();
    createSurface();
    createAdapter();
    createDevice();
    createQueue();
    createBuffers();

    std::vector<unsigned char> defaultTextureData = {255, 255, 255, 255};
    loadTexture(defaultTextureData, 1, 1);
}

unsigned int WebGPURenderer::loadTexture(const std::vector<unsigned char>& data, int width,
                                         int height)
{
    if (data.empty() || width <= 0 || height <= 0) return 0;

    wgpu::TextureDescriptor textureDesc{};
    textureDesc.label     = wgpu::StringView(std::format("{} Loaded Texture", APP_NAME));
    textureDesc.dimension = wgpu::TextureDimension::e2D;
    textureDesc.size =
        wgpu::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    textureDesc.format        = wgpu::TextureFormat::RGBA8Unorm;
    textureDesc.usage         = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount   = 1;

    wgpu::Texture texture = m_device.CreateTexture(&textureDesc);

    bool                       isFont = (data.size() == static_cast<size_t>(width * height));
    std::vector<unsigned char> rgbaData;

    const unsigned char* uploadPtr  = data.data();
    size_t               uploadSize = data.size();

    if (isFont)
    {
        rgbaData.resize(width * height * 4);
        for (int i = 0; i < width * height; ++i)
        {
            unsigned char alpha = data[i];
            rgbaData[i * 4 + 0] = 255;    // R
            rgbaData[i * 4 + 1] = 255;    // G
            rgbaData[i * 4 + 2] = 255;    // B
            rgbaData[i * 4 + 3] = alpha;  // Alpha
        }
        uploadPtr  = rgbaData.data();
        uploadSize = rgbaData.size();
    }

    wgpu::TexelCopyTextureInfo texInfo{};
    texInfo.texture  = texture;
    texInfo.mipLevel = 0;
    texInfo.origin   = wgpu::Origin3D{0, 0, 0};
    texInfo.aspect   = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout texLayout{};
    texLayout.offset       = 0;
    texLayout.bytesPerRow  = static_cast<uint32_t>(width) * 4;
    texLayout.rowsPerImage = static_cast<uint32_t>(height);

    wgpu::Extent3D writeSize =
        wgpu::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    m_queue.WriteTexture(&texInfo, uploadPtr, uploadSize, &texLayout, &writeSize);

    wgpu::TextureViewDescriptor viewDesc{};
    viewDesc.label           = wgpu::StringView(std::format("{} Texture View", APP_NAME));
    viewDesc.format          = wgpu::TextureFormat::RGBA8Unorm;
    viewDesc.dimension       = wgpu::TextureViewDimension::e2D;
    viewDesc.baseMipLevel    = 0;
    viewDesc.mipLevelCount   = 1;
    viewDesc.baseArrayLayer  = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect          = wgpu::TextureAspect::All;

    wgpu::TextureView textureView = texture.CreateView(&viewDesc);

    unsigned int id = m_nextTextureId++;
    m_textures[id]  = std::move(textureView);

    return id;
}

void WebGPURenderer::flush()
{
    if (m_renderQueue.getCommands().empty() || !m_currentEncoder) return;

    auto& queue = m_renderQueue.getMutableCommands();

    std::sort(queue.begin(), queue.end(),
              [](const DrawCommand& a, const DrawCommand& b)
              {
                  if (a.textureId != b.textureId) return a.textureId < b.textureId;
                  return a.textureMode < b.textureMode;
              });

    std::vector<OptikosVertex> allVertices;
    std::vector<uint32_t>      allIndices;
    std::vector<uint8_t>       uniformCpuBuffer;

    wgpu::Limits limits;
    m_device.GetLimits(&limits);
    size_t alignment     = limits.minUniformBufferOffsetAlignment;
    size_t uniformStride = (sizeof(RenderUniform) + alignment - 1) & ~(alignment - 1);

    struct GpuBatch
    {
        uint32_t     indexCount;
        uint32_t     firstIndex;
        uint32_t     uniformOffset;
        unsigned int textureId;
    };
    std::vector<GpuBatch> batchesToRender;

    uint32_t indexOffset        = 0;
    uint32_t currentIndexOffset = 0;

    float screenWidth  = static_cast<float>(m_window->getWidth());
    float screenHeight = static_cast<float>(m_window->getHeight());

    for (size_t i = 0; i < queue.size();)
    {
        unsigned int currentTextureId   = queue[i].textureId;
        int          currentTextureMode = queue[i].textureMode;
        uint32_t     batchIndexCount    = 0;
        uint32_t     startBatchIndex    = currentIndexOffset;

        size_t nextBatchIdx = i;
        while (nextBatchIdx < queue.size() && queue[nextBatchIdx].textureId == currentTextureId &&
               queue[nextBatchIdx].textureMode == currentTextureMode)
        {
            const auto& cmd = queue[nextBatchIdx];

            for (const auto& vertex : cmd.vertices)
            {
                OptikosVertex v{};
                v.position[0]   = vertex.x;
                v.position[1]   = vertex.y;
                v.color[0]      = std::clamp(vertex.r / 255.0f, 0.0f, 1.0f);
                v.color[1]      = std::clamp(vertex.g / 255.0f, 0.0f, 1.0f);
                v.color[2]      = std::clamp(vertex.b / 255.0f, 0.0f, 1.0f);
                v.color[3]      = std::clamp(vertex.a / 255.0f, 0.0f, 1.0f);
                v.texCoord[0]   = vertex.u;
                v.texCoord[1]   = vertex.v;
                v.fontParams[0] = vertex.fw;
                v.fontParams[1] = vertex.tw;
                v.fontParams[2] = vertex.fh;
                v.fontParams[3] = vertex.th;
                allVertices.push_back(v);
            }

            for (auto idx : cmd.indices)
            {
                allIndices.push_back(idx + indexOffset);
            }

            batchIndexCount += static_cast<uint32_t>(cmd.indices.size());
            indexOffset += static_cast<uint32_t>(cmd.vertices.size());
            currentIndexOffset += static_cast<uint32_t>(cmd.indices.size());
            nextBatchIdx++;
        }

        if (batchIndexCount > 0)
        {
            RenderUniform uniformData{};
            uniformData.uHasTexture    = (currentTextureId != 0) ? currentTextureMode : 0;
            uniformData.uScreenSize[0] = screenWidth;
            uniformData.uScreenSize[1] = screenHeight;

            size_t offsetInBytes = uniformCpuBuffer.size();
            uniformCpuBuffer.resize(offsetInBytes + uniformStride);
            std::memcpy(uniformCpuBuffer.data() + offsetInBytes, &uniformData,
                        sizeof(RenderUniform));

            batchesToRender.push_back({batchIndexCount, startBatchIndex,
                                       static_cast<uint32_t>(offsetInBytes), currentTextureId});
        }

        i = nextBatchIdx;
    }

    if (allVertices.empty()) return;

    m_queue.WriteBuffer(m_vertexBuffer, 0, allVertices.data(),
                        allVertices.size() * sizeof(OptikosVertex));
    m_queue.WriteBuffer(m_indexBuffer, 0, allIndices.data(), allIndices.size() * sizeof(uint32_t));

    if (!m_uniformBuffer || m_uniformBuffer.GetSize() < uniformCpuBuffer.size())
    {
        wgpu::BufferDescriptor desc{};
        desc.size       = uniformCpuBuffer.size();
        desc.usage      = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        m_uniformBuffer = m_device.CreateBuffer(&desc);

        m_bindGroupCache.clear();
    }
    m_queue.WriteBuffer(m_uniformBuffer, 0, uniformCpuBuffer.data(), uniformCpuBuffer.size());

    if (m_passEncoder)
    {
        m_passEncoder.SetPipeline(m_pipeline);
        m_passEncoder.SetVertexBuffer(0, m_vertexBuffer);
        m_passEncoder.SetIndexBuffer(m_indexBuffer, wgpu::IndexFormat::Uint32);

        for (const auto& batch : batchesToRender)
        {
            wgpu::TextureView currentView = nullptr;
            auto              it =
                m_textures.find(batch.textureId != 0 ? batch.textureId : m_textures.begin()->first);
            if (it != m_textures.end()) currentView = it->second;

            if (!currentView && !m_textures.empty()) currentView = m_textures.begin()->second;
            if (!currentView) continue;

            wgpu::BindGroup batchBindGroup = getOrCreateBindGroupForTexture(currentView);

            uint32_t dynamicOffset = batch.uniformOffset;
            m_passEncoder.SetBindGroup(0, batchBindGroup, 1, &dynamicOffset);

            m_passEncoder.DrawIndexed(batch.indexCount, 1, batch.firstIndex, 0, 0);
        }
    }

    m_renderQueue.clear();
}

void WebGPURenderer::beginFrame()
{
    if (m_pipeline == nullptr)
    {
        createRenderPipeline();
    }
    m_renderQueue.clear();

    wgpu::CommandEncoderDescriptor encoderDesc{};
    m_currentEncoder = m_device.CreateCommandEncoder(&encoderDesc);

    wgpu::SurfaceTexture surfaceTexture;
    m_surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView screenView = surfaceTexture.texture.CreateView();

    wgpu::RenderPassColorAttachment colorAttachment{};
    colorAttachment.view       = screenView;
    colorAttachment.loadOp     = wgpu::LoadOp::Clear;
    colorAttachment.storeOp    = wgpu::StoreOp::Store;
    colorAttachment.clearValue = wgpu::Color{0.1f, 0.1f, 0.11f, 1.0f};

    wgpu::RenderPassDescriptor renderPassDesc{};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments     = &colorAttachment;

    m_passEncoder = m_currentEncoder.BeginRenderPass(&renderPassDesc);
}

void WebGPURenderer::endFrame()
{
    flush();

    if (!m_currentEncoder) return;

    if (m_passEncoder)
    {
        m_passEncoder.End();
        m_passEncoder = nullptr;
    }

    wgpu::CommandBufferDescriptor cmdBufferDesc{};
    wgpu::CommandBuffer           commandBuffer = m_currentEncoder.Finish(&cmdBufferDesc);

    m_queue.Submit(1, &commandBuffer);
    m_currentEncoder = nullptr;
}

wgpu::BindGroup WebGPURenderer::getOrCreateBindGroupForTexture(wgpu::TextureView textureView)
{
    WGPUTextureView key = textureView.Get();

    auto it = m_bindGroupCache.find(key);
    if (it != m_bindGroupCache.end())
    {
        return it->second;
    }

    std::vector<wgpu::BindGroupEntry> entries(3);

    entries[0].binding = 0;
    entries[0].buffer  = m_uniformBuffer;
    entries[0].offset  = 0;
    entries[0].size    = sizeof(RenderUniform);

    entries[1].binding     = 1;
    entries[1].textureView = textureView;

    entries[2].binding = 2;
    entries[2].sampler = m_defaultSampler;

    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout     = m_pipeline.GetBindGroupLayout(0);
    bindGroupDesc.entryCount = entries.size();
    bindGroupDesc.entries    = entries.data();

    wgpu::BindGroup newBindGroup = m_device.CreateBindGroup(&bindGroupDesc);

    m_bindGroupCache[key] = newBindGroup;
    return newBindGroup;
}

void WebGPURenderer::swap_buffer()
{
    m_surface.Present();
}

void WebGPURenderer::submit(const DrawCommand&& command)
{
    m_renderQueue.submit(std::move(command));
}

void WebGPURenderer::resetToDefault()
{
}
void WebGPURenderer::restoreStates()
{
}

IRenderQueue& WebGPURenderer::getRenderQueue()
{
    return m_renderQueue;
}

void WebGPURenderer::createRenderPipeline()
{
    std::vector<wgpu::VertexAttribute> vertexAttributes(4);

    vertexAttributes[0].format         = wgpu::VertexFormat::Float32x2;
    vertexAttributes[0].offset         = 0;
    vertexAttributes[0].shaderLocation = 0;

    vertexAttributes[1].format         = wgpu::VertexFormat::Float32x4;
    vertexAttributes[1].offset         = 2 * sizeof(float);
    vertexAttributes[1].shaderLocation = 1;

    vertexAttributes[2].format         = wgpu::VertexFormat::Float32x2;
    vertexAttributes[2].offset         = (2 + 4) * sizeof(float);
    vertexAttributes[2].shaderLocation = 2;

    vertexAttributes[3].format         = wgpu::VertexFormat::Float32x4;
    vertexAttributes[3].offset         = (2 + 4 + 2) * sizeof(float);
    vertexAttributes[3].shaderLocation = 3;

    wgpu::VertexBufferLayout vertexBufferLayout{};
    vertexBufferLayout.arrayStride    = sizeof(OptikosVertex);
    vertexBufferLayout.stepMode       = wgpu::VertexStepMode::Vertex;
    vertexBufferLayout.attributeCount = vertexAttributes.size();
    vertexBufferLayout.attributes     = vertexAttributes.data();

    std::vector<wgpu::BindGroupLayoutEntry> layoutEntries(3);

    layoutEntries[0].binding     = 0;
    layoutEntries[0].visibility  = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    layoutEntries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    layoutEntries[0].buffer.hasDynamicOffset = true;
    layoutEntries[0].buffer.minBindingSize   = sizeof(RenderUniform);

    layoutEntries[1].binding               = 1;
    layoutEntries[1].visibility            = wgpu::ShaderStage::Fragment;
    layoutEntries[1].texture.sampleType    = wgpu::TextureSampleType::Float;
    layoutEntries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    layoutEntries[2].binding      = 2;
    layoutEntries[2].visibility   = wgpu::ShaderStage::Fragment;
    layoutEntries[2].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount                    = layoutEntries.size();
    bglDesc.entries                       = layoutEntries.data();
    wgpu::BindGroupLayout bindGroupLayout = m_device.CreateBindGroupLayout(&bglDesc);

    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.bindGroupLayoutCount     = 1;
    layoutDesc.bindGroupLayouts         = &bindGroupLayout;
    wgpu::PipelineLayout pipelineLayout = m_device.CreatePipelineLayout(&layoutDesc);

    wgpu::RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.label  = wgpu::StringView(std::format("{} Main Pipeline", APP_NAME));
    pipelineDesc.layout = pipelineLayout;

    auto wgslShader = static_cast<WGSLShader*>(m_shader.get());

    pipelineDesc.vertex.module      = wgslShader->getModule(m_defaultShader);
    pipelineDesc.vertex.entryPoint  = m_wgsl_vertex_entrypoint;
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers     = &vertexBufferLayout;

    wgpu::BlendState blend{};
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;

    wgpu::ColorTargetState colorTarget{};
    colorTarget.format    = wgpu::TextureFormat::BGRA8Unorm;
    colorTarget.blend     = &blend;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState{};
    fragmentState.module      = wgslShader->getModule(m_defaultShader);
    fragmentState.entryPoint  = m_wgsl_fragment_entrypoint;
    fragmentState.targetCount = 1;
    fragmentState.targets     = &colorTarget;

    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.primitive.topology         = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace        = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode         = wgpu::CullMode::None;

    pipelineDesc.multisample.count                  = 1;
    pipelineDesc.multisample.mask                   = 0xFFFFFFFF;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_pipeline = m_device.CreateRenderPipeline(&pipelineDesc);
}

void WebGPURenderer::onWindowResize(int width, int height)
{
    if (width == 0 || height == 0) return;

    wgpu::SurfaceConfiguration cfg{};
    cfg.device      = m_device;
    cfg.format      = wgpu::TextureFormat::BGRA8Unorm;
    cfg.width       = static_cast<uint32_t>(width);
    cfg.height      = static_cast<uint32_t>(height);
    cfg.usage       = wgpu::TextureUsage::RenderAttachment;
    cfg.presentMode = wgpu::PresentMode::Fifo;

    m_surface.Configure(&cfg);
}

void WebGPURenderer::createBuffers()
{
    wgpu::BufferDescriptor vertexBufDesc{};
    vertexBufDesc.label = wgpu::StringView(std::format("{} Vertex Buffer", APP_NAME));
    vertexBufDesc.size  = m_maxVertices * sizeof(OptikosVertex);
    vertexBufDesc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;

    m_vertexBuffer = m_device.CreateBuffer(&vertexBufDesc);

    wgpu::BufferDescriptor indexBufferDesc{};
    indexBufferDesc.label = wgpu::StringView(std::format("{} Index Buffer", APP_NAME));
    indexBufferDesc.size  = m_maxIndices * sizeof(uint32_t);
    indexBufferDesc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;

    m_indexBuffer = m_device.CreateBuffer(&indexBufferDesc);

    wgpu::BufferDescriptor uniformBufferDesc{};
    uniformBufferDesc.label = wgpu::StringView(std::format("{} Uniform Buffer", APP_NAME));
    uniformBufferDesc.size  = sizeof(RenderUniform);
    uniformBufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

    m_uniformBuffer = m_device.CreateBuffer(&uniformBufferDesc);
}

void WebGPURenderer::createInstance()
{
    dawn::native::DawnInstanceDescriptor dawnDesc{};
    dawnDesc.backendValidationLevel = dawn::native::BackendValidationLevel::Full;

    wgpu::InstanceDescriptor instanceDesc{};
    instanceDesc.nextInChain = &dawnDesc;

    m_instance = wgpu::CreateInstance(&instanceDesc);
}

void WebGPURenderer::createSurface()
{
    m_window->createWebGPUSurface(APP_NAME, m_instance, &m_surface);
}

void WebGPURenderer::createAdapter()
{
    wgpu::RequestAdapterOptions options{};
    options.powerPreference   = wgpu::PowerPreference::HighPerformance;
    options.compatibleSurface = m_surface;

    m_instance.RequestAdapter(
        &options, wgpu::CallbackMode::AllowProcessEvents,
        [](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message,
           wgpu::Adapter* userdata)
        {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                std::string msg =
                    "Can't find GPU. Status code: " + std::to_string(static_cast<int>(status));
                if (message.data && message.length > 0)
                {
                    msg.append("| Message: ");
                    msg.append(std::string_view(message.data, message.length));
                }
                LOG_ERROR(msg, "log");
                return;
            }

            *userdata = std::move(adapter);
        },
        &m_adapter);

    while (m_adapter == nullptr)
    {
        m_instance.ProcessEvents();
    }
}

void WebGPURenderer::createDevice()
{
    if (!m_adapter)
    {
        LOG_ERROR("[FATAL] createDevice() (WebGPU backend) called but m_adapter is null", "log");
        return;
    }

    std::vector<wgpu::FeatureName> requiredFeatures = {wgpu::FeatureName::Float32Filterable};

    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message)
        {
            if (message.data)
            {
                std::string msg = "[Dawn Error] Device uncaptured error: ";
                msg.append(std::string_view(message.data, message.length));
                LOG_ERROR(msg, "log");
            }
        });
    deviceDesc.requiredFeatureCount = requiredFeatures.size();
    deviceDesc.requiredFeatures     = requiredFeatures.data();
    deviceDesc.label                = wgpu::StringView(std::format("{} Main Device", APP_NAME));

    m_adapter.RequestDevice(
        &deviceDesc, wgpu::CallbackMode::AllowProcessEvents,
        [](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message,
           wgpu::Device* userdata)
        {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                std::string msg = "Can't create WebGPU Device. Status: " +
                                  std::to_string(static_cast<int>(status));
                if (message.data && message.length > 0)
                {
                    msg.append(" | Error: ");
                    msg.append(std::string_view(message.data, message.length));
                }
                LOG_ERROR(msg, "log");
                return;
            }

            *userdata = std::move(device);
        },
        &m_device);

    while (m_device == nullptr)
    {
        m_instance.ProcessEvents();
    }

    if (auto* wgslShader = static_cast<WGSLShader*>(m_shader.get()))
    {
        wgslShader->setDevice(m_device);
    }

    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.magFilter    = wgpu::FilterMode::Linear;
    samplerDesc.minFilter    = wgpu::FilterMode::Linear;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    samplerDesc.addressModeU = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeV = wgpu::AddressMode::ClampToEdge;
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    m_defaultSampler         = m_device.CreateSampler(&samplerDesc);

    m_device.SetLoggingCallback(
        [](wgpu::LoggingType type, const char* message)
        {
            LOG_ERROR("[Dawn GPU] Type: " + std::to_string(static_cast<int>(type)) +
                          "| Message: " + message,
                      "log");
        });

    if (auto* wgslShader = static_cast<WGSLShader*>(m_shader.get()))
    {
        wgslShader->setDevice(m_device);

        std::string shaderPath = std::string(OPTIKOS_SHADER_PATH) + "shader.wgsl";
        auto        sources    = wgslShader->parseShader(shaderPath);

        m_defaultShader = wgslShader->createShader(sources.vertexSource, sources.fragmentSource);

        LOG_INFO("[WebGPURenderer] Default WGSL shader compiled successfully with ID: " +
                     std::to_string(m_defaultShader),
                 "log");
    }
}

void WebGPURenderer::createQueue()
{
    m_queue = m_device.GetQueue();
}

}  // namespace Optikos