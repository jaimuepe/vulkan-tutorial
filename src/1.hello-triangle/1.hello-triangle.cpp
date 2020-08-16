#include "filesystemutils.h"
#include "queuefamilyindices.h"
#include "swapchainsupportdetails.h"
#include "vulkanutils.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

class HelloTriangleApp {

public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  GLFWwindow *m_window;

  VkInstance m_instance;
  VkDebugUtilsMessengerEXT m_debugMessenger;

  VkSurfaceKHR m_surface;

  VkPhysicalDevice m_physicalDevice;
  VkDevice m_device;
  VkQueue m_graphicsQueue;
  VkQueue m_presentQueue;

  VkSwapchainKHR m_swapchain;

  VkExtent2D m_swapchainExtent;
  VkFormat m_swapchainImageFormat;

  std::vector<VkImage> m_swapchainImages;
  std::vector<VkImageView> m_swapchainImageViews;

  VkRenderPass m_renderPass;
  VkPipelineLayout m_pipelineLayout;

  void initWindow() {

    glfwInit();

    // no openGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
  }

  /// Create a vkInstance to interact with the vulkan driver
  void createInstance() {

    checkLayerSupport();
    checkExtensionSupport();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // The extensions needed by glfw (surface capabilities)
    std::vector<const char *> requiredExtensions = getRequiredExtensions();

    createInfo.enabledExtensionCount = requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    // layers required (if debug we require the VK_LAYER_KHRONOS_validation)
    std::vector<const char *> requiredLayers = getRequiredLayers();

    createInfo.enabledLayerCount = requiredLayers.size();
    createInfo.ppEnabledLayerNames = requiredLayers.data();

    // outside of the if to avoid early destruction
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
      populateDebugMessengerCreateInfo(debugCreateInfo);

      // to create a special debugMessenger used only
      // during instance creation
      createInfo.pNext = &debugCreateInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to create vk_instance!"};
    }
  }

  /// Create the debug messenger that will handle all messages.
  void setupDebugMessenger() {

    if (!enableValidationLayers) {
      return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr,
                                     &m_debugMessenger) != VK_SUCCESS) {
      throw std::runtime_error{"Failed to set up debug messenger!"};
    }
  }

  /// creates the surface to interact with the window system.
  void createSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) !=
        VK_SUCCESS) {
      throw std::runtime_error{"Failed to create window surface!"};
    }
  }

  /// Pick the optimal swapchain surface format (B8G8R8A8 & SRGB).
  VkSurfaceFormatKHR pickSwapchainSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats) {

    for (const VkSurfaceFormatKHR &surfaceFormat : availableFormats) {
      if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return surfaceFormat;
      }
    }

    // if we can't find an optimal format just use the first available
    return availableFormats[0];
  }

  /// Pick the optimal swapchain present mode (VK_PRESENT_MODE_MAILBOX_KHR).
  VkPresentModeKHR pickSwapchainPresentMode(
      const std::vector<VkPresentModeKHR> &availableModes) {

    for (const VkPresentModeKHR &presentMode : availableModes) {
      if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return presentMode;
      }
    }

    // this one is always available
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  /// Pick the optimal swapchain extent.
  VkExtent2D
  pickSwapchainExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities) {

    if (surfaceCapabilities.currentExtent.width == UINT32_MAX) {

      // special case - the window manager asks us to pick the surface

      VkExtent2D currentExtent{WIDTH, HEIGHT};

      currentExtent.width = glm::clamp(
          currentExtent.width, surfaceCapabilities.minImageExtent.width,
          surfaceCapabilities.maxImageExtent.width);

      currentExtent.height = glm::clamp(
          currentExtent.height, surfaceCapabilities.minImageExtent.height,
          surfaceCapabilities.maxImageExtent.height);

      return currentExtent;
    }

    // just use the actual extent
    return surfaceCapabilities.currentExtent;
  }

  /// Checks if a physical devices matches the application requirements.
  bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) {

    // check if this device has all the extensions needed
    bool extensionsSupported =
        checkPhysicalDeviceExtensionSupport(physicalDevice);

    if (!extensionsSupported) {
      return false;
    }

    // at this point swapchain is supported
    SwapchainSupportDetails swapchainDetails =
        querySwapChainSupport(physicalDevice, m_surface);

    // make sure the specifics of our swapchain are supported

    bool swapchainAddecuate = !swapchainDetails.formats.empty() &&
                              !swapchainDetails.presentModes.empty();

    if (!swapchainAddecuate) {
      return false;
    }

    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    // any GPU that supports graphics queue & presenting images
    return queueFamilyIndices.isComplete();
  }

  /// Picks an appropiate physicalDevice (graphics card).
  void pickPhysicalDevice() {

    std::vector<VkPhysicalDevice> physicalDevices =
        getPhysicalDevices(m_instance);

    if (physicalDevices.size() == 0) {
      throw std::runtime_error{"Failed to find GPUs with vulkan support!"};
    }

    for (const VkPhysicalDevice &device : physicalDevices) {
      if (isPhysicalDeviceSuitable(device)) {
        m_physicalDevice = device;
        break;
      }
    }

    if (!m_physicalDevice) {
      throw std::runtime_error{"Failed to find a suitable GPU!"};
    }

    VkPhysicalDeviceProperties properties =
        getPhysicalDeviceProperties(m_physicalDevice);

    std::cout << "Physical device: " << properties.deviceName << std::endl;
  }

  /// creates a logical device to interface with the selected physical device.
  void createLogicalDevice() {

    QueueFamilyIndices queueIndices = findQueueFamilies(m_physicalDevice);

    std::set<uint32_t> uniqueQueueFamilyIndices{
        queueIndices.graphicsFamily.value(),
        queueIndices.presentFamily.value()};

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilyIndices.size());

    // We have to create one queueCreateInfo for each unique queue index, even
    // if the same queue if used for multiple things
    for (const uint32_t &queueFamilyIndex : uniqueQueueFamilyIndices) {

      VkDeviceQueueCreateInfo queueCreateInfo{};

      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

      queueCreateInfo.queueCount = 1;
      queueCreateInfo.queueFamilyIndex = queueFamilyIndex;

      // queue priority in the command buffer scheduling. Not used for now but
      // we still have to fill it
      float queuePriority = 1.0f;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos.push_back(queueCreateInfo);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    // we dont need any special feature right now - everything defaults to FALSE
    VkPhysicalDeviceFeatures deviceFeatures{};
    createInfo.pEnabledFeatures = &deviceFeatures;

    // device required extensions (swapchain support)
    std::vector<const char *> extensions =
        getRequiredPhysicalDeviceExtensions();
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    // device layers are deprecated, but we could set them for older
    // implementations
    std::vector<const char *> layers = getRequiredLayers();
    createInfo.enabledLayerCount = layers.size();
    createInfo.ppEnabledLayerNames = layers.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) !=
        VK_SUCCESS) {
      throw std::runtime_error{"Failed to create logical device!"};
    }

    // retrieve the first queue of the graphics & present family
    vkGetDeviceQueue(m_device, queueIndices.graphicsFamily.value(), 0,
                     &m_graphicsQueue);

    vkGetDeviceQueue(m_device, queueIndices.presentFamily.value(), 0,
                     &m_presentQueue);
  }

  /// Returns the neccesary query indices for a physical device.
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) const {

    QueueFamilyIndices indices;

    std::vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(physicalDevice);

    for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {

      // supports graphics queue?
      if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = i;
      }

      // supports presenting images to a surface?
      if (hasSurfaceSupport(physicalDevice, i, m_surface)) {
        indices.presentFamily = i;
      }

      if (indices.isComplete()) {
        break;
      }
    }

    return indices;
  }

  /// Creates the swapchain, picking the optimal configuration (surface format,
  /// present mode, extent, number of images...)
  void createSwapchain() {

    SwapchainSupportDetails swapchainDetails =
        querySwapChainSupport(m_physicalDevice, m_surface);

    VkSurfaceFormatKHR surfaceFormat =
        pickSwapchainSurfaceFormat(swapchainDetails.formats);

    VkPresentModeKHR presentMode =
        pickSwapchainPresentMode(swapchainDetails.presentModes);

    VkExtent2D extent = pickSwapchainExtent(swapchainDetails.capabilities);

    // +1 to avoid wait times caused by internal operations before we can
    // acquire another image to render to
    uint32_t imageCount = swapchainDetails.capabilities.minImageCount + 1;

    // Also careful to not exceed the maxImageCount (0 is a special value that
    // means 'don't care')
    if (swapchainDetails.capabilities.maxImageCount > 0) {
      imageCount =
          glm::min(imageCount, swapchainDetails.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    createInfo.surface = m_surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;

    // amount of layers each image consist of (always 1 except when
    // stereoscopic applications)
    createInfo.imageArrayLayers = 1;

    // what are we going to use the image for?
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    if (indices.graphicsFamily == indices.presentFamily) {
      // same queue, no need for concurrent access (an image is owned by the
      // queue)
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
      // the image ownership is shared between queues
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

      uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                       indices.presentFamily.value()};

      // only needed when concurrent to specify which queues have ownership of
      // an image
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }

    // to apply an specific pretransform to all images in the swapchain
    createInfo.preTransform = swapchainDetails.capabilities.currentTransform;

    // to allow blending with other windows in the windows system
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode = presentMode;

    // if we don't care about the pixels that are shadowed by another window
    createInfo.clipped = VK_TRUE;

    // sometimes the swapchain will become invalid (eg. when resizing the
    // window). In these cases we can use the old swapchain to recreate the new
    // one
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create swapchain!");
    }

    m_swapchainExtent = extent;
    m_swapchainImageFormat = surfaceFormat.format;

    m_swapchainImages = getSwapchainImages(m_device, m_swapchain);
  }

  /// Create the imageviews that allow us to interact with the swapchain images.
  void createImageViews() {

    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i) {

      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

      createInfo.image = m_swapchainImages[i];

      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = m_swapchainImageFormat;

      // We can map a channel to another channel, or even to a constant value.
      // For now just the default value
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

      // subresource range describes the image purpose and which parts should be
      // accessed
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      // no mipmap
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      // layers are for stereoscopic apps
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(m_device, &createInfo, nullptr,
                            &m_swapchainImageViews[i]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image views!");
      }
    }
  }

  /// Setup the render pass (specify the framebuffer attachments & subpasses
  /// that will be used for rendering the frame).
  void createRenderPass() {

    // we are going to use just a color buffer attachment. The format should
    // match with the swapchain images
    VkAttachmentDescription colorAttachmentDesc{};
    colorAttachmentDesc.format = m_swapchainImageFormat;
    colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

    // to clear the values before
    colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // to store the content in memory so we can read them later (so we can
    // present them)
    colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // we don't care about the initial stencil values nor the final stencil
    // values
    colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // images need to be in specific layouts that are suitable to the operation
    // they are going to be involved next

    // initial layout is the layout the images will have before the render pass
    // begins. Since we are going to clear the image anyway we don't care about
    // the initial layout
    colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // final layout is the layout the images will have after the render pass
    // ends. Since we want to present the images this is the optimal
    // layout.
    colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // a render pass can consist of multiple subpasses. for example, a sequence
    // of post-processing events would be multiple passes since each one depends
    // on the results of the previous one. By grouping them in a single
    // renderpass vulkan can reorder operations and conserve memory bandwidth.

    // for now just a simple subpass

    VkAttachmentReference colorAttachmentRef{};

    // this references the AttachmentDescription at index 0 (the one we have
    // created previously).
    colorAttachmentRef.attachment = 0;

    // the layout we would like the attachment to have during the subpass.
    // Vulkan will transition the attachment to this layout when the subpass
    // starts.
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc{};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // The order of these color attachments is directly referenced in the
    // shaders: layout(location = 0) vec4 outColor;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    // it seems 'weird' that we have to specify the attachments twice: in the
    // renderpass we reference the attachmentDesc and in the subpass we
    // reference the  attachmentRef, which also points to the index of the
    // attachmentDesc in the renderpass.

    // It makes sense, since the subpass can only work with attachments already
    // defined in the renderpass. But i still have to wrap my head around it.

    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachmentDesc;

    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDesc;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create RenderPass!");
    }
  }

  /// Creates & configures the rendering pipeline & stages (vertex input, vertex
  /// shader, rasterizer, fragment shader...)
  void createGraphicsPipeline() {

    // *** shader modules ***
    // create the shader modules of the pipeline

    std::vector<char> vertShaderCode = readShaderFile("vert.spv");
    std::vector<char> fragShaderCode = readShaderFile("frag.spv");

    VkShaderModule vertShaderModule =
        createShaderModule(m_device, vertShaderCode);
    VkShaderModule fragShaderModule =
        createShaderModule(m_device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    // entrypoint
    vertShaderStageInfo.pName = "main";

    // this allows us to set values for shader constants (!!)
    // it's faster than using uniforms because the compiler can do optimizations
    // like eliminating if statements
    // vertShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = vertShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                      fragShaderStageInfo};

    // *** vertex input ***
    // define the vertex shader input attributes

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // no bindings / attributes for now, since the data is hardcoded in the
    // shader files
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    // *** input assembly ***
    // define the type of geometry & primitive restart

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // primitiveRestart allows to break triangles and lines when using a _STRIP
    // topology
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    // *** viewport & scissors ***
    // viewports define transformation from image->framebuffer
    // scissor define in which region the pixels will be stored

    // viewports can scale the image but scissors can only "cut" it

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    // size of the swapchain images doesn't necessarily have to match with the
    // window size
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    // depth values used for the framebuffer (usually [0.0 - 1.0])
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportStateInfo{};
    viewportStateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.pViewports = &viewport;
    viewportStateInfo.scissorCount = 1;
    viewportStateInfo.pScissors = &scissor;

    // *** rasterizer ***
    // configure the rasterizing stage (generation of fragments)
    // also depth testing, face culling, scissor test, wireframe mode

    VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
    rasterizerInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // depthClamp means that fragments that are outside the nearplane-farplane
    // region are clamped instead of discarded
    // requires a GPU feature
    // useful for shadow maps
    rasterizerInfo.depthClampEnable = VK_FALSE;

    // if discardEnable = true then geometry never passes the rasterizer stage,
    // disabling any output to the framebuffer
    rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;

    // determines how fragments are generated for geometry (FILL, LINE, POINT)
    // requires a GPU feature if other than FILL
    rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;

    // thickness of lines in terms of number of fragments
    // requires a GPU feature if > 1.0f (wideLines)
    rasterizerInfo.lineWidth = 1.0f;

    // to cull back faces
    rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    // clockwise? in OpenGL is CCW
    rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // the rasterizer can alter the depth values applying a constant bias or
    // based on a fragment's slope
    //  useful for shadow mapping
    rasterizerInfo.depthBiasEnable = VK_FALSE;
    rasterizerInfo.depthBiasConstantFactor = 0.0f;
    rasterizerInfo.depthBiasClamp = 0.0f;
    rasterizerInfo.depthBiasSlopeFactor = 0.0f;

    // *** multisampling ***
    // configure the MSAA (requires GPU feature)

    // for now we are not going to use it
    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    multisampleInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // *** depth & stencil testing ***

    // not going to use one for now

    // *** color blending ***
    // define the framebuffer blending

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};

    // defines which channels will be present in the final color
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = GL_FALSE;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;

    // *** dynamic state ***
    // some aspects of the pipeline can be changed dynamically without having to
    // recreate the pipeline

    // unused for now

    // *** pipeline layout ***
    // specify the uniforms passed to the shaders

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // for now we are not going to define any layout

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr,
                               &m_pipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create pipeline layout!");
    }

    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {

    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

    for (const VkImageView &imageView : m_swapchainImageViews) {
      vkDestroyImageView(m_device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    vkDestroyDevice(m_device, nullptr);

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

    if (enableValidationLayers) {
      DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }

    vkDestroyInstance(m_instance, nullptr);

    glfwDestroyWindow(m_window);
    glfwTerminate();
  }
};

int main() {
  HelloTriangleApp app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}