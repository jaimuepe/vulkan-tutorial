#include "queuefamilyindices.h"
#include "vulkanutils.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
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

  VkPhysicalDevice m_physicalDevice;

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
    pickPhysicalDevice();
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
      throw std::runtime_error{"failed to create vk_instance!"};
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
      throw std::runtime_error("Failed to set up debug messenger!");
    }
  }

  /// Checks if a physical devices matches the application requirements
  bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice) {

    // any GPU supports graphics queue
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    return queueFamilyIndices.isComplete();
  }

  /// Picks an appropiate physicalDevice (graphics card)
  void pickPhysicalDevice() {

    std::vector<VkPhysicalDevice> physicalDevices =
        getPhysicalDevices(m_instance);

    if (physicalDevices.size() == 0) {
      throw std::runtime_error("Failed to find GPUs with vulkan support!");
    }

    for (const VkPhysicalDevice &device : physicalDevices) {
      if (isPhysicalDeviceSuitable(device)) {
        m_physicalDevice = device;
        break;
      }
    }

    if (!m_physicalDevice) {
      throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties properties =
        getPhysicalDeviceProperties(m_physicalDevice);

    std::cout << "Physical device: " << properties.deviceName << std::endl;
  }

  /// Returns the neccesary query indices for a physical device.
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) {

    QueueFamilyIndices indices;

    std::vector<VkQueueFamilyProperties> queueFamilyProperties =
        getPhysicalDeviceQueueFamilyProperties(physicalDevice);

    for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {

      // supports graphics queue?
      if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = i;
      }

      if (indices.isComplete()) {
        break;
      }
    }

    return indices;
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {

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