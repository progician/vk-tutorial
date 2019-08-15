#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>


using VulkanExtensionVec = std::vector<VkExtensionProperties>;

auto VulkanExtensions() -> VulkanExtensionVec {
  auto count = 0u;
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

  auto extensions = VulkanExtensionVec(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());

  return extensions;
}


std::ostream& operator <<(std::ostream& lhs, VulkanExtensionVec rhs) {
  for (auto const& extension : rhs) {
    lhs << "\t" << extension.extensionName << "\n";
  }
  return lhs;
}


struct QueueFamilyIndices {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> presentation;

  bool complete() const {
    return graphics.has_value() && presentation;
  }
};


struct SwapChainSupport {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

static auto const Width = 800u;
static auto const Height = 600u;


class TriangleApp {
  GLFWwindow* window_{nullptr};
  VkInstance instance_;
  VkSurfaceKHR surface_;
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_;

  VkQueue graphics_;
  VkQueue present_;

  VkSwapchainKHR swapchain_;
  VkFormat format_;
  VkExtent2D extent_;
  std::vector<VkImage> swapchain_images_;
  std::vector<VkImageView> swapchain_image_views_;


  auto find_queue_families(VkPhysicalDevice device) const -> QueueFamilyIndices {
    QueueFamilyIndices indices;

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, nullptr);

    std::vector<VkQueueFamilyProperties> families(family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families.data());

    const auto graphics = std::find_if(
        families.begin(), families.end(),
        [](auto properties) {
          if (properties.queueCount < 1)
            return false;

          return (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        }
    );
    indices.graphics = graphics - families.begin();

    for (int family_index = 0; family_index < families.size(); ++family_index) {
      VkBool32 support_presentation = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index, surface_, &support_presentation);
      if (families[family_index].queueCount > 0 && support_presentation) {
        indices.presentation = family_index;
        break;
      }
    }
    return indices;
  }


  void create_surface() {
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
      throw std::runtime_error("couldn't create surface");
    }
  }


  void create_instance() {
    auto app_info = VkApplicationInfo{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    auto create_info = VkInstanceCreateInfo{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    uint32_t extension_count = 0;
    const char** extensions;
    extensions = glfwGetRequiredInstanceExtensions(&extension_count);
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    create_info.enabledLayerCount = 0;

    VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
      throw std::runtime_error("failed to create instance!");
    }
  }


  const std::vector<char const*> extensions_ = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };


  auto device_supports_extensions(VkPhysicalDevice device) const -> bool {
    auto extension_count = 0u;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    auto available = std::vector<VkExtensionProperties>(extension_count);
    vkEnumerateDeviceExtensionProperties(
      device, nullptr, &extension_count, available.data()
    );

    auto required = std::set<std::string>(extensions_.begin(), extensions_.end());
    for (const auto& extension : available) {
      required.erase(extension.extensionName);
    }
    return required.empty();
  }


  auto suitable(VkPhysicalDevice device) const -> bool {
    const auto indices = find_queue_families(device);
    const auto extensions_support = device_supports_extensions(device);
    return indices.complete();
  }


  void pick_physical_device() {
    auto count = 0u;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
      throw std::runtime_error("can't find any GPUs");
    }

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    auto device_it = std::find_if(
      devices.begin(), devices.end(),
      [this](auto device) { return suitable(device); }
    );

    if (device_it == devices.end()) {
      throw std::runtime_error("Can't find suitable GPU!");
    }
    physical_device_ = *device_it;
  }


  void create_logical_device() {
    QueueFamilyIndices indices = find_queue_families(physical_device_);

    auto create_infos = std::vector<VkDeviceQueueCreateInfo>{};
    std::set<uint32_t> unique_queue_families{
      indices.graphics.value(),
      indices.presentation.value()
    };

    for (auto queue_family : unique_queue_families) {
      auto queue_create_info = VkDeviceQueueCreateInfo{};
      queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queue_create_info.queueFamilyIndex = indices.graphics.value();
      queue_create_info.queueCount = 1;

      auto priority = 1.0f;
      queue_create_info.pQueuePriorities = &priority;
      create_infos.push_back(queue_create_info);
    }

    auto device_features = VkPhysicalDeviceFeatures{};

    auto device_create_info = VkDeviceCreateInfo{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = create_infos.data();
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(create_infos.size());
    device_create_info.pEnabledFeatures = &device_features; 
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions_.size());
    device_create_info.ppEnabledExtensionNames = extensions_.data();
    device_create_info.enabledLayerCount = 0;


    if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_) != VK_SUCCESS) {
      throw std::runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(device_, indices.graphics.value(), 0, &graphics_);
    vkGetDeviceQueue(device_, indices.presentation.value(), 0, &present_);
  }


  auto swap_chain_support(VkPhysicalDevice device) -> SwapChainSupport {
    SwapChainSupport details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device, surface_, &details.capabilities
    );

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr);
    if (format_count > 0) {
      details.formats.resize(format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(
          device, surface_, &format_count, details.formats.data()
      );
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, nullptr);
    if (present_mode_count > 0) {
      details.present_modes.resize(present_mode_count);
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          device, surface_, &present_mode_count, details.present_modes.data()
      );
    }
    return details;
  }


  auto choose_swap_surface_format(
      std::vector<VkSurfaceFormatKHR> const& formats
  ) -> VkSurfaceFormatKHR {
    auto const format_iterator = std::find_if(
        formats.begin(), formats.end(),
        [](auto const& format) {
          if (format.format != VK_FORMAT_B8G8R8_UNORM)
            return false;
          return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    );
    
    if (format_iterator != formats.end()) {
      return *format_iterator;
    }
    return formats[0];
  }


  auto choose_swap_present_mode(
      std::vector<VkPresentModeKHR> const& present_modes
  ) -> VkPresentModeKHR {
    static auto const preferred_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    auto const triple_buffering_available = std::find(
        present_modes.begin(), present_modes.end(),
        preferred_mode
    ) != present_modes.end();

    if (triple_buffering_available) {
      return preferred_mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }


  auto choose_swap_extent(VkSurfaceCapabilitiesKHR const& caps) const -> VkExtent2D {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return caps.currentExtent;
    }

    auto extent = VkExtent2D{Width, Height};
    extent.width = std::max(
        caps.minImageExtent.width,
        std::min(caps.maxImageExtent.width, extent.width)
    );
    extent.height = std::max(
      caps.minImageExtent.height,
      std::min(caps.maxImageExtent.height, extent.height)
    );
    return extent;
  }

  void create_swapchain() {
    auto support = swap_chain_support(physical_device_);
    auto surface_format = choose_swap_surface_format(support.formats);
    auto present_mode = choose_swap_present_mode(support.present_modes);
    auto extent = choose_swap_extent(support.capabilities);

    auto image_count = support.capabilities.minImageCount + 1u;
    if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
      image_count = support.capabilities.maxImageCount;
    }

    auto swapchain_create_info = VkSwapchainCreateInfoKHR{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface_;

    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto const indices = find_queue_families(physical_device_);
    uint32_t queue_family_indices[] = {indices.graphics.value(), indices.presentation.value()};
    if (indices.graphics != indices.presentation) {
      swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      swapchain_create_info.queueFamilyIndexCount = 2;
      swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
      swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain_create_info.queueFamilyIndexCount = 0;
      swapchain_create_info.pQueueFamilyIndices = nullptr;
    }

    swapchain_create_info.preTransform = support.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &swapchain_create_info, nullptr, &swapchain_) != VK_SUCCESS) {
      throw std::runtime_error("failed to create swapchain!");
    }

    auto swapchain_image_count = 0u;
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, nullptr);
    swapchain_images_.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &swapchain_image_count, swapchain_images_.data());

    format_ = surface_format.format;
    extent_ = extent;
  }


  void create_image_views() {
    swapchain_image_views_.resize(swapchain_images_.size());
    for (auto i = 0U; i < swapchain_images_.size(); i++) {
      auto create_info = VkImageViewCreateInfo{};
      create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      create_info.image = swapchain_images_[i];
      create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      create_info.format = format_;

      create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

      create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      create_info.subresourceRange.baseMipLevel = 0;
      create_info.subresourceRange.levelCount = 1;
      create_info.subresourceRange.baseArrayLayer = 0;
      create_info.subresourceRange.layerCount = 1;

      if (vkCreateImageView(device_, &create_info, nullptr, &swapchain_image_views_[i]) != VK_SUCCESS) {
        throw std::runtime_error("couldn't create image views!");
      }
    }
  }


public:
  TriangleApp(int, char const*[]) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

    create_instance();
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_image_views();
  }


  ~TriangleApp() {
    for (auto image_view : swapchain_image_views_) {
      vkDestroyImageView(device_, image_view, nullptr);
    }

    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);

    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  auto execute() -> int {
    while(!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
    }
    return 0;
  }
};


auto main(int argc, char const* argv[]) -> int {
  auto app = TriangleApp{argc, argv};
  return app.execute();
}
