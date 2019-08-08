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


class TriangleApp {
  GLFWwindow* window_{nullptr};
  VkInstance instance_;
  VkSurfaceKHR surface_;
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_;

  VkQueue graphics_;
  VkQueue present_;


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
    device_create_info.enabledExtensionCount = 0;
    device_create_info.enabledLayerCount = 0;


    if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_) != VK_SUCCESS) {
      throw std::runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(device_, indices.graphics.value(), 0, &graphics_);
    vkGetDeviceQueue(device_, indices.presentation.value(), 0, &present_);
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
  }


  ~TriangleApp() {
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
