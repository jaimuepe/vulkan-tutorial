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

/// Defines how many simultaneous frames we can process in the
/// drawFrame function. If we try to push one more we will have to wait
/// (vkWaitFences).
/// Shouldn't be higher than the number of swapchainImages!
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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

  std::vector<VkFramebuffer> m_swapchainFramebuffers;

  VkRenderPass m_renderPass;
  VkPipelineLayout m_pipelineLayout;

  VkPipeline m_graphicsPipeline;

  VkCommandPool m_commandPool;
  std::vector<VkCommandBuffer> m_commandBuffers;

  std::vector<VkSemaphore> m_imageAvailableSemaphores;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;
  std::vector<VkFence> m_inFlightFences;

  size_t m_currentFrame = 0;

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
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
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

    // to clear the values before the renderpass begins
    colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // to store the content in memory so we can read them later after the
    // renderpass ends (so we can present them)
    colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // we don't care about the stencil in this example
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

    // from the doc:
    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL must only be used as a color or
    // resolve attachment in a VkFramebuffer. This layout is valid only for
    // image subresources of images created with the
    // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT usage bit enabled.

    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc{};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // from the doc:
    // Each element of the pInputAttachments array corresponds to an input
    // attachment index in a fragment shader, i.e. if a shader declares an image
    // variable decorated with a InputAttachmentIndex value of X, then it uses
    // the attachment provided in pInputAttachments[X].

    // Each element of the pColorAttachments array corresponds to an output
    // location in the shader, i.e. if the shader declares an output variable
    // decorated with a Location value of X, then it uses the attachment
    // provided in pColorAttachments[X].

    // in our helloTriangle example, we have in the fragment shader:
    // layout(location = 0) out vec4 outColor;

    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorAttachmentRef;

    // we are using a semaphore to wait for the presentation engine to be done
    // before we can use the image in the drawFrame() method. We also are
    // telling the command buffer to wait in the
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT. However, the subpass
    // layout transition happens as soon as it begins, and we might not have the
    // image ready yet. One way of solving this is using a subpass dependency to
    // stop the subpass from starting until we have reached the
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT stage in the previous
    // subpass (EXTERNAL - whatever happened before the renderpass).

    VkSubpassDependency subpassDependency{};
    // these fields specify the indices of the dependency and the dependent
    // subpass.
    //  VK_SUBPASS_EXTERNAL refers to the implicit subpass before or
    // after the render pass (depending if we use it as src or dst)
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    // 0 means our subpass (since we only have one)
    subpassDependency.dstSubpass = 0;

    // The srcStageMask and srcAccessMask specify the operations where we have
    // to wait and in which stages to they occur.
    // In our case, we have to wait until the image is ready before we can
    // actually write to it (the semaphore in drawFrame() waits until we reach
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
    subpassDependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;

    subpassDependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &subpassDependency;

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

    std::vector<char> vertShaderCode = readShaderFile("shader.vert.spv");
    std::vector<char> fragShaderCode = readShaderFile("shader.frag.spv");

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
    fragShaderStageInfo.module = fragShaderModule;
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
    // viewports describe the region of the framebuffer that the output will be
    // rendered to
    // scissor define in which region the pixels will be stored

    // viewports can scale the image but scissors can only "cut" it. Anything
    // outside the scissor rectangle will be discarded by the rasterizer.

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

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    // vertex + fragment shader
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizerInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;

    pipelineInfo.layout = m_pipelineLayout;

    // we can actually use this pipeline with another renderpass, but they have
    // to be compatible with the renderpass.
    pipelineInfo.renderPass = m_renderPass;
    // the subpass that will use this pipeline
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
  }

  /// Create the framebuffers that will wrap the attachments used during the
  /// render pass. A framebuffer references all of the VkImageView objects that
  /// represent those attachments. Since we have multiple VkImageViews, we need
  /// multiple VkFramebuffers.
  void createFramebuffers() {

    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {

      VkFramebufferCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

      // Which renderpass this framebuffer has to be COMPATIBLE with (but can be
      // used with other compatible renderpasses).
      createInfo.renderPass = m_renderPass;

      // from the doc:
      // pAttachments is a pointer to an array of VkImageView handles, each of
      // which will be used as the corresponding attachment in a render pass
      // instance.
      createInfo.attachmentCount = 1;
      createInfo.pAttachments = &m_swapchainImageViews[i];

      // why do we have a width & height here? don't we already have the
      // swapchain images?
      //  from the doc:
      // It is legal for a subpass to use no color or depth/stencil attachments,
      // either because it has no attachment references or because all of them
      // are VK_ATTACHMENT_UNUSED. This kind of subpass can use shader side
      // effects such as image stores and atomics to produce an output. In this
      // case, the subpass continues to use the width, height, and layers of the
      // framebuffer to define the dimensions of the rendering area[...]
      createInfo.width = m_swapchainExtent.width;
      createInfo.height = m_swapchainExtent.height;

      createInfo.layers = 1;

      if (vkCreateFramebuffer(m_device, &createInfo, nullptr,
                              &m_swapchainFramebuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer!");
      }
    }
  }

  /// We need a command pool from which we can create command buffers. Command
  /// pools manage the memory of their command buffers.
  void createCommandPool() {

    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // command buffers are submitted to a device queue, so we have to specify to
    // which queue it's going to be submitted.
    // Each command pool can only allocate command buffers from a single queue.

    // For now we are going to create a commandPool to records commands for
    // drawing
    createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create command pool!");
    }
  }

  /// Allocate the command buffers and record the drawing commands in them. We
  /// need to create one commandBuffer for each image in the swapchain because
  /// one of the steps of the command is to bind a framebuffer.
  void createCommandBuffers() {

    m_commandBuffers.resize(m_swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo commandBufferInfo{};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool = m_commandPool;

    // primary buffers can be submitted to a queue for execution but can't be
    // called from other command buffers; secondary cannot be submitted directly
    // but can be called from primary command buffers
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    commandBufferInfo.commandBufferCount = m_commandBuffers.size();

    if (vkAllocateCommandBuffers(m_device, &commandBufferInfo,
                                 m_commandBuffers.data()) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate command buffers!");
    }

    // record each command buffer
    for (size_t i = 0; i < m_commandBuffers.size(); ++i) {

      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

      if (vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
      }

      VkRenderPassBeginInfo renderPassInfo{};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = m_renderPass;
      renderPassInfo.framebuffer = m_swapchainFramebuffers[i];

      // Define the size of the render area. The pixels outside this region will
      // have undefined values. For best performance it should match the
      // attachments size.
      renderPassInfo.renderArea.offset = {0, 0};
      renderPassInfo.renderArea.extent = m_swapchainExtent;

      VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = &clearColor;

      // CONTENTS_INLINE means the render pass commands will be embedded in the
      // primary command buffer and no secondary buffers will be executed.
      // CONTENTS_SECONDARY means that the renderpass commands will be executed
      // from secondary render buffers.
      vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo,
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                        m_graphicsPipeline);

      vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);

      vkCmdEndRenderPass(m_commandBuffers[i]);

      if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
      }
    }
  }

  /// We need to create semaphores to synchronize the operations, since they are
  /// executed asynchronously.
  void createSyncObjects() {

    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // we have to create the fences as SIGNALED (to fake that they have been
    // signaled before and we can use them to wait in our drawFrame function)
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      if (vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr,
                            &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
          vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr,
                            &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
          vkCreateFence(m_device, &fenceCreateInfo, nullptr,
                        &m_inFlightFences[i])) {
        throw std::runtime_error("Failed to create synchronization objects!");
      }
    }
  }

  void mainLoop() {

    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
      drawFrame();
    }

    // since vulkan is asynchronous we have to wait until the all the operations
    // have ended before we can delete the objects
    vkDeviceWaitIdle(m_device);
  }

  void drawFrame() {

    // The drawing consist of three stages:
    // 1) Acquiring an image from the swapchain
    // 2) Execute the command buffer with the image as attachment
    // 3) Return the image to the swapchain for presentation

    // but first, we have to wait for the current image to end all previous work
    // (if CPU is going too fast, the work can be scheduled faster than the gpu
    // can process it and we might try to fetch an image that is not ready yet)

    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE,
                    UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    // *** 1) Acquiring the image ***

    // A timeout of UINT64_MAX means no timeout
    // from the doc:
    // The presentation engine may not have finished reading from the image at
    // the time it is acquired, so the application must use semaphore and/or
    // fence to ensure that the image layout and contents are not modified until
    // the presentation engine reads have completed.
    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                          m_imageAvailableSemaphores[m_currentFrame],
                          VK_NULL_HANDLE, &imageIndex);

    // *** 2) Submitting the command buffer ***

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // from the doc:
    // The presentation engine may not have finished reading from the image at
    // the time it is acquired, so the application must use semaphore and/or
    // fence to ensure that the image layout and contents are not modified until
    // the presentation engine reads have completed.

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];

    // Each entry in the waitSemaphores array corresponds with the stage in
    // waitDstStageMask. We are basically saying that it has to wait for the
    // 'imageAvailableSemaphore' in the stage that writes to the color
    // attachment (fragmentShader), but it can execute stuff before that stage
    // even if the image is not yet available.

    // from the doc:
    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT specifies the stage of the
    // pipeline after blending where the final color values are output from the
    // pipeline.
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

    // we notify the renderFinishedSemaphore when the command buffer has
    // finished execution
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

    // the fence will be signaled when the command finishes execution
    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo,
                      m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // we have to wait for the renderFinishedSemaphore before we can grab the
    // image to present it
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(m_presentQueue, &presentInfo);

    // if we don't wait at the end of the frame we get a bunch of errors because
    // we are submitting work too fast and reusing the semaphores in different
    // frames before the previous one has ended with them.

    // however, this is not a good solution because now the whole pipeline is
    // only used for one frame. Ideally we want to start in the next frame the
    // stages that are already done in the current frame.

    // vkQueueWaitIdle(m_presentQueue);

    // we can also do this with the flamesInFlight approach: we only allow a
    // number of frames (lower than the # of swapchainImages) in the background,
    // and force the synchronization with fences

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  void cleanup() {

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
      vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
      vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

    for (const VkFramebuffer &framebuffer : m_swapchainFramebuffers) {
      vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

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