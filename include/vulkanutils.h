
#ifndef VULKAN_UTILS_H
#define VULKAN_UTILS_H

#include "swapchainsupportdetails.h"

#include <glfw/glfw3.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
const std::vector<const char *> validationLayers{};
#else
constexpr bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {

  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {

  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");

  if (func) {
    return func(instance, debugMessenger, pAllocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {

  std::stringstream errorMsg;

  errorMsg << "validation layer: ";

  switch (messageSeverity) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    errorMsg << "[VERB | ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    errorMsg << "[INFO | ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    errorMsg << "[WARN | ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    errorMsg << "[ERR | ";
    break;
  }

  switch (messageType) {
  case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
    errorMsg << "GENERAL    ] ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
    errorMsg << "VALIDATION ] ";
    break;
  case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
    errorMsg << "PERFORMANCE] ";
    break;
  }

  errorMsg << pCallbackData->pMessage;

  std::cerr << errorMsg.str() << std::endl;

  return VK_FALSE;
}

/// creates and fills the fields of the debugMessengerCreateInfo.
void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {

  createInfo = {};

  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  createInfo.pfnUserCallback = debugCallback;
}

/// Returns the extensions available to the vkInstance.
std::vector<VkExtensionProperties> getInstanceExtensions() {

  uint32_t extensionCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> extensions{extensionCount};
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                         extensions.data());

  return std::move(extensions);
}

/// Returns the layers available to the vkInstance.
std::vector<VkLayerProperties> getInstanceLayers() {

  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> layers{layerCount};
  vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

  return std::move(layers);
}

/// Returns the extensions required by the vkInstance.
std::vector<const char *> getRequiredExtensions() {

  uint32_t glfwExtensionCount;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> extensions = {glfwExtensions,
                                          glfwExtensions + glfwExtensionCount};

  if (enableValidationLayers) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return std::move(extensions);
}

/// Returns the layers required by the vkInstance.
std::vector<const char *> getRequiredLayers() {
  if (enableValidationLayers) {
    return {"VK_LAYER_KHRONOS_validation"};
  } else {
    return {};
  }
}

/// Returns the subset of extensions that are required but not available.
std::vector<std::string> getUnsupportedExtensions(
    const std::vector<VkExtensionProperties> &availableExtensions,
    const std::vector<const char *> &requiredExtensions) {

  std::vector<std::string> unsupportedExtensions;

  for (size_t i = 0; i < requiredExtensions.size(); ++i) {

    const char *reqExtensionName = requiredExtensions[i];

    bool found = false;

    for (size_t j = 0; j < availableExtensions.size(); ++j) {

      const char *avExtensionName = availableExtensions[j].extensionName;

      if (strcmp(reqExtensionName, avExtensionName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      unsupportedExtensions.push_back(reqExtensionName);
    }
  }

  return std::move(unsupportedExtensions);
}

/// Returns the subset of layers that are required but not available.
std::vector<std::string>
getUnsupportedLayers(const std::vector<VkLayerProperties> &availableLayers,
                     const std::vector<const char *> &requiredLayers) {

  std::vector<std::string> unsupportedLayers;

  for (size_t i = 0; i < requiredLayers.size(); ++i) {

    const std::string &reqLayerName = requiredLayers[i];

    bool found = false;

    for (size_t j = 0; j < availableLayers.size(); ++j) {

      const char *avLayerName = availableLayers[j].layerName;

      if (strcmp(reqLayerName.c_str(), avLayerName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      unsupportedLayers.push_back(reqLayerName);
    }
  }

  return std::move(unsupportedLayers);
}

/// Check if all the required extensions are available.
void checkExtensionSupport() {

  std::vector<std::string> unsupportedExtensions = getUnsupportedExtensions(
      getInstanceExtensions(), getRequiredExtensions());

  if (unsupportedExtensions.size() > 0) {

    std::stringstream errorMsg;
    errorMsg << "Extensions requested but are not available: ";

    for (size_t i = 0; i < unsupportedExtensions.size(); ++i) {
      errorMsg << unsupportedExtensions[i];
      if (i < unsupportedExtensions.size() - 1) {
        errorMsg << ", ";
      }
    }

    throw std::runtime_error{errorMsg.str()};
  }
}

/// Check if all the layers required are available.
void checkLayerSupport() {

  std::vector<std::string> unsupportedLayers =
      getUnsupportedLayers(getInstanceLayers(), getRequiredLayers());

  if (unsupportedLayers.size() > 0) {

    std::stringstream errorMsg;
    errorMsg << "Validation layers requested but are not available: ";

    for (size_t i = 0; i < unsupportedLayers.size(); ++i) {
      errorMsg << unsupportedLayers[i];
      if (i < unsupportedLayers.size() - 1) {
        errorMsg << ", ";
      }
    }

    throw std::runtime_error{errorMsg.str()};
  }
}

std::vector<VkPhysicalDevice> getPhysicalDevices(const VkInstance instance) {

  uint32_t physicalDeviceCount;
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

  std::vector<VkPhysicalDevice> physicalDevices{physicalDeviceCount};
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount,
                             physicalDevices.data());

  return std::move(physicalDevices);
}

/// Returns the extensions available for a physical device.
std::vector<VkExtensionProperties>
getPhysicalDeviceExtensions(const VkPhysicalDevice physicalDevice) {
  uint32_t extensionsCount;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                       &extensionsCount, nullptr);

  std::vector<VkExtensionProperties> extensions{extensionsCount};

  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                       &extensionsCount, extensions.data());

  return std::move(extensions);
}

/// Returns the properties of a physical device.
VkPhysicalDeviceProperties
getPhysicalDeviceProperties(const VkPhysicalDevice physicalDevice) {

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  return properties;
}

/// Returns the features of a physical device.
VkPhysicalDeviceFeatures
getPhysicalDeviceFeatures(const VkPhysicalDevice physicalDevice) {

  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(physicalDevice, &features);

  return features;
}

// Returns the queueFamilies of a physical device.
std::vector<VkQueueFamilyProperties>
getPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice physicalDevice) {

  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies{queueFamilyCount};
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies.data());

  return std::move(queueFamilies);
}

/// Checks if a physical device & queue supports presenting images to a surface
VkBool32 hasSurfaceSupport(const VkPhysicalDevice physicalDevice,
                           const uint32_t queueFamilyIndex,
                           const VkSurfaceKHR surface) {

  VkBool32 surfaceSupport;
  vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex,
                                       surface, &surfaceSupport);

  return surfaceSupport;
}

/// Returns the extensions required for a physical device (swapchain support).
std::vector<const char *> getRequiredPhysicalDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

/// Check if all the required extensions are available for a physical device.
bool checkPhysicalDeviceExtensionSupport(
    const VkPhysicalDevice physicalDevice) {

  std::vector<std::string> unsupportedExtensions =
      getUnsupportedExtensions(getPhysicalDeviceExtensions(physicalDevice),
                               getRequiredPhysicalDeviceExtensions());

  return unsupportedExtensions.size() == 0;
}

/// Returns swapchain support info for a physical device & a surface.
SwapchainSupportDetails
querySwapChainSupport(const VkPhysicalDevice physicalDevice,
                      const VkSurfaceKHR surface) {

  SwapchainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       nullptr);
  if (formatCount > 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, nullptr);
  if (presentModeCount > 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                              &presentModeCount,
                                              details.presentModes.data());
  }

  return details;
}

/// Returns the images that belong to a specific device & swapchain.
std::vector<VkImage> getSwapchainImages(VkDevice device,
                                        VkSwapchainKHR swapchain) {

  uint32_t swapchainImageCount;
  vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);

  // When doing uniform initialization, initializer_list constructor takes
  // precedence over other constructors
  std::vector<VkImage> swapchainImages(swapchainImageCount);
  vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount,
                          swapchainImages.data());

  return std::move(swapchainImages);
}

#endif // VULKAN_UTILS_H