/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "App.h"
#include "ImGuiUtils.h"
#include "TopLevelWindow.h"
#include "Utils.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "implot.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <NvPerfInit.h>
#include <NvPerfUtilities.h>
#include <nvperf_host_impl.h>
#include <stdio.h>
#include <stdlib.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>

using namespace nv::perf;
using namespace nv::perf::tool;

//#define APP_USE_UNLIMITED_FRAME_RATE

namespace nv { namespace perf { namespace tool {

    extern const unsigned char g_Font_FASolid900TTF[];
    extern const size_t g_Font_FASolid900TTF_size;

    extern const unsigned char g_Icon[];
    extern const size_t g_Icon_size;

    extern const char* const pDefaultLayout;

}}}

struct VulkanData
{
    VkAllocationCallbacks*   pAllocator = nullptr;
    VkInstance               instance = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;
    VkDevice                 device = VK_NULL_HANDLE;
    uint32_t                 graphicsQueueFamily = (uint32_t)-1;
    VkQueue                  queue = VK_NULL_HANDLE;
    VkPipelineCache          pipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool         descriptorPool = VK_NULL_HANDLE;
};

static bool InitializeVulkan(VulkanData& vulkanData);
static bool InitializeVulkanWindow(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiWindow, VkSurfaceKHR surface, int width, int height, uint32_t minImageCount);
static bool FrameRender(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiWindow, ImDrawData* pDrawData, bool& swapChainRebuild);
static bool FramePresent(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiWindow, bool& swapChainRebuild);
static void CleanupVulkan(VulkanData& vulkanData);
static void CleanupVulkanWindow(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiMainWindowData);

static void GlfwErrorLoggingCallback(int error, const char* description)
{
    NV_PERF_LOG_ERR(10, "GLFW Error %d: %s\n", error, description);
}

static void LoadFonts()
{
    constexpr float FontSizeInPixels = 13.0f;
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig defaultFontConfig;
    defaultFontConfig.SizePixels = FontSizeInPixels;
    ImFont* pDefaultFont = io.Fonts->AddFontDefault(&defaultFontConfig);
    if (!pDefaultFont)
    {
        NV_PERF_LOG_ERR(10, "Failed to load the default font");
        return;
    }
    App::Instance().SetDefaultFont(pDefaultFont);

    // Use a static array because as per ImGUI, the array must persist up until the atlas is build.
    // The range is within the Unicode Private Use Area(PUA), where FontAwesome(https://github.com/FortAwesome/Font-Awesome) uses it for icons.
    static const ImWchar s_fontRange[] = { 0xf000, 0xf3ff, 0 };
    ImFontConfig config;
    config.MergeMode = true; // FA is merged with the default font
    config.PixelSnapH = true;
    config.FontDataOwnedByAtlas = false; // since the TTF is stored in read-only data, it cannot be destroyed by ImGUI
    ImFont* pFAFont = io.Fonts->AddFontFromMemoryTTF((void*)g_Font_FASolid900TTF, (int)g_Font_FASolid900TTF_size, FontSizeInPixels, &config, s_fontRange);
    if (!pFAFont)
    {
        NV_PERF_LOG_ERR(50, "Failed to load custom font FA Solid 900.\n");
        return;
    }
    App::Instance().SetFAFont(pFAFont);
}

static float GetDpiScale(GLFWwindow* pWindow)
{
    auto pMonitor = glfwGetWindowMonitor(pWindow);
    if (!pMonitor)
    {
        pMonitor = glfwGetPrimaryMonitor();
    }
    if (!pMonitor)
    {
        return 1.0;
    }

    float x = 0.0f;
    float y = 0.0f;
    glfwGetMonitorContentScale(pMonitor, &x, &y);
    return x;
}

int main(int, char**)
{
    // Set up NvPerf's logging
    // Note NvPerf's logging is a standalone functionality implemented in the NvPerfInit.h and not dependent on the initialization of NvPerf library.
    // Therefore, it's safe to use NvPerf's logging even before initializing NvPerf.
    App::Instance().SetCwd(GetCurrentWorkingDirectory());
    constexpr size_t LoggingStorageCapacity = 512;
    InitializeNvPerfLogging(LoggingStorageCapacity, App::Instance().GetLogFileName().c_str());
    NV_PERF_LOG_INF(50, "Current Working Directory: %s\n", App::Instance().GetCwd().c_str());
    NV_PERF_LOG_INF(50, "Output log file: %s\n", nv::perf::utilities::JoinDriectoryAndFileName(App::Instance().GetCwd(), App::Instance().GetLogFileName()).c_str());

    constexpr int ErrorCode = 1;
    glfwSetErrorCallback(GlfwErrorLoggingCallback);
    if (!glfwInit())
    {
        NV_PERF_LOG_ERR(10, "Failed to initialize GLFW\n");
        return ErrorCode;
    }

    // Create window with Vulkan context
    GLFWwindow* pWindow = nullptr;
    {
        GLFWmonitor* pPrimaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* pMode = glfwGetVideoMode(pPrimaryMonitor);
        const float scale = 0.75;
        const int windowWidth = int(pMode->width * scale);
        const int windowHeight = int(pMode->height * scale);
        const char* pTitle = "Timeline Viewer";
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        pWindow = glfwCreateWindow(windowWidth, windowHeight, pTitle, nullptr, nullptr);
        if (!pWindow)
        {
            NV_PERF_LOG_ERR(10, "Failed to create window\n");
            return ErrorCode;
        }

        // Set window icon
        int imageWidth = 0;
        int imageHeight = 0;
        int imageChannels = 0;
        unsigned char* pImage = stbi_load_from_memory(g_Icon, (int)g_Icon_size, &imageWidth, &imageHeight, &imageChannels, 4); // 4 channels for RGBA
        if (pImage)
        {
            GLFWimage icon;
            icon.width = imageWidth;
            icon.height = imageHeight;
            icon.pixels = pImage;
            glfwSetWindowIcon(pWindow, 1, &icon);
        }
        else
        {
            NV_PERF_LOG_WRN(50, "Failed to load window icon\n");
        }
    }
    if (!glfwVulkanSupported())
    {
        NV_PERF_LOG_ERR(10, "GLFW does not support Vulkan\n");
        return ErrorCode;
    }

    VulkanData vulkanData{};
    if (!InitializeVulkan(vulkanData))
    {
        NV_PERF_LOG_ERR(10, "Failed to initialize Vulkan\n");
        return ErrorCode;
    }

    // Create Window Surface
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(vulkanData.instance, pWindow, vulkanData.pAllocator, &surface) != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "Failed to create window surface\n");
        return ErrorCode;
    }

    // Create Framebuffers
    ImGui_ImplVulkanH_Window imGuiWindow = {};
    const int MinImageCount = 2;
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(pWindow, &width, &height);
        if (!InitializeVulkanWindow(vulkanData, imGuiWindow, surface, width, height, MinImageCount))
        {
            NV_PERF_LOG_ERR(10, "Failed to initialize Vulkan window\n");
            return ErrorCode;
        }
    }

    // Initialize NvPerf
    {
        // Set NvPerf library's search paths to current executable's dir and the current working dir
        const std::string exeDir = nv::perf::utilities::GetCurrentExecutableDirectory();
        const char* pPaths[] = {
            exeDir.c_str(),
            App::Instance().GetCwd().c_str()
        };

        NVPW_SetLibraryLoadPaths_Params params = { NVPW_SetLibraryLoadPaths_Params_STRUCT_SIZE };
        params.numPaths = sizeof(pPaths) / sizeof(pPaths[0]);
        params.ppPaths = pPaths;
        NV_PERF_LOG_INF(50, "Setting library load paths: %s, %s\n", pPaths[0], pPaths[1]);
        if (NVPW_SetLibraryLoadPaths(&params) != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to set library load paths\n");
        }
        if (InitializeNvPerf())
        {
            App::Instance().SetNvPerfInitialized();
        }
        else
        {
            NV_PERF_LOG_ERR(10, "Failed to initialize NvPerf. Please check if the nvperf library has been placed in searchable library paths.\n");
        }
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    // Setup Dear ImGui style
    ImGuiCustomDarkTheme(); // prefer custom style over ImGui::StyleColorsDark()

    // Override the zoom key from mouse scroll only to ctrl + scroll
    {
        ImPlotInputMap& inputMap = ImPlot::GetInputMap();
        inputMap.OverrideMod = ImGuiMod_None;
        inputMap.ZoomMod = ImGuiMod_Ctrl; // ctrl + scroll to zoom
    }

    // Enable docking & viewports. Note these must be set prior to ImGui_ImplGlfw_InitForVulkan()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // enable keyboard controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // enable docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // enable multi-viewport / platform windows

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
    }

    // Setup Platform/Renderer backends
    {
        if (!ImGui_ImplGlfw_InitForVulkan(pWindow, true))
        {
            NV_PERF_LOG_ERR(10, "Failed to initialize ImGui GLFW backend for Vulkan\n");
            return ErrorCode;
        }
        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = vulkanData.instance;
        initInfo.PhysicalDevice = vulkanData.physicalDevice;
        initInfo.Device = vulkanData.device;
        initInfo.QueueFamily = vulkanData.graphicsQueueFamily;
        initInfo.Queue = vulkanData.queue;
        initInfo.PipelineCache = vulkanData.pipelineCache;
        initInfo.DescriptorPool = vulkanData.descriptorPool;
        initInfo.RenderPass = imGuiWindow.RenderPass;
        initInfo.Subpass = 0;
        initInfo.MinImageCount = MinImageCount;
        initInfo.ImageCount = imGuiWindow.ImageCount;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Allocator = vulkanData.pAllocator;
        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            NV_PERF_LOG_ERR(10, "Failed to initialize ImGui Vulkan\n");
            return ErrorCode;
        }
    }

    // Handle DPI scale
    {
        const float dpiScale = GetDpiScale(pWindow);
        NV_PERF_LOG_INF(50, "Current DPI: %.1f\n", dpiScale);
        App::Instance().SetDpiScale(dpiScale);

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(App::Instance().GetDpiScale());

        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = App::Instance().GetDpiScale();
    }

    // Load fonts
    LoadFonts();
    // Continue even if the custom font is not loaded. It will display "?", but otherwise it will ease clients to notice this error with the presence of the logging window.
    App::Instance().SetDefaultLayout(pDefaultLayout); // must be called before initializing top level window

    TopLevelWindow topLevelWindow;
    if (!topLevelWindow.Initialize())
    {
        NV_PERF_LOG_ERR(10, "Failed to initialize top level window\n");
        return ErrorCode;
    }

    bool swapChainRebuild = false;
    while (!glfwWindowShouldClose(pWindow))
    {
        glfwPollEvents();

        if (swapChainRebuild)
        {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(pWindow, &width, &height);
            if (width && height)
            {
                ImGui_ImplVulkan_SetMinImageCount(MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(vulkanData.instance, vulkanData.physicalDevice, vulkanData.device, &imGuiWindow, vulkanData.graphicsQueueFamily, vulkanData.pAllocator, width, height, MinImageCount);
                imGuiWindow.FrameIndex = 0;
                swapChainRebuild = false;
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (topLevelWindow.ShouldClose())
        {
            glfwSetWindowShouldClose(pWindow, GLFW_TRUE);
        }
        topLevelWindow.OnUpdate();
        topLevelWindow.OnRender();

        ImGui::Render();
        ImDrawData* pDrawData = ImGui::GetDrawData();
        const bool isMinimized = (pDrawData->DisplaySize.x <= 0.0f || pDrawData->DisplaySize.y <= 0.0f);
        const ImVec4 clearColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        imGuiWindow.ClearValue.color.float32[0] = clearColor.x * clearColor.w;
        imGuiWindow.ClearValue.color.float32[1] = clearColor.y * clearColor.w;
        imGuiWindow.ClearValue.color.float32[2] = clearColor.z * clearColor.w;
        imGuiWindow.ClearValue.color.float32[3] = clearColor.w;
        if (!isMinimized)
        {
            if (!FrameRender(vulkanData, imGuiWindow, pDrawData, swapChainRebuild))
            {
                NV_PERF_LOG_ERR(10, "Failed to render frame\n");
                return ErrorCode;
            }
        }

        // Update and render additional platform windows
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        if (!isMinimized)
        {
            if (!FramePresent(vulkanData, imGuiWindow, swapChainRebuild))
            {
                NV_PERF_LOG_ERR(10, "Failed to present frame\n");
                return ErrorCode;
            }
        }
    }

    topLevelWindow.Shutdown();

    // Cleanup
    if (vkDeviceWaitIdle(vulkanData.device) != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "Failed to wait for device idle\n");
    }
    ImPlot::DestroyContext();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow(vulkanData, imGuiWindow);
    CleanupVulkan(vulkanData);

    glfwDestroyWindow(pWindow);
    glfwTerminate();

    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int ret = main(0, nullptr);
    return ret;
}
#endif // _WIN32

static bool InitializeVulkan(VulkanData& vulkanData)
{
    std::vector<const char*> instanceExtensions;
    {
        uint32_t numExtensions = 0;
        const char** pGlfwExtensions = glfwGetRequiredInstanceExtensions(&numExtensions);
        instanceExtensions.reserve(numExtensions);
        for (size_t ii = 0; ii < numExtensions; ++ii)
        {
            instanceExtensions.push_back(pGlfwExtensions[ii]);
        }
    }

    auto isExtensionAvailable = [](const std::vector<VkExtensionProperties>& extensionProperties, const char* pExtension) {
        for (const VkExtensionProperties& property : extensionProperties)
        {
            if (!strcmp(property.extensionName, pExtension))
            {
                return true;
            }
        }
        return false;
    };

    VkResult vkResult = VK_SUCCESS;

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        std::vector<VkExtensionProperties> extensionProperties;
        uint32_t numProperties = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &numProperties, nullptr);
        extensionProperties.resize(numProperties);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &numProperties, extensionProperties.data()) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to enumerate instance extension properties\n");
            return false;
        }

        if (isExtensionAvailable(extensionProperties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        {
            instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (isExtensionAvailable(extensionProperties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        createInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        if (vkCreateInstance(&createInfo, vulkanData.pAllocator, &vulkanData.instance) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to create Vulkan instance\n");
            return false;
        }
    }

    // Select physical device
    {
        uint32_t numPhysicalDevices = 0;
        if (vkEnumeratePhysicalDevices(vulkanData.instance, &numPhysicalDevices, nullptr) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to enumerate physical devices\n");
            return false;
        }
        if (!numPhysicalDevices)
        {
            NV_PERF_LOG_ERR(10, "No physical devices found\n");
            return false;
        }

        std::vector<VkPhysicalDevice> physicalDevices;
        physicalDevices.resize(numPhysicalDevices);
        if (vkEnumeratePhysicalDevices(vulkanData.instance, &numPhysicalDevices, physicalDevices.data()) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to enumerate physical devices\n");
            return false;
        }

        // TODO: once this tool supports profiling, we would like to select an Nvidia GPU if available.
        for (VkPhysicalDevice& physicalDevice : physicalDevices)
        {
            VkPhysicalDeviceProperties deviceProperties = {};
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                vulkanData.physicalDevice = physicalDevice;
                break;
            }
        }

        // use the first physical device if no discrete GPU is found
        if ((vulkanData.physicalDevice == VK_NULL_HANDLE) && !physicalDevices.empty())
        {
            vulkanData.physicalDevice = physicalDevices[0];
        }

        if (vulkanData.physicalDevice == VK_NULL_HANDLE)
        {
            NV_PERF_LOG_ERR(10, "Failed to find a suitable physical device\n");
            return false;
        }
    }

    // Select graphics queue family
    {
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
        uint32_t numQueueFamilyProperties = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(vulkanData.physicalDevice, &numQueueFamilyProperties, nullptr);
        queueFamilyProperties.resize(numQueueFamilyProperties);
        vkGetPhysicalDeviceQueueFamilyProperties(vulkanData.physicalDevice, &numQueueFamilyProperties, queueFamilyProperties.data());
        for (uint32_t ii = 0; ii < numQueueFamilyProperties; ++ii)
        {
            if (queueFamilyProperties[ii].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                vulkanData.graphicsQueueFamily = ii;
                break;
            }
        }
        if (vulkanData.graphicsQueueFamily == (uint32_t)-1)
        {
            NV_PERF_LOG_ERR(10, "Failed to find a suitable queue family\n");
            return false;
        }
    }

    // Create Logical Device
    {
        std::vector<const char*> deviceExtensions;
        {
            deviceExtensions.push_back("VK_KHR_swapchain");

            uint32_t numProperties = 0;
            vkEnumerateDeviceExtensionProperties(vulkanData.physicalDevice, nullptr, &numProperties, nullptr);
            std::vector<VkExtensionProperties> deviceExtensionProperties;
            deviceExtensionProperties.resize(numProperties);
            vkEnumerateDeviceExtensionProperties(vulkanData.physicalDevice, nullptr, &numProperties, deviceExtensionProperties.data());
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (IsExtensionAvailable(deviceExtensionProperties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            {
                deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            }
#endif
        }

        constexpr float QueuePriority[] = { 1.0f };
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = vulkanData.graphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = QueuePriority;
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        if (vkCreateDevice(vulkanData.physicalDevice, &createInfo, vulkanData.pAllocator, &vulkanData.device) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to create Vulkan device\n");
            return false;
        }
        vkGetDeviceQueue(vulkanData.device, vulkanData.graphicsQueueFamily, 0, &vulkanData.queue);
    }

    // Create Descriptor Pool
    {
        constexpr VkDescriptorPoolSize PoolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        };
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = PoolSizes;
        if (vkCreateDescriptorPool(vulkanData.device, &poolInfo, vulkanData.pAllocator, &vulkanData.descriptorPool) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to create Vulkan descriptor pool\n");
            return false;
        }
    }

    return true;
}

static bool InitializeVulkanWindow(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiWindow, VkSurfaceKHR surface, int width, int height, uint32_t minImageCount)
{
    imGuiWindow.Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(vulkanData.physicalDevice, vulkanData.graphicsQueueFamily, imGuiWindow.Surface, &res);
    if (res != VK_TRUE)
    {
        NV_PERF_LOG_ERR(10, "Error no WSI support on physical device 0\n");
        return false;
    }

    // Select Surface Format
    constexpr VkFormat RequestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    constexpr VkColorSpaceKHR RequestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    imGuiWindow.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(vulkanData.physicalDevice, imGuiWindow.Surface, RequestSurfaceImageFormat, (size_t)sizeof(RequestSurfaceImageFormat) / sizeof(RequestSurfaceImageFormat[0]), RequestSurfaceColorSpace);

    // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
    constexpr VkPresentModeKHR PresentModes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    constexpr VkPresentModeKHR PresentModes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
    imGuiWindow.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(vulkanData.physicalDevice, imGuiWindow.Surface, &PresentModes[0], sizeof(PresentModes) / sizeof(PresentModes[0]));

    // Create SwapChain, RenderPass, Framebuffer, etc.
    ImGui_ImplVulkanH_CreateOrResizeWindow(vulkanData.instance, vulkanData.physicalDevice, vulkanData.device, &imGuiWindow, vulkanData.graphicsQueueFamily, vulkanData.pAllocator, width, height, minImageCount);
    return true;
}

static void CleanupVulkan(VulkanData& vulkanData)
{
    vkDestroyDescriptorPool(vulkanData.device, vulkanData.descriptorPool, vulkanData.pAllocator);
    vkDestroyDevice(vulkanData.device, vulkanData.pAllocator);
    vkDestroyInstance(vulkanData.instance, vulkanData.pAllocator);
}

static void CleanupVulkanWindow(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiMainWindowData)
{
    ImGui_ImplVulkanH_DestroyWindow(vulkanData.instance, vulkanData.device, &imGuiMainWindowData, vulkanData.pAllocator);
}

static bool FrameRender(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiWindow, ImDrawData* pDrawData, bool& swapChainRebuild)
{
    VkResult vkResult = VK_SUCCESS;

    VkSemaphore imageAcquiredSemaphore  = imGuiWindow.FrameSemaphores[imGuiWindow.SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore renderCompleteSemaphore = imGuiWindow.FrameSemaphores[imGuiWindow.SemaphoreIndex].RenderCompleteSemaphore;
    vkResult = vkAcquireNextImageKHR(vulkanData.device, imGuiWindow.Swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &imGuiWindow.FrameIndex);
    if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
    {
        swapChainRebuild = true;
        return true;
    }
    if (vkResult != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "Failed to acquire next image\n");
        return false;
    }

    ImGui_ImplVulkanH_Frame* fd = &imGuiWindow.Frames[imGuiWindow.FrameIndex];
    {
        if (vkWaitForFences(vulkanData.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)    // wait indefinitely instead of periodically checking
        {
            NV_PERF_LOG_ERR(10, "Failed to wait for fence\n");
            return false;
        }

        if (vkResetFences(vulkanData.device, 1, &fd->Fence) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to reset fence\n");
            return false;
        }
    }
    {
        if (vkResetCommandPool(vulkanData.device, fd->CommandPool, 0) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to reset command pool\n");
            return false;
        }
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(fd->CommandBuffer, &info) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to begin command buffer\n");
            return false;
        }
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = imGuiWindow.RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = imGuiWindow.Width;
        info.renderArea.extent.height = imGuiWindow.Height;
        info.clearValueCount = 1;
        info.pClearValues = &imGuiWindow.ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(pDrawData, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        constexpr VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &imageAcquiredSemaphore;
        info.pWaitDstStageMask = &WaitStage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &renderCompleteSemaphore;

        if (vkEndCommandBuffer(fd->CommandBuffer) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to end command buffer\n");
            return false;
        }
        if (vkQueueSubmit(vulkanData.queue, 1, &info, fd->Fence) != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to submit queue\n");
            return false;
        }
    }
    return true;
}

static bool FramePresent(VulkanData& vulkanData, ImGui_ImplVulkanH_Window& imGuiWindow, bool& swapChainRebuild)
{
    if (swapChainRebuild)
    {
        return true;
    }

    VkSemaphore renderCompleteSemaphore = imGuiWindow.FrameSemaphores[imGuiWindow.SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &renderCompleteSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &imGuiWindow.Swapchain;
    info.pImageIndices = &imGuiWindow.FrameIndex;
    VkResult vkResult = vkQueuePresentKHR(vulkanData.queue, &info);
    if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
    {
        swapChainRebuild = true;
        return true;
    }
    if (vkResult != VK_SUCCESS)
    {
        NV_PERF_LOG_ERR(10, "Failed to present queue\n");
        return false;
    }
    imGuiWindow.SemaphoreIndex = (imGuiWindow.SemaphoreIndex + 1) % imGuiWindow.SemaphoreCount; // Now we can use the next set of semaphores
    return true;
}