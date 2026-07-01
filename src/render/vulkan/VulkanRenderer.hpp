#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "VulkanConfig.hpp"
#include "platform/IWindow.hpp"
#include "render/IRenderer.hpp"
#include "render/RenderQueue.hpp"
#include "shader/IShader.hpp"
#include "utilities/logger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Optikos
{
int constexpr SIZE_OF_ATTRIBUTES = 4;

enum
{
    POSITION_IN_ATTRIBUTES = 0,
    COLOR_IN_ATTRIBUTES    = 1,
    UV_IN_ATTRIBUTES       = 2,
    WH_IN_ATTRIBUTES       = 3
};

int constexpr DEFAULT_SHADER       = 0;
int constexpr NO_TEXTURE           = 0;
int constexpr DEFAULT_TEXTURE_UNIT = 0;

int constexpr POSITION_SIZE = 2;
int constexpr COLOR_SIZE    = 4;
int constexpr UV_SIZE       = 2;
int constexpr WH_SIZE       = 4;
int constexpr VERTEX_SIZE   = sizeof(Vertex);

size_t constexpr POSITION_POS = offsetof(Vertex, x);
size_t constexpr COLOR_POS    = offsetof(Vertex, r);
size_t constexpr UV_POS       = offsetof(Vertex, u);
size_t constexpr WH_POS       = offsetof(Vertex, fw);

constexpr const char* APP_NAME    = "Optikos";
constexpr const char* ENGINE_NAME = "No Engine";

constexpr const int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanRenderer : public IRenderer
{
   public:
    explicit VulkanRenderer(IWindow* window, std::unique_ptr<IShader> shader);
    ~VulkanRenderer() override;

    void         onWindowResize(int width, int height) override;
    void         beginFrame() override;
    void         endFrame() override;
    void         submit(DrawCommand&& command) override;
    void         flush() override;
    void         swap_buffer() override;
    unsigned int loadTexture(const std::vector<unsigned char>& data, int width,
                             int height) override;
    void         waitIdle()
    {
        vkDeviceWaitIdle(m_device);
    }
    bool setUpConfig(void* outConfig) override
    {
        if (!outConfig) return false;

        auto& config = *static_cast<SharedVulkanConfig*>(outConfig);

        config.instance         = &m_instance;
        config.physicalDevice   = &Selected().m_physDevice;
        config.device           = &m_device;
        config.surface          = &m_surface;
        config.graphicsQueue    = &m_graphicsQueue;
        config.presentQueue     = &m_presentQueue;
        config.swapChain        = &m_swapChain;
        config.swapChainFormat  = &m_swapChainImageFormat;
        config.swapChainExtent  = &m_swapChainExtent;
        config.renderPass       = &m_renderPass;
        config.pipelineLayout   = &m_pipelineLayout;
        config.graphicsPipeline = &m_graphicsPipeline;
        config.commandPool      = &m_commandPool;

        return config.isValid();
    }

    void resetToDefault() override;
    void restoreStates() override;

    IRenderQueue& getRenderQueue() override;

   private:
    struct Batch
    {
        // unsigned int              VAO         = 0;
        // unsigned int              VBO         = 0;
        // unsigned int              IBO         = 0;
        // unsigned int              shaderId    = 0;
        // unsigned int              textureId   = 0;
        // int                       textureMode = 0;
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;

        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT>       m_vertexBuffer       = {VK_NULL_HANDLE};
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> m_vertexBufferMemory = {VK_NULL_HANDLE};
        std::array<void*, MAX_FRAMES_IN_FLIGHT>          m_vertexMapped       = {nullptr};
        std::array<VkDeviceSize, MAX_FRAMES_IN_FLIGHT>   m_vertexCapacity     = {0};

        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT>       m_indexBuffer       = {VK_NULL_HANDLE};
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> m_indexBufferMemory = {VK_NULL_HANDLE};
        std::array<void*, MAX_FRAMES_IN_FLIGHT>          m_indexMapped       = {nullptr};
        std::array<VkDeviceSize, MAX_FRAMES_IN_FLIGHT>   m_indexCapacity     = {0};

        void clear()
        {
            vertices.clear();
            indices.clear();
        }

        static VkVertexInputBindingDescription getBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding   = 0;
            bindingDescription.stride    = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, SIZE_OF_ATTRIBUTES>
        getAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, SIZE_OF_ATTRIBUTES>
                attributeDescriptions{};
            attributeDescriptions[POSITION_IN_ATTRIBUTES].binding  = 0;
            attributeDescriptions[POSITION_IN_ATTRIBUTES].location = POSITION_IN_ATTRIBUTES;
            attributeDescriptions[POSITION_IN_ATTRIBUTES].format   = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[POSITION_IN_ATTRIBUTES].offset   = POSITION_POS;

            attributeDescriptions[COLOR_IN_ATTRIBUTES].binding  = 0;
            attributeDescriptions[COLOR_IN_ATTRIBUTES].location = COLOR_IN_ATTRIBUTES;
            attributeDescriptions[COLOR_IN_ATTRIBUTES].format   = VK_FORMAT_R8G8B8A8_UNORM;
            attributeDescriptions[COLOR_IN_ATTRIBUTES].offset   = COLOR_POS;

            attributeDescriptions[UV_IN_ATTRIBUTES].binding  = 0;
            attributeDescriptions[UV_IN_ATTRIBUTES].location = UV_IN_ATTRIBUTES;
            attributeDescriptions[UV_IN_ATTRIBUTES].format   = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[UV_IN_ATTRIBUTES].offset   = UV_POS;

            attributeDescriptions[WH_IN_ATTRIBUTES].binding  = 0;
            attributeDescriptions[WH_IN_ATTRIBUTES].location = WH_IN_ATTRIBUTES;
            attributeDescriptions[WH_IN_ATTRIBUTES].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[WH_IN_ATTRIBUTES].offset   = WH_POS;

            return attributeDescriptions;
        }
    };

    struct Texture
    {
        VkImage         image         = VK_NULL_HANDLE;
        VkDeviceMemory  imageMemory   = VK_NULL_HANDLE;
        VkImageView     imageView     = VK_NULL_HANDLE;
        VkSampler       sampler       = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        uint32_t        width = 0, height = 0;
        bool            isRenderTarget = false;
    };

    // TODO: can be padding
    // https://docs.vulkan.org/guide/latest/push_constants.html#:~:text=The%20following%20diagram%20provides%20a%20visualization%20of%20how%20push%20constant%20offsets%20work.
    struct PushConstants
    {
        float screenWidth;
        float screenHeight;
        int   hasTexture;
    };
    struct PendingDeletion
    {
        VkBuffer       buffer;
        VkDeviceMemory memory;
        uint64_t       frameIndex;
    };

    std::vector<PendingDeletion> m_pendingDeletes;

    Batch m_currentBatch;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct PhysicalDevice
    {
        VkPhysicalDevice                     m_physDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties           m_devProps{};
        VkPhysicalDeviceFeatures             m_devFeatures{};
        std::vector<VkQueueFamilyProperties> m_qFamilyProps;
        std::vector<VkBool32>                m_qSupportsPresent;
        std::vector<VkExtensionProperties>   m_supportedExtensions;
        std::vector<VkSurfaceFormatKHR>      m_surfaceFormats;
        VkSurfaceCapabilitiesKHR             m_surfaceCaps{};
        VkPhysicalDeviceMemoryProperties     m_memProps{};
        std::vector<VkPresentModeKHR>        m_presentModes;
    };

    std::vector<PhysicalDevice> m_devices;
    int                         m_selectedDeviceIndex = -1;

    const PhysicalDevice& Selected() const
    {
        if (m_selectedDeviceIndex == -1) throw std::runtime_error("No physical device selected!");
        return m_devices[m_selectedDeviceIndex];
    }

    IWindow*                 m_window;
    std::unique_ptr<IShader> m_shader;
    RenderQueue              m_renderQueue;

    VkInstance               m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_surface        = VK_NULL_HANDLE;

    VkDevice m_device        = VK_NULL_HANDLE;
    VkQueue  m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue  m_presentQueue  = VK_NULL_HANDLE;

    VkSwapchainKHR           m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage>     m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;
    VkFormat                 m_swapChainImageFormat;
    VkExtent2D               m_swapChainExtent;

    VkRenderPass     m_renderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout;

    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    VkCommandPool                m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler        m_defaultSampler = VK_NULL_HANDLE;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;

    bool m_framebufferResized = false;
    int  m_width = 0, m_height = 0;

    uint32_t m_currentFrame = 0;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    std::unordered_map<std::string, unsigned int> m_shaderCache;
    unsigned int                                  m_defaultShader = DEFAULT_SHADER;

    std::unordered_map<unsigned int, Texture> m_textures;
    unsigned int                              m_nextTextureId = 1;

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapChain();
    void cleanupSwapChain();

    std::vector<const char*> getRequiredExtensions();
    bool                     isDeviceSuitable(const PhysicalDevice& device);
    bool                     populateDeviceDetails(PhysicalDevice& device);
    QueueFamilyIndices       findQueueFamilies(VkPhysicalDevice device);
    void                     printDeviceDetails(const PhysicalDevice& device);

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT             messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    VkResult CreateDebugUtilsMessengerEXT(VkInstance                                instance,
                                          const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks*              pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkImageView    createImageView(VkImage image, VkFormat format);
    VkSampler      getOrCreateDefaultSampler();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image,
                     VkDeviceMemory& imageMemory);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void ensureBufferCapacity(VkBuffer& buffer, VkDeviceMemory& memory, void*& mapped,
                              VkDeviceSize& capacity, VkDeviceSize requiredSize,
                              VkBufferUsageFlags usage);

    void renderBatch(Batch& batch, VkCommandBuffer commandBuffer);

    void cleanupFrame(uint64_t finishedFrame);

    VkCommandBuffer beginSingleTimeCommands();
    void            endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void            transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                                          VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    void drawFrame();
};

}  // namespace Optikos

#endif /* VULKANRENDERER_H */