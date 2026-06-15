#include "VulkanRenderer.hpp"

namespace Optikos
{

VulkanRenderer::VulkanRenderer(IWindow* window, std::unique_ptr<IShader> shader)
    : m_window(window), m_shader(std::move(shader))
{
    // 1. Заполняем визитную карточку приложения
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Optikos";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    LOG_INFO("Vulkan version: " + std::to_string(VK_API_VERSION_1_3), "log");

    auto extensions = m_window->getVulkanExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = 0;

/* TODO: add check with vkEnumerateInstanceLayerProperties if there is VK_LAYER_KHRONOS_validation
 */
#ifdef ENABLE_VULKAN_DEBUG_LAYER
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount   = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);

    if (result != VK_SUCCESS)
    {
        LOG_ERROR(
            "[VulkanRenderer] vkCreateInstance failed! Result code: " + std::to_string(result),
            "log");
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    LOG_INFO("Available Vulkan extensions on this system:", "log");
    for (const auto& extension : availableExtensions) LOG_INFO(extension.extensionName, "log");
}

VulkanRenderer::~VulkanRenderer()
{
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::onWindowResize(int width, int height)
{
    (void) width;
    (void) height;
    /* stub */
}

void VulkanRenderer::beginFrame()
{
    /* stub */
}

void VulkanRenderer::endFrame()
{
    /* stub */
}

void VulkanRenderer::submit(const DrawCommand&& command)
{
    (void) command;
    /* stub */
}

void VulkanRenderer::flush()
{
    /* stub */
}

void VulkanRenderer::swap_buffer()
{
    /* stub */
}

unsigned int VulkanRenderer::loadTexture(const std::vector<unsigned char>& data, int width,
                                         int height)
{
    (void) data;
    (void) width;
    (void) height;

    /* stub */
    return 0;
}

void VulkanRenderer::resetToDefault()
{
    /* stub */
}

void VulkanRenderer::restoreStates()
{
    /* stub */
}

IRenderQueue& VulkanRenderer::getRenderQueue()
{
    return m_renderQueue;
}

}  // namespace Optikos