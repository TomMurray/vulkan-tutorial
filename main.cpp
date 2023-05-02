#include <iostream>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#define APP_NAME "vulkan-tutorial"

#define DEFAULT_WIDTH (800)
#define DEFAULT_HEIGHT (600)

int main(int argc, char** argv) {
    // Initialise SDL subsystems - loading everything for
    // now though we don't need it.
    if (SDL_Init(SDL_INIT_EVERYTHING)) {
        std::cerr << "Failed to initialize SDL subsystems\n";
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Surface* screen_surface = NULL;

    // Create the SDL window, with vulkan enabled
    if ((window = SDL_CreateWindow(APP_NAME,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   DEFAULT_WIDTH,
                                   DEFAULT_HEIGHT,
                                   SDL_WINDOW_SHOWN |
                                   SDL_WINDOW_VULKAN)) == NULL) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        return 1;
    }

    VkInstance instance;
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = APP_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get the required Vulkan extensions to use it with SDL
    uint32_t sdl_ext_count = 0;
    if (SDL_Vulkan_GetInstanceExtensions(window, &sdl_ext_count, NULL) == SDL_FALSE) {
        std::cerr << "Failed to count required vulkan extensions for SDL: " << SDL_GetError() << "\n";
        return 1;
    }

    std::cout << "There are " << sdl_ext_count << " required Vulkan extensions for SDL\n";
    char **sdl_ext_names = new char*[sdl_ext_count];
    if (SDL_Vulkan_GetInstanceExtensions(window, &sdl_ext_count, const_cast<const char **>(sdl_ext_names)) == SDL_FALSE) {
        std::cerr << "Failed to get names of required vulkan extensions for SDL: " << SDL_GetError() << "\n";
        return 1;
    }
    createInfo.enabledExtensionCount = sdl_ext_count;
    createInfo.ppEnabledExtensionNames = sdl_ext_names;

    { // Dump available instance extensions 
        uint32_t ext_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);

        std::cout << "Found " << ext_count << " extensions:\n";

        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());

        for (const auto &ext : exts) {
            std::cout << "  " << ext.extensionName << "\n";
        }
    }

    const std::vector<const char *> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

    #ifdef NDEBUG
    const bool enable_validation_layers = false;
    #else 
    const bool enable_validation_layers = false;
    #endif

    { // Validation layer configuration

        if (enable_validation_layers) {
            uint32_t layer_count = 0;
            vkEnumerateInstanceLayerProperties(&layer_count, NULL);

            std::vector<VkLayerProperties> layers(layer_count);
            vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
        
            bool found = false;
            for (const auto &layer_name : validation_layers) {
                auto it = std::find_if(layers.begin(), layers.end(), [&layer_name](const auto &properties) {
                    return std::strcmp(properties.layerName, layer_name) == 0;
                });
                if (it == layers.end()) {
                    std::cerr << "Failed to find validation layer " << layer_name << std::endl;
                    return 1;
                }
            }
            createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            createInfo.ppEnabledLayerNames = validation_layers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
    }

    const VkAllocationCallbacks *apiAllocCallbacks = nullptr;
    VkResult result = VK_SUCCESS;
    if ((result = vkCreateInstance(&createInfo, apiAllocCallbacks, &instance)) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << string_VkResult(result) << std::endl;
        return 1;
    }
    std::cout << "Created VkInstance" << std::endl;

    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, instance, &surface) == SDL_FALSE) {
        std::cerr << "Failed to create SDL window surface for Vulkan: " << SDL_GetError() << std::endl;
        return 1;
    }

    delete[] sdl_ext_names;

    const std::vector<const char *> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    int queue_graphics_family = 0, queue_present_family = 0;
    struct SwapChainSupport {
        VkSurfaceCapabilitiesKHR caps;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> modes;
    } swap_chain_support{};
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, NULL);

        if (device_count == 0) {
            std::cerr << "Failed to find a VkPhysicalDevice to use" << std::endl;
            return 1;
        }
        std::cout << "Found " << device_count << " potential VkPhysicalDevices to use" << std::endl;

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        for (const auto &device : devices) {
            std::cout << "Checking a physical device" << std::endl;
            VkPhysicalDeviceProperties props;
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceProperties(device, &props);
            vkGetPhysicalDeviceFeatures(device, &features);

            if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                continue;
            }

            uint32_t extension_count = 0;
            vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);
            std::vector<VkExtensionProperties> extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions.data());

            std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());
            for (const auto &extension : extensions) {
                required_extensions.erase(extension.extensionName);
            }

            if (!required_extensions.empty()) {
                continue;
            }

            // Check swap-chain support
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swap_chain_support.caps);

            uint32_t format_count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);

            if (format_count == 0) {
                continue;
            }

            swap_chain_support.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, swap_chain_support.formats.data());


            uint32_t present_mode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);

            if (present_mode_count == 0) {
                continue;
            }

            swap_chain_support.modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, swap_chain_support.modes.data());

            physical_device = device;
            break;
        }

        if (physical_device == VK_NULL_HANDLE) {
            std::cerr << "Failed to find a suitable VkPhysicalDevice to use" << std::endl;
            return 1;
        }

        std::cout << "Found a VkPhysicalDevice" << std::endl;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        std::optional<int> graphics, present;
        for (std::size_t idx = 0; idx != queue_family_count; ++idx) {
            if (queue_families[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics = static_cast<int>(idx);
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, static_cast<uint32_t>(idx), surface, &present_support);
            if (present_support) {
                present = static_cast<int>(idx);
            }
        }

        if (!graphics) {
            std::cerr << "No queue family with graphics capability found" << std::endl;
            return 1;
        }
        if (!present) {
            std::cerr << "No queue family with present capability found" << std::endl;
            return 1;
        }
        queue_graphics_family = *graphics;
        queue_present_family = *present;
        std::cout << "Found queue graphics and present families" << std::endl;
    }

    // Create logical device
    VkDevice device = VK_NULL_HANDLE;
    {
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

        std::set<int> unique_queue_families = {queue_graphics_family, queue_present_family};
        float queue_priority = 1.0f;
        for (const auto &family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_graphics_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.emplace_back(std::move(queue_create_info));
        }

        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(physical_device, &device_features);
        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pEnabledFeatures = &device_features;

        create_info.ppEnabledExtensionNames = device_extensions.data();
        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());

        if (enable_validation_layers) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
            create_info.ppEnabledLayerNames = validation_layers.data();
        } else {
            create_info.enabledLayerCount = 0;
        }
        
        if ((result = vkCreateDevice(physical_device, &create_info, apiAllocCallbacks, &device)) != VK_SUCCESS) {
            std::cerr << "Failed to create logical device: " << string_VkResult(result) << std::endl;
            return 1;
        }
        std::cout << "Created logical device" << std::endl;
    }

    // Get our graphics queue.
    VkQueue graphics_queue, present_queue;
    // NOTE: These queues may well be the same, but these are just handles
    // to them. When creating the logical device we ensured we used unique 
    // queue indices.
    vkGetDeviceQueue(device, queue_graphics_family, 0, &graphics_queue);
    vkGetDeviceQueue(device, queue_present_family, 0, &present_queue);

    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    std::vector<VkImageView> swap_image_views;
    { // Create the swap chain
        VkSurfaceFormatKHR selected_format = swap_chain_support.formats.front();
        // Prefer B8G8R8_SRGB over others, but we use a fallback.
        for (const auto &format : swap_chain_support.formats) {
            if (format.format == VK_FORMAT_B8G8R8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                selected_format = format;
                break;
            }
        }

        VkPresentModeKHR selected_present_mode = swap_chain_support.modes.front();
        for (const auto &present_mode : swap_chain_support.modes) {
            if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                selected_present_mode = present_mode;
                break;
            }
        }

        VkExtent2D selected_extent;
        if (swap_chain_support.caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            selected_extent = swap_chain_support.caps.currentExtent;
        } else {
            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);
            VkExtent2D ext = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            // Clamping based on reported capabilities of the swap chain
            ext.width = std::clamp(ext.width, swap_chain_support.caps.minImageExtent.width, swap_chain_support.caps.maxImageExtent.width);
            ext.height = std::clamp(ext.height, swap_chain_support.caps.minImageExtent.height, swap_chain_support.caps.maxImageExtent.height);

            selected_extent = ext;
        }

        // Choose image count for swap chain
        // The minimum means we might have to wait on the driver before
        // we can acquire another image to render to. Therefore request
        // at least one more image than the min.
        uint32_t image_count = swap_chain_support.caps.minImageCount + 1;

        if (swap_chain_support.caps.maxImageCount != 0) {
            image_count = std::min(image_count, swap_chain_support.caps.maxImageCount);
        }

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = surface;
        create_info.minImageCount = image_count;
        create_info.imageColorSpace = selected_format.colorSpace;
        create_info.imageFormat = selected_format.format;
        create_info.imageExtent = selected_extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Some extra configuration depending on whether the graphics and
        // present queue families are different. We must either explicitly
        // manage ownership of an image by different queues, or use the
        // concurrent sharing mode to allow shared ownership between queues which is what we do here.
        uint32_t queue_family_indices[] = {
            static_cast<uint32_t>(queue_graphics_family),
            static_cast<uint32_t>(queue_present_family)
        };

        if (queue_graphics_family != queue_present_family) {
            create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            create_info.queueFamilyIndexCount = 2;
            create_info.pQueueFamilyIndices = queue_family_indices;
        } else {
            create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            create_info.queueFamilyIndexCount = 1;
            create_info.pQueueFamilyIndices = NULL;
        }

        create_info.preTransform = swap_chain_support.caps.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = selected_present_mode;
        // e.g. if another window comes in front of the game window, this
        // will mean we actually clip those pixels and don't produce colour
        // values for them.
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = VK_NULL_HANDLE;

        if ((result = vkCreateSwapchainKHR(device, &create_info, NULL, &swap_chain)) != VK_SUCCESS) {
            std::cerr << "Failed to create swap chain: " << string_VkResult(result) << "\n";
            return 1;
        }

        std::cout << "Created swap chain\n";

        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, NULL);
        std::vector<VkImage> swap_images(image_count);
        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_images.data());

        // Create image views for our swap chain images
        swap_image_views.resize(image_count);
        for (std::size_t i = 0; i != swap_images.size(); ++i) {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = swap_images[i];

            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = selected_format.format;
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 0;

            if ((result = vkCreateImageView(device, &create_info, apiAllocCallbacks, &swap_image_views[i])) != VK_SUCCESS) {
                std::cerr << "Failed to create swap chain image view " << i << ": " << string_VkResult(result) << "\n";
                return 1;
            }
        }
    }


    // SDL event loop
    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                std::cout << "Got SDL_QUIT" << std::endl;
                quit = true;
                break;
            }
        }
    }

    std::cout << "Exiting...\n";

    for (auto &view : swap_image_views) {
        vkDestroyImageView(device, view, apiAllocCallbacks);
    }
    vkDestroySwapchainKHR(device, swap_chain, apiAllocCallbacks);
    vkDestroyDevice(device, apiAllocCallbacks);
    vkDestroySurfaceKHR(instance, surface, apiAllocCallbacks);
    vkDestroyInstance(instance, apiAllocCallbacks);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
