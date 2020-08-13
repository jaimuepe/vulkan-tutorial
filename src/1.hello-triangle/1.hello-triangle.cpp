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

  /// creates the surface to interact with the window system
  void createSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create window surface!");
    }
  }

  /// Pick the optimal swapchain surface format (B8G8R8A8 & SRGB)
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

  /// Pick the optimal swapchain present mode (VK_PRESENT_MODE_MAILBOX_KHR)
  VkPresentModeKHR
  pickSwapchainPresentMode(const std::vector<VkPresentModeKHR> availableModes) {

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

    // any GPU that supports graphics queue & presenting images
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

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

      // queue priority in the command buffer scheduling
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

    // layer extensions are deprecated, but we should set them for older
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
    // means 'unbounded')
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

  void mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {

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