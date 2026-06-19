#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H

#include <vulkan/vulkan.h>

#include <algorithm>
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

namespace Optikos
{
int constexpr DEFAULT_SHADER = 0;

constexpr const char* APP_NAME    = "Optikos";
constexpr const char* ENGINE_NAME = "No Engine";

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
    void createGraphicsPipeline();

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
};

}  // namespace Optikos

#endif /* VULKANRENDERER_H */