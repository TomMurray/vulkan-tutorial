#include <iostream>

#include <SDL.h>
#include <vulkan/vulkan.h>

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
    if ((window = SDL_CreateWindow("vulkan-tutorial",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   DEFAULT_WIDTH,
                                   DEFAULT_HEIGHT,
                                   SDL_WINDOW_SHOWN |
                                   SDL_WINDOW_VULKAN)) == NULL) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << "\n";
        return 1;
    }

    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::cout << "Found " << extension_count << " extensions\n";

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
