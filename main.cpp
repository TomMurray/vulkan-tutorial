#include <iostream>

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <set>
#include <vector>

#define APP_NAME "vulkan-tutorial"

#define DEFAULT_WIDTH (800)
#define DEFAULT_HEIGHT (600)

static std::vector<char> read_bytes(const char *file_path) {
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::logic_error(std::string("Could not find file ") + file_path); 
    }
    std::vector<char> bytes(file.tellg());
    file.seekg(0);
    file.read(bytes.data(), bytes.size());
    file.close();
    return bytes;
}

static bool create_buffer(VkBuffer &b, VkDeviceMemory &mem,
    const VkDevice &device, const VkPhysicalDeviceMemoryProperties &mem_props, uint32_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bytes,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkResult result;
    if ((result = vkCreateBuffer(device, &buffer_info, nullptr, &b)) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer: " << string_VkResult(result) << "\n";
        return false;
    }

    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, b, &mem_req);


    uint32_t type_filter = mem_req.memoryTypeBits;

    uint32_t type_idx = 0;
    for (; type_idx != mem_props.memoryTypeCount; ++type_idx) {
        if ((type_filter & (1u << type_idx)) &&
            (mem_props.memoryTypes[type_idx].propertyFlags & properties) == properties) { 
            break;
        }
    }
    if (type_idx >= mem_props.memoryTypeCount) {
        std::cerr << "Failed to find a suitable memory type to allocate buffer\n";
        return false;
    }

    VkMemoryAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = type_idx,
    };

    if ((result = vkAllocateMemory(device, &alloc_info, nullptr, &mem)) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for buffer: " << string_VkResult(result) << "\n";
        return false;
    }

    // For now we don't do bind offsets.
    vkBindBufferMemory(device, b, mem, 0);

    return true;
}

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
                                   SDL_WINDOW_RESIZABLE |
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

    struct SwapChainSupport {
        VkSurfaceCapabilitiesKHR caps;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> modes;
    } swap_chain_support{};

    auto get_swap_chain_support = [&](const VkPhysicalDevice &device) {
        // Check swap-chain support
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swap_chain_support.caps);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);

        if (format_count == 0) {
            return false;
        }

        swap_chain_support.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, swap_chain_support.formats.data());


        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);

        if (present_mode_count == 0) {
            return false;
        }

        swap_chain_support.modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, swap_chain_support.modes.data());
        return true;
    };
    
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties device_memory_props;
    int queue_graphics_family = 0, queue_present_family = 0;
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

            if (!get_swap_chain_support(device)) {
                continue;
            }

            physical_device = device;
            break;
        }

        if (physical_device == VK_NULL_HANDLE) {
            std::cerr << "Failed to find a suitable VkPhysicalDevice to use" << std::endl;
            return 1;
        }

        std::cout << "Found a VkPhysicalDevice" << std::endl;

        vkGetPhysicalDeviceMemoryProperties(physical_device, &device_memory_props);

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

    // Load shader SPIR-V
    VkShaderModule vert_module = VK_NULL_HANDLE, frag_module = VK_NULL_HANDLE;
    {
        auto vert_bytes = read_bytes("../shaders/vertex.spirv");
        auto frag_bytes = read_bytes("../shaders/fragment.spirv");

        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = vert_bytes.size();
        create_info.pCode = reinterpret_cast<const uint32_t *>(vert_bytes.data());

        if ((result = vkCreateShaderModule(device, &create_info, apiAllocCallbacks, &vert_module)) != VK_SUCCESS) {
            std::cerr << "Failed to create shader module for vertex shader: " << string_VkResult(result) << "\n";
            return 1;
        }

        create_info.codeSize = frag_bytes.size();
        create_info.pCode = reinterpret_cast<const uint32_t *>(frag_bytes.data());
        if ((result = vkCreateShaderModule(device, &create_info, apiAllocCallbacks, &frag_module)) != VK_SUCCESS) {
            std::cerr << "Failed to create shader module for fragment shader: " << string_VkResult(result) << "\n";
            return 1;
        }
    }

    VkSurfaceFormatKHR selected_format = swap_chain_support.formats.front();

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    { // Create pipeline
        VkPipelineShaderStageCreateInfo vert_create_info{};
        vert_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vert_create_info.module = vert_module;
        vert_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo frag_create_info{};
        frag_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_create_info.module = frag_module;
        frag_create_info.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = {
            vert_create_info,
            frag_create_info
        };

        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        // Create description of our vertex buffer binding
        VkVertexInputBindingDescription input_binding{
            .binding = 0,
            // 5 32-bit floats, 2 for pos, 3 for colour
            .stride = 5 * 4,
            // This relates to instancing.
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        // Create descriptions of our vertex position & colour attributes
        VkVertexInputAttributeDescription input_attrs[] = {
            // Position
            {
                // Location is the location in the GLSL shader
                .location = 0,
                // Binding gives the binding slot of the vertex buffer that
                // this attribute comes from
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 0,
            },
            // Colour
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                // Offset is 2 32-bit floats or (2 * 4) = 8 bytes
                .offset = 2 * 4,
            }
        };

        VkPipelineVertexInputStateCreateInfo vertex_input{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &input_binding,
            .vertexAttributeDescriptionCount = 2,
            .pVertexAttributeDescriptions = input_attrs,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport_state {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .viewportCount = 1,
            .pViewports = NULL,
            .scissorCount = 1,
            .pScissors = NULL

        };

        VkPipelineRasterizationStateCreateInfo rasterization {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo msaa{};
        msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msaa.sampleShadingEnable = VK_FALSE;
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        msaa.minSampleShading = 1.0f;
        msaa.pSampleMask = NULL;
        msaa.alphaToCoverageEnable = VK_FALSE;
        msaa.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo color_blend_state{};
        color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state.logicOpEnable = VK_FALSE;
        color_blend_state.attachmentCount = 1;
        color_blend_state.pAttachments = &color_blend_attachment;

        VkPipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        if ((result = vkCreatePipelineLayout(device, &pipeline_layout_info, apiAllocCallbacks, &pipeline_layout)) != VK_SUCCESS) {
            std::cerr << "Failed to create pipeline layout: " << string_VkResult(result) << "\n";
            return 1;
        }

        VkAttachmentDescription color_attachment{};
        color_attachment.format = selected_format.format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Note that this relates directly to location = 0 in
        // glsl fragment shader.
        // Despite the final layout being PRESENT_SRC, we want
        // optimal colour layout in this reference.
        // Q: When is the layout transitioned. Does this happen
        // at the end of the render pass...?
        VkAttachmentReference color_ref{};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Sub-pass seems similar to command encoder scope in Metal
        VkSubpassDescription subpass{
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref
        };

        VkSubpassDependency dependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };

        VkRenderPassCreateInfo render_pass_info{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_attachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };

        if ((result = vkCreateRenderPass(device, &render_pass_info, apiAllocCallbacks, &render_pass)) != VK_SUCCESS) {
            std::cerr << "Failed to create render pass: " << string_VkResult(result) << "\n";
            return 1;
        }

        VkGraphicsPipelineCreateInfo pipeline_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = stages,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &msaa,
            .pDepthStencilState = NULL, // VkPipelineDepthStencilStateCreateInfo
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic,
            .layout = pipeline_layout,
            .renderPass = render_pass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if ((result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &graphics_pipeline)) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline: " << string_VkResult(result) << "\n";
            return 1;
        }
    }

    VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
    std::vector<VkImageView> swap_image_views;
    std::vector<VkFramebuffer> swap_framebuffers;
    VkExtent2D swap_chain_extent;
    auto create_swap_chain = [&]{
        // Refresh swap chain support info
        if (!get_swap_chain_support(physical_device)) {
            std::cerr << "Failed to get swap chain support info\n";
            return 1;
        }

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

        if (swap_chain_support.caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            swap_chain_extent = swap_chain_support.caps.currentExtent;
        } else {
            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);
            VkExtent2D ext = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            // Clamping based on reported capabilities of the swap chain
            ext.width = std::clamp(ext.width, swap_chain_support.caps.minImageExtent.width, swap_chain_support.caps.maxImageExtent.width);
            ext.height = std::clamp(ext.height, swap_chain_support.caps.minImageExtent.height, swap_chain_support.caps.maxImageExtent.height);

            swap_chain_extent = ext;
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
        create_info.imageExtent = swap_chain_extent;
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
            create_info.subresourceRange.layerCount = 1;

            if ((result = vkCreateImageView(device, &create_info, apiAllocCallbacks, &swap_image_views[i])) != VK_SUCCESS) {
                std::cerr << "Failed to create swap chain image view " << i << ": " << string_VkResult(result) << "\n";
                return 1;
            }
        }
        swap_framebuffers.resize(image_count);
        for (std::size_t i = 0; i != swap_image_views.size(); ++i) {
            VkFramebufferCreateInfo framebuffer_info{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = NULL,
                .renderPass = render_pass,
                .attachmentCount = 1,
                .pAttachments = &swap_image_views[i],
                .width = swap_chain_extent.width,
                .height = swap_chain_extent.height,
                .layers = 1
            };

            if ((result = vkCreateFramebuffer(device, &framebuffer_info, apiAllocCallbacks, &swap_framebuffers[i])) != VK_SUCCESS) {
                std::cerr << "Error creating framebuffer for swap image " << i << ": " << string_VkResult(result) << "\n";
                return 1;
            }
        }
        return 0;
    };
    if (create_swap_chain()) {
        return 1;
    }

    // create vertex buffer
    const uint32_t n_vertices = 4;
    const uint32_t bytes_per_vertex = 4 * 5;
    VkBuffer vb = VK_NULL_HANDLE;
    VkDeviceMemory vb_alloc = VK_NULL_HANDLE;
    VkBuffer vb_staging = VK_NULL_HANDLE;
    VkDeviceMemory vb_staging_alloc = VK_NULL_HANDLE;

    if (!create_buffer(vb_staging, vb_staging_alloc, device, device_memory_props, bytes_per_vertex * n_vertices, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        return 1;
    }
    // Upload the vertex data via Map
    {
        float vertex_data[] = {
            -0.5f, -0.5f, 1.0f, 1.0f, 1.0f, // Top left
             0.5f, -0.5f, 1.0f, 0.0f, 0.0f, // Top right
             0.5f,  0.5f, 0.0f, 0.0f, 1.0f, // Bottom right
            -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, // Bottom left
        };
        const std::size_t bytes = sizeof(vertex_data);
        void *ptr = nullptr;
        vkMapMemory(device, vb_staging_alloc, 0, bytes, 0, &ptr);
        std::memcpy(ptr, vertex_data, bytes);
        vkUnmapMemory(device, vb_staging_alloc);
    }
    if (!create_buffer(vb, vb_alloc, device, device_memory_props, bytes_per_vertex * n_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        return 1;
    }

    const uint32_t n_indices = 6;
    const uint32_t bytes_per_index = 2;
    VkBuffer ib, ib_staging;
    VkDeviceMemory ib_alloc, ib_staging_alloc;
    if (!create_buffer(ib_staging, ib_staging_alloc, device, device_memory_props, bytes_per_index * n_indices, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        return 1;
    }
    {
        uint16_t indices_data[] = {
            0, 1, 2, 2, 3, 0,
        };
        const std::size_t bytes = bytes_per_index * n_indices;
        void *ptr = nullptr;
        vkMapMemory(device, ib_staging_alloc, 0, bytes, 0, &ptr);
        std::memcpy(ptr, indices_data, bytes);
        vkUnmapMemory(device, ib_staging_alloc);
    }
    if (!create_buffer(ib, ib_alloc, device, device_memory_props, bytes_per_index * n_indices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        return 1;
    }


    VkCommandPool command_pool;
    { // Create the command pool

        // The RESET_COMMAND_BUFFER flag here lets us re-use the command buffer for
        // encoding after resetting it.
        VkCommandPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = static_cast<uint32_t>(queue_graphics_family),
        };

        if ((result = vkCreateCommandPool(device, &pool_info, apiAllocCallbacks, &command_pool)) != VK_SUCCESS) {
            std::cerr << "Failed to create command pool for graphics queue: " << string_VkResult(result) << "\n";
            return 1;
        }
    }

    // Setup to handle N frames in flight
    const int max_frames_in_flight = 2;

    std::vector<VkCommandBuffer> command_buffer(max_frames_in_flight);
    {
        VkCommandBufferAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = max_frames_in_flight,
        };

        if ((result = vkAllocateCommandBuffers(device, &alloc_info, command_buffer.data())) != VK_SUCCESS) {
            std::cerr << "Failed to create command buffer: " << string_VkResult(result) << "\n";
            return 1;
        }
    }

    std::vector<VkSemaphore> image_available_sem(max_frames_in_flight);
    std::vector<VkSemaphore> render_finished_sem(max_frames_in_flight);
    std::vector<VkFence> in_flight_fence(max_frames_in_flight);
    // next_frame always % max_frames_in_flight
    uint32_t next_frame = 0;
    {
        VkSemaphoreCreateInfo semaphore_info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fence_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        for (std::size_t i = 0; i != max_frames_in_flight; ++i) {
            if ((result = vkCreateSemaphore(device, &semaphore_info, apiAllocCallbacks, &image_available_sem[i])) != VK_SUCCESS) {
                std::cerr << "Failed to create semaphore: " << string_VkResult(result) << "\n";
                return 1;
            }
            if ((result = vkCreateSemaphore(device, &semaphore_info, apiAllocCallbacks, &render_finished_sem[i])) != VK_SUCCESS) {
                std::cerr << "Failed to create semaphore: " << string_VkResult(result) << "\n";
                return 1;
            }
            if ((result = vkCreateFence(device, &fence_info, apiAllocCallbacks, &in_flight_fence[i])) != VK_SUCCESS) {
                std::cerr << "Failed to create fence: " << string_VkResult(result) << "\n";
                return 1;
            }
        }
    }

    auto cleanup_swap_chain = [&]{
        for (auto &fb : swap_framebuffers) {
            vkDestroyFramebuffer(device, fb, apiAllocCallbacks);
        }

        for (auto &view : swap_image_views) {
            vkDestroyImageView(device, view, apiAllocCallbacks);
        }
        vkDestroySwapchainKHR(device, swap_chain, apiAllocCallbacks);
    };

    auto recreate_swap_chain = [&]{
        std::cout << "Recreating swap chain\n";
        vkDeviceWaitIdle(device);
        cleanup_swap_chain();
        return create_swap_chain();
    };

    // Perform initial vertex data copy to device local buffer.
    {
        VkCommandPool init_cmd_pool;
        VkCommandPoolCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            // This will only be used to produce a single command buffer,
            // and then we'll get rid of it, so use transient flag.
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = static_cast<uint32_t>(queue_graphics_family),
        };
        if ((result = vkCreateCommandPool(device, &create_info, apiAllocCallbacks, &init_cmd_pool)) != VK_SUCCESS) {
            std::cerr << "Failed to create vertex data upload command pool: " << string_VkResult(result) << "\n";
            return 1;
        }

        VkCommandBuffer cmd_buf;
        VkCommandBufferAllocateInfo cmd_buf_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = init_cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        if ((result = vkAllocateCommandBuffers(device, &cmd_buf_info, &cmd_buf)) != VK_SUCCESS) {
            std::cerr << "Failed to create vertex data upload command buffer: " << string_VkResult(result) << "\n";
            return 1;
        }

        VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL,
        };
        vkBeginCommandBuffer(cmd_buf, &begin_info);

        VkBufferCopy copy_region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = bytes_per_vertex * n_vertices,
        };

        vkCmdCopyBuffer(cmd_buf, vb_staging, vb, 1, &copy_region);
        copy_region.size = bytes_per_index * n_indices;
        vkCmdCopyBuffer(cmd_buf, ib_staging, ib, 1, &copy_region);

        vkEndCommandBuffer(cmd_buf);

        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 0,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd_buf,
            .signalSemaphoreCount = 0,
        };
        if ((result = vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE)) != VK_SUCCESS) {
            std::cerr << "Failed to submit initial buffer copy to graphics queue: " << string_VkResult(result) << "\n";
            return 1;
        }
        // Wait for everything to complete.
        vkQueueWaitIdle(graphics_queue);

        vkFreeCommandBuffers(device, init_cmd_pool, 1, &cmd_buf);
        vkDestroyCommandPool(device, init_cmd_pool, apiAllocCallbacks);

        // Clean up staging buffer as well
        vkDestroyBuffer(device, vb_staging, apiAllocCallbacks);
        vkDestroyBuffer(device, ib_staging, apiAllocCallbacks);
        vkFreeMemory(device, vb_staging_alloc, apiAllocCallbacks);
        vkFreeMemory(device, ib_staging_alloc, apiAllocCallbacks);
    }


    uint32_t image_index = 0;

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

        // Note that we need to wait for the frame in question to no longer be in-flight
        // because it would be an error for us to reset the command buffer while the
        // GPU may still be/may be about to read from it.
        vkWaitForFences(device, 1, &in_flight_fence[next_frame], VK_TRUE, std::numeric_limits<std::uint64_t>::max());

        if ((result = vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_sem[next_frame], VK_NULL_HANDLE, &image_index)) != VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                if (recreate_swap_chain()) {
                    return 1;
                }
                continue;
            } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                std::cerr << "Failed to acquire next swap chain image: " << string_VkResult(result) << "\n";
                return 1;
            }
        }

        vkResetFences(device, 1, &in_flight_fence[next_frame]);
        vkResetCommandBuffer(command_buffer[next_frame], 0);

        { // Record our command buffer!
            VkCommandBufferBeginInfo begin_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = NULL,
                .flags = 0,
                .pInheritanceInfo = NULL,
            };
            if ((result = vkBeginCommandBuffer(command_buffer[next_frame], &begin_info)) != VK_SUCCESS) {
                std::cerr << "Failed to begin command buffer: " << string_VkResult(result) << "\n";
                return 1;
            }

            VkClearValue clear_color = {{{
                0.0f, 0.0f, 0.0f, 1.0f
            }}};
            VkRenderPassBeginInfo render_pass_info{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = render_pass,
                .framebuffer = swap_framebuffers[image_index],
                .renderArea = {
                    .offset = {0, 0},
                    .extent = swap_chain_extent,
                },
                .clearValueCount = 1,
                .pClearValues = &clear_color,
            };

            // Recording the render pass in the command buffer.
            vkCmdBeginRenderPass(command_buffer[next_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(command_buffer[next_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

            VkViewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = (float) swap_chain_extent.width,
                .height = (float) swap_chain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            vkCmdSetViewport(command_buffer[next_frame], 0, 1, &viewport);

            VkRect2D scissor{
                .offset = {0, 0},
                .extent = swap_chain_extent,
            };
            vkCmdSetScissor(command_buffer[next_frame], 0, 1, &scissor);

            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(command_buffer[next_frame], 0, 1, &vb, offsets);

            vkCmdBindIndexBuffer(command_buffer[next_frame], ib, 0, VK_INDEX_TYPE_UINT16);

            // Draw 3 vertices!
            vkCmdDrawIndexed(command_buffer[next_frame], n_indices, 1, 0, 0, 0);
            
            vkCmdEndRenderPass(command_buffer[next_frame]);

            if ((result = vkEndCommandBuffer(command_buffer[next_frame])) != VK_SUCCESS) {
                std::cerr << "Failed to successfully record command buffer: " << string_VkResult(result) << "\n";
                return 1;
            }

        }


        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &image_available_sem[next_frame],
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer[next_frame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &render_finished_sem[next_frame],
        };

        if ((result = vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fence[next_frame])) != VK_SUCCESS) {
            std::cerr << "Failed to submit to queue: " << string_VkResult(result) << "\n";
            return 1;
        }

        VkPresentInfoKHR present_info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render_finished_sem[next_frame],
            .swapchainCount = 1,
            .pSwapchains = &swap_chain,
            .pImageIndices = &image_index,
            .pResults = NULL,
        };

        if ((result = vkQueuePresentKHR(present_queue, &present_info)) != VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                if (recreate_swap_chain()) {
                    return 1;
                }
            } else {
                std::cerr << "Failed to present: " << string_VkResult(result) << "\n";
                return 1;
            }
        }

        next_frame = (next_frame + 1) % max_frames_in_flight;
    }

    std::cout << "Exiting...\n";

    vkDeviceWaitIdle(device);

    for (auto &fence : in_flight_fence) {
        vkDestroyFence(device, fence, apiAllocCallbacks);
    }
    for (auto &sem : render_finished_sem) {
        vkDestroySemaphore(device, sem, apiAllocCallbacks);
    }
    for (auto &sem : image_available_sem) {
        vkDestroySemaphore(device, sem, apiAllocCallbacks);
    }
    vkDestroyCommandPool(device, command_pool, apiAllocCallbacks);

    vkDestroyBuffer(device, vb, apiAllocCallbacks);
    vkFreeMemory(device, vb_alloc, apiAllocCallbacks);

    vkDestroyPipeline(device, graphics_pipeline, apiAllocCallbacks);
    vkDestroyRenderPass(device, render_pass, apiAllocCallbacks);
    vkDestroyPipelineLayout(device, pipeline_layout, apiAllocCallbacks);

    vkDestroyShaderModule(device, vert_module, apiAllocCallbacks);
    vkDestroyShaderModule(device, frag_module, apiAllocCallbacks);

    cleanup_swap_chain();
    vkDestroyDevice(device, apiAllocCallbacks);
    vkDestroySurfaceKHR(instance, surface, apiAllocCallbacks);
    vkDestroyInstance(instance, apiAllocCallbacks);

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
