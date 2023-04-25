#include <iostream>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

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
    createInfo.enabledLayerCount = 0;

    const VkAllocationCallbacks *apiAllocCallbacks = nullptr;
    VkResult result = VK_SUCCESS;
    if ((result = vkCreateInstance(&createInfo, apiAllocCallbacks, &instance)) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << string_VkResult(result) << "\n";
        return 1;
    }

    delete[] sdl_ext_names;

    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);

    std::cout << "Found " << ext_count << " extensions:\n";

    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());

    for (const auto &ext : exts) {
        std::cout << "  " << ext.extensionName << "\n";
    }

    // SDL event loop
    SDL_Event e;
    bool quit = false;
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }
    }

    vkDestroyInstance(instance, nullptr);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
