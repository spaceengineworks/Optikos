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

#include "platform/IWindow.hpp"
#include "render/IRenderer.hpp"
#include "render/RenderQueue.hpp"
#include "shader/IShader.hpp"
#include "utilities/logger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

namespace Optikos
{
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

    void resetToDefault() override;
    void restoreStates() override;

    IRenderQueue& getRenderQueue() override;

   private:
    struct UniformBufferObject
    {
        std::vector<float> position;
    };
    
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

        VkBuffer       m_vertexBuffer       = VK_NULL_HANDLE;
        VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;

        VkBuffer       m_indexBuffer       = VK_NULL_HANDLE;
        VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

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

        static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
            attributeDescriptions[0].binding  = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset   = POSITION_POS;

            attributeDescriptions[1].binding  = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format   = VK_FORMAT_R8G8B8A8_UNORM;
            attributeDescriptions[1].offset   = COLOR_POS;

            return attributeDescriptions;
        }
    };

    Batch m_currentBatch;

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

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence>     m_inFlightFences;

    bool m_framebufferResized = false;
    int  m_width = 0, m_height = 0;

    uint32_t m_currentFrame = 0;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;

    std::unordered_map<std::string, unsigned int> m_shaderCache;
    unsigned int                                  m_defaultShader = DEFAULT_SHADER;

    std::unordered_map<std::string, unsigned int> m_textureCache;

    bool m_uiStateSet = false;

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
    void createVertexBuffer();
    void createIndexBuffer();

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

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void drawFrame();
};

}  // namespace Optikos

#endif /* VULKANRENDERER_H */