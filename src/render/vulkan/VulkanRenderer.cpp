#include "VulkanRenderer.hpp"

namespace Optikos
{

VulkanRenderer::VulkanRenderer(IWindow* window, std::unique_ptr<IShader> shader)
    : m_window(window), m_shader(std::move(shader))
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
}

VulkanRenderer::~VulkanRenderer()
{
    for (auto framebuffer : m_swapChainFramebuffers)
    {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    if (m_graphicsPipeline) vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    if (m_renderPass) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    for (auto imageView : m_swapChainImageViews)
        if (imageView) vkDestroyImageView(m_device, imageView, nullptr);
    m_swapChainImageViews.clear();

    if (m_swapChain) vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    if (m_device) vkDestroyDevice(m_device, nullptr);
    if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_debugMessenger) DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = APP_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = ENGINE_NAME;
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    LOG_INFO("Vulkan version: " + std::to_string(VK_API_VERSION_1_3), "log");

    auto extensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    LOG_INFO("Available Vulkan layers on this system:", "log");
    for (const auto& layer : availableLayers) LOG_INFO(layer.layerName, "log");
    LOG_INFO("END", "log");

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

#ifdef ENABLE_VULKAN_DEBUG_LAYER
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount   = 1;
    createInfo.ppEnabledLayerNames = validationLayers;
    LOG_INFO("[VulkanRenderer] Validation layer ENABLED", "log");

    populateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = &debugCreateInfo;
#else
    createInfo.enabledLayerCount = 0;
    LOG_INFO("[VulkanRenderer] Validation layer DISABLED", "log");
    createInfo.pNext = nullptr;
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
    LOG_INFO("END", "log");
}

void VulkanRenderer::setupDebugMessenger()
{
#ifdef ENABLE_VULKAN_DEBUG_LAYER
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) !=
        VK_SUCCESS)
    {
        LOG_ERROR("[VulkanRenderer] CreateDebugUtilsMessengerEXT failed!", "log");
        throw std::runtime_error("failed to set up debug messenger!");
    }
#endif
}

void VulkanRenderer::createSurface()
{
    m_window->createVulkanSurface(m_instance, &m_surface);
}

bool VulkanRenderer::isDeviceSuitable(const PhysicalDevice& device)
{
    QueueFamilyIndices indices = findQueueFamilies(device.m_physDevice);

    bool swapchainExtensionSupported = false;
    for (const auto& extension : device.m_supportedExtensions)
    {
        if (std::string(extension.extensionName) == VK_KHR_SWAPCHAIN_EXTENSION_NAME)
        {
            swapchainExtensionSupported = true;
            break;
        }
    }

    bool swapChainAdequate = false;
    if (swapchainExtensionSupported)
    {
        swapChainAdequate = !device.m_surfaceFormats.empty() && !device.m_presentModes.empty();
    }

    return indices.isComplete() && swapchainExtensionSupported && swapChainAdequate;
}

void VulkanRenderer::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        LOG_ERROR("Failed to find GPUs with Vulkan support!", "log");
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> rawDevices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, rawDevices.data());

    for (const auto& rawDevice : rawDevices)
    {
        PhysicalDevice device;
        device.m_physDevice = rawDevice;
        populateDeviceDetails(device);
        m_devices.push_back(device);
    }

#ifdef ENABLE_VULKAN_DEBUG_LAYER
    LOG_INFO("=== DETAILED GPU REPORT ===", "log");
    for (const auto& device : m_devices)
    {
        printDeviceDetails(device);
    }
    LOG_INFO("===========================", "log");
#endif

    LOG_INFO("Detected GPUs on this system:", "log");
    for (size_t i = 0; i < m_devices.size(); i++)
    {
        const auto& dev = m_devices[i];

        std::string apiVersionStr =
            std::to_string(VK_API_VERSION_MAJOR(dev.m_devProps.apiVersion)) + "." +
            std::to_string(VK_API_VERSION_MINOR(dev.m_devProps.apiVersion)) + "." +
            std::to_string(VK_API_VERSION_PATCH(dev.m_devProps.apiVersion));

        std::string deviceTypeStr = "Unknown GPU";
        if (dev.m_devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            deviceTypeStr = "Discrete GPU";
        else if (dev.m_devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            deviceTypeStr = "Integrated GPU";
        else if (dev.m_devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
            deviceTypeStr = "Virtual GPU";
        else if (dev.m_devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
            deviceTypeStr = "CPU";

        LOG_INFO(" [" + std::to_string(i) + "] - Name: " + std::string(dev.m_devProps.deviceName) +
                     " [" + deviceTypeStr + "], Supported Vulkan API: " + apiVersionStr,
                 "log");

        if (m_selectedDeviceIndex == -1)
        {
            bool isDiscrete = (dev.m_devProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
            if (isDiscrete && isDeviceSuitable(dev))
            {
                m_selectedDeviceIndex = static_cast<int>(i);
                LOG_INFO("   ^-- [Selected as Primary Device]", "log");
            }
        }
    }

    if (m_selectedDeviceIndex == -1)
    {
        for (size_t i = 0; i < m_devices.size(); i++)
        {
            if (isDeviceSuitable(m_devices[i]))
            {
                m_selectedDeviceIndex = static_cast<int>(i);
                LOG_INFO("   ^-- [Discrete GPU not found. Selected Fallback Device: " +
                             std::to_string(i) + "]",
                         "log");
                break;
            }
        }
    }

    if (m_selectedDeviceIndex == -1)
    {
        LOG_ERROR("failed to find a suitable GPU!", "log");
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanRenderer::createLogicalDevice()
{
    const auto&        selectedDevice = Selected();
    QueueFamilyIndices indices        = findQueueFamilies(selectedDevice.m_physDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t>                   uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                                                indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    // TODO: device feature will be choose with separe function
    deviceFeatures.geometryShader     = VK_TRUE;
    deviceFeatures.tessellationShader = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos    = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures     = &deviceFeatures;

    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                 VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME};

#ifdef __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledLayerCount       = 0;

    VkResult deviceResult =
        vkCreateDevice(selectedDevice.m_physDevice, &deviceCreateInfo, nullptr, &m_device);
    if (deviceResult != VK_SUCCESS)
    {
        LOG_ERROR("[VulkanRenderer] vkCreateDevice failed!", "log");
        throw std::runtime_error("failed to create logical device!");
    }

    LOG_INFO("[VulkanRenderer] Logical Device successfully created.", "log");

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void VulkanRenderer::createSwapChain()
{
    const auto& selectedDevice = Selected();

    const VkSurfaceCapabilitiesKHR&        caps         = selectedDevice.m_surfaceCaps;
    const std::vector<VkSurfaceFormatKHR>& formats      = selectedDevice.m_surfaceFormats;
    const std::vector<VkPresentModeKHR>&   presentModes = selectedDevice.m_presentModes;

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR   presentMode   = chooseSwapPresentMode(presentModes);
    VkExtent2D         extent        = chooseSwapExtent(caps);

    uint32_t imageCount = caps.minImageCount + 1;

    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
    {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = m_surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    QueueFamilyIndices indices    = findQueueFamilies(selectedDevice.m_physDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;
    }

    createInfo.preTransform   = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
    {
        LOG_ERROR("[createSwapChain] Error while trying create swap chain", "log");
        throw std::runtime_error("failed to create swap chain!");
    }

    LOG_TRACE("[createSwapChain] Swap chain successfully created", "log");

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent      = extent;
}

void VulkanRenderer::createImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapChainImages[i];

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format   = m_swapChainImageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]) !=
            VK_SUCCESS)
        {
            LOG_ERROR("[createImageViews] Error while creating image view", "log");
            throw std::runtime_error("failed to create image views!");
        }
        LOG_TRACE("[createImageViews] " + std::to_string(i) + " Image created", "log");
    }
}

void VulkanRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format  = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
    {
        LOG_ERROR("[createRenderPass] failed to create render pass", "log");
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanRenderer::createGraphicsPipeline()
{
#ifndef OPTIKOS_SHADER_PATH
#define OPTIKOS_SHADER_PATH "res/shaders/"
#endif

    auto vertShaderCode =
        m_shader->readFile(std::string(OPTIKOS_SHADER_PATH) + "VKshader.vert.spv");
    auto fragShaderCode =
        m_shader->readFile(std::string(OPTIKOS_SHADER_PATH) + "VKshader.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    LOG_TRACE("[VulkanRenderer] Shader modules successfully created and staged.", "log");

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = (float) m_swapChainExtent.width;
    viewport.height   = (float) m_swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp          = 0.0f;
    rasterizer.depthBiasSlopeFactor    = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading      = 1.0f;
    multisampling.pSampleMask           = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pSetLayouts            = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) !=
        VK_SUCCESS)
    {
        LOG_ERROR("[createGraphicsPipeline] failed to create pipeline layout", "log");
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages    = shaderStages;

    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = nullptr;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;

    pipelineInfo.layout = m_pipelineLayout;

    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass    = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex  = -1;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_graphicsPipeline) != VK_SUCCESS)
    {
        LOG_ERROR("[createGraphicsPipeline] failed to create graphics pipeline", "log");
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    LOG_TRACE("[createGraphicsPipeline] Graphics pipeline successfully created", "log");

    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
}

void VulkanRenderer::createFramebuffers()
{
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i = 0; i < m_swapChainImageViews.size(); i++)
    {
        VkImageView attachments[] = {m_swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = attachments;
        framebufferInfo.width           = m_swapChainExtent.width;
        framebufferInfo.height          = m_swapChainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) !=
            VK_SUCCESS)
        {
            LOG_ERROR("[createFramebuffers] failed to create framebuffer", "log");
            throw std::runtime_error("failed to create framebuffer!");
        }
        LOG_TRACE(
            "[createFramebuffers] frame buffer " + std::to_string(i) + " successfully created",
            "log");
    }
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions()
{
    auto extensions = m_window->getVulkanExtensions();

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

#ifdef ENABLE_VULKAN_DEBUG_LAYER
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    return extensions;
}

void VulkanRenderer::onWindowResize(int width, int height)
{
    (void) width;
    (void) height;
}

void VulkanRenderer::beginFrame()
{
}
void VulkanRenderer::endFrame()
{
}
void VulkanRenderer::submit(const DrawCommand&& command)
{
    (void) command;
}
void VulkanRenderer::flush()
{
}
void VulkanRenderer::swap_buffer()
{
}

unsigned int VulkanRenderer::loadTexture(const std::vector<unsigned char>& data, int width,
                                         int height)
{
    (void) data;
    (void) width;
    (void) height;
    return 0;
}

void VulkanRenderer::resetToDefault()
{
}
void VulkanRenderer::restoreStates()
{
}

IRenderQueue& VulkanRenderer::getRenderQueue()
{
    return m_renderQueue;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    (void) messageType;
    (void) pUserData;
    std::string msg = std::string("[Validation Layer] ") + pCallbackData->pMessage;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        LOG_ERROR(msg, "log");
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        LOG_WARN(msg, "log");
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        LOG_INFO(msg, "log");
    else
        LOG_TRACE(msg, "log");

    return VK_FALSE;
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(VkInstance                   instance,
                                                   VkDebugUtilsMessengerEXT     debugMessenger,
                                                   const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) func(instance, debugMessenger, pAllocator);
}

void VulkanRenderer::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo                 = {};
    createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData       = nullptr;
}

bool VulkanRenderer::populateDeviceDetails(PhysicalDevice& device)
{
    vkGetPhysicalDeviceProperties(device.m_physDevice, &device.m_devProps);
    vkGetPhysicalDeviceFeatures(device.m_physDevice, &device.m_devFeatures);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.m_physDevice, &queueFamilyCount, nullptr);
    device.m_qFamilyProps.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device.m_physDevice, &queueFamilyCount,
                                             device.m_qFamilyProps.data());

    bool supportsPresent = false;
    device.m_qSupportsPresent.resize(queueFamilyCount);
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(device.m_physDevice, i, m_surface,
                                             &device.m_qSupportsPresent[i]);
        if (device.m_qSupportsPresent[i] == VK_TRUE)
        {
            supportsPresent = true;
        }
    }

    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device.m_physDevice, nullptr, &extensionCount, nullptr);
    device.m_supportedExtensions.resize(extensionCount);
    vkEnumerateDeviceExtensionProperties(device.m_physDevice, nullptr, &extensionCount,
                                         device.m_supportedExtensions.data());

    if (supportsPresent)
    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.m_physDevice, m_surface,
                                                  &device.m_surfaceCaps);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.m_physDevice, m_surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            device.m_surfaceFormats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device.m_physDevice, m_surface, &formatCount,
                                                 device.m_surfaceFormats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.m_physDevice, m_surface, &presentModeCount,
                                                  nullptr);
        if (presentModeCount != 0)
        {
            device.m_presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device.m_physDevice, m_surface, &presentModeCount, device.m_presentModes.data());
        }
    }

    vkGetPhysicalDeviceMemoryProperties(device.m_physDevice, &device.m_memProps);

    return true;
}

VulkanRenderer::QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
        i++;
    }
    return indices;
}

void VulkanRenderer::printDeviceDetails(const PhysicalDevice& device)
{
    LOG_INFO("--------------------------------------------------", "log");
    LOG_INFO("Device Name: " + std::string(device.m_devProps.deviceName), "log");

    std::string apiVersionStr =
        std::to_string(VK_API_VERSION_MAJOR(device.m_devProps.apiVersion)) + "." +
        std::to_string(VK_API_VERSION_MINOR(device.m_devProps.apiVersion)) + "." +
        std::to_string(VK_API_VERSION_PATCH(device.m_devProps.apiVersion));
    LOG_INFO("API Version: " + apiVersionStr, "log");

    LOG_INFO("Num of family queues: " + std::to_string(device.m_qFamilyProps.size()), "log");
    for (size_t i = 0; i < device.m_qFamilyProps.size(); i++)
    {
        const auto& queueFamily = device.m_qFamilyProps[i];
        std::string flags       = "";
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            flags += "GFX Yes, ";
        else
            flags += "GFX No, ";
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            flags += "Compute Yes, ";
        else
            flags += "Compute No, ";
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            flags += "Transfer Yes";
        else
            flags += "Transfer No";

        bool supportsPresent =
            (i < device.m_qSupportsPresent.size()) && device.m_qSupportsPresent[i];
        std::string presentStr = supportsPresent ? " [Supports Present]" : " [No Present Support]";

        LOG_INFO("  Family " + std::to_string(i) + " Num queues: " +
                     std::to_string(queueFamily.queueCount) + " -> " + flags + presentStr,
                 "log");
    }

    LOG_INFO("Supported Surface Formats: " + std::to_string(device.m_surfaceFormats.size()), "log");

    LOG_INFO("Num memory types: " + std::to_string(device.m_memProps.memoryTypeCount), "log");
    for (uint32_t i = 0; i < device.m_memProps.memoryTypeCount; i++)
    {
        uint32_t              heapIndex = device.m_memProps.memoryTypes[i].heapIndex;
        VkMemoryPropertyFlags props     = device.m_memProps.memoryTypes[i].propertyFlags;

        std::string typeFlags = "";
        if (props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) typeFlags += "DEVICE_LOCAL ";
        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) typeFlags += "HOST_VISIBLE ";
        if (props & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) typeFlags += "HOST_COHERENT ";
        if (props & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) typeFlags += "HOST_CACHED ";

        LOG_INFO("  " + std::to_string(i) + ": heap " + std::to_string(heapIndex) +
                     " Flags: " + typeFlags,
                 "log");
    }

    LOG_INFO("Num heap types: " + std::to_string(device.m_memProps.memoryHeapCount), "log");
    for (uint32_t i = 0; i < device.m_memProps.memoryHeapCount; i++)
    {
        VkDeviceSize sizeInMB = device.m_memProps.memoryHeaps[i].size / (1024 * 1024);
        std::string  heapType =
            (device.m_memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                ? "DEVICE_LOCAL"
                : "HOST_SYSTEM";
        LOG_INFO("  Heap " + std::to_string(i) + " Size: " + std::to_string(sizeInMB) + " MB [" +
                     heapType + "]",
                 "log");
    }
    LOG_INFO("--------------------------------------------------", "log");
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        VkExtent2D actualExtent = {static_cast<uint32_t>(m_window->getWidth()),
                                   static_cast<uint32_t>(m_window->getHeight())};

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        LOG_ERROR("[createShaderModule] failed to create shader module", "log");
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

}  // namespace Optikos