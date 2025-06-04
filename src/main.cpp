#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <optional>
#include <cstdlib>
#include <array>

#define APP_NAME "MyVKHL Template"

class VulkanContext {
public:
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue graphicsQueue;
    uint32_t graphicsQueueFamily;
    vk::CommandPool commandPool;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::ImageView> swapchainImageViews;
    vk::RenderPass renderPass;
    std::vector<vk::Framebuffer> framebuffers;
    vk::DescriptorPool descriptorPool; // for ImGui

    void init(GLFWwindow* window) {
        createInstance();
        createSurface(window);
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        createSwapchain(window);
        createImageViews();
        createRenderPass();
        createFramebuffers();
        createDescriptorPool();
    }

    void cleanup() {
        device.waitIdle();
        for (auto fb : framebuffers)
            device.destroyFramebuffer(fb);
        device.destroyRenderPass(renderPass);
        for (auto view : swapchainImageViews)
            device.destroyImageView(view);
        device.destroySwapchainKHR(swapchain);
        device.destroyDescriptorPool(descriptorPool);
        device.destroyCommandPool(commandPool);
        device.destroy();
        instance.destroySurfaceKHR(surface);
        instance.destroy();
    }

private:
    void createInstance() {
        vk::ApplicationInfo appInfo(APP_NAME, 1, "MyVKHL", 1, VK_API_VERSION_1_3);

        uint32_t count = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + count);

        vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr, extensions.size(), extensions.data());
        instance = vk::createInstance(createInfo);
    }

    void createSurface(GLFWwindow* window) {
        VkSurfaceKHR c_surface;
        if (glfwCreateWindowSurface(instance, window, nullptr, &c_surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface!");
        surface = c_surface;
    }

    void pickPhysicalDevice() {
        auto devices = instance.enumeratePhysicalDevices();
        for (auto& dev : devices) {
            auto queueFamilies = dev.getQueueFamilyProperties();
            for (uint32_t i = 0; i < queueFamilies.size(); i++) {
                if ((queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    dev.getSurfaceSupportKHR(i, surface)) {
                    physicalDevice = dev;
                    graphicsQueueFamily = i;
                    return;
                }
            }
        }
        throw std::runtime_error("failed to find suitable GPU!");
    }

    void createLogicalDevice() {
        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo({}, graphicsQueueFamily, 1, &queuePriority);
        const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        vk::DeviceCreateInfo createInfo({}, queueCreateInfo, 0, nullptr, 1, deviceExtensions);
        device = physicalDevice.createDevice(createInfo);
        graphicsQueue = device.getQueue(graphicsQueueFamily, 0);
    }

    void createCommandPool() {
        vk::CommandPoolCreateInfo poolInfo({}, graphicsQueueFamily);
        commandPool = device.createCommandPool(poolInfo);
    }

    void createSwapchain(GLFWwindow* window) {
        auto caps = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
        swapchainImageFormat = formats[0].format;

        if (caps.currentExtent.width != UINT32_MAX) {
            swapchainExtent = caps.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            swapchainExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        }
        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        vk::SwapchainCreateInfoKHR createInfo({}, surface, imageCount, swapchainImageFormat,
                                              formats[0].colorSpace, swapchainExtent, 1,
                                              vk::ImageUsageFlagBits::eColorAttachment);
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.preTransform = caps.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode = vk::PresentModeKHR::eFifo;
        createInfo.clipped = VK_TRUE;

        swapchain = device.createSwapchainKHR(createInfo);
        swapchainImages = device.getSwapchainImagesKHR(swapchain);
    }

    void createImageViews() {
        swapchainImageViews.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); i++) {
            vk::ImageViewCreateInfo viewInfo({}, swapchainImages[i], vk::ImageViewType::e2D,
                                             swapchainImageFormat, {},
                                             { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
            swapchainImageViews[i] = device.createImageView(viewInfo);
        }
    }

    void createRenderPass() {
        vk::AttachmentDescription colorAttachment({}, swapchainImageFormat, vk::SampleCountFlagBits::e1,
                                                 vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                                                 vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                                                 vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics,
                                       0, nullptr, 1, &colorAttachmentRef);

        vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         {}, vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass, 1, &dependency);
        renderPass = device.createRenderPass(renderPassInfo);
    }

    void createFramebuffers() {
        framebuffers.resize(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            vk::ImageView attachments[] = { swapchainImageViews[i] };
            vk::FramebufferCreateInfo framebufferInfo({}, renderPass, 1, attachments,
                                                     swapchainExtent.width, swapchainExtent.height, 1);
            framebuffers[i] = device.createFramebuffer(framebufferInfo);
        }
    }

    void createDescriptorPool() {
        std::array<vk::DescriptorPoolSize, 11> poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 1000),
            vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 1000)
        };

        vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                              1000 * poolSizes.size(), poolSizes.size(), poolSizes.data());
        descriptorPool = device.createDescriptorPool(poolInfo);
    }
};

void initImGui(GLFWwindow* window, VulkanContext& ctx) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = ctx.instance;
    init_info.PhysicalDevice = ctx.physicalDevice;
    init_info.Device = ctx.device;
    init_info.QueueFamily = ctx.graphicsQueueFamily;
    init_info.Queue = ctx.graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = ctx.descriptorPool;
    init_info.MinImageCount = static_cast<uint32_t>(ctx.swapchainImages.size());
    init_info.ImageCount = static_cast<uint32_t>(ctx.swapchainImages.size());

    ImGui_ImplVulkan_Init(&init_info, ctx.renderPass);

    // upload fonts
    {
        vk::CommandBufferAllocateInfo allocInfo(ctx.commandPool, vk::CommandBufferLevel::ePrimary, 1);
        vk::CommandBuffer cmd = ctx.device.allocateCommandBuffers(allocInfo)[0];
        cmd.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
        cmd.end();
        vk::SubmitInfo submitInfo({}, {}, cmd);
        ctx.graphicsQueue.submit(submitInfo, {});
        ctx.graphicsQueue.waitIdle();
        ImGui_ImplVulkan_DestroyFontUploadObjects();
        ctx.device.freeCommandBuffers(ctx.commandPool, cmd);
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, APP_NAME, nullptr, nullptr);

    VulkanContext context;
    context.init(window);

    initImGui(window, context);

    // command buffers for each framebuffer
    std::vector<vk::CommandBuffer> commandBuffers(context.framebuffers.size());
    {
        vk::CommandBufferAllocateInfo allocInfo(context.commandPool, vk::CommandBufferLevel::ePrimary,
                                                (uint32_t)commandBuffers.size());
        commandBuffers = context.device.allocateCommandBuffers(allocInfo);
    }

    std::vector<vk::Semaphore> imageAvailableSemaphores(commandBuffers.size());
    std::vector<vk::Semaphore> renderFinishedSemaphores(commandBuffers.size());
    std::vector<vk::Fence> inFlightFences(commandBuffers.size());
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        imageAvailableSemaphores[i] = context.device.createSemaphore({});
        renderFinishedSemaphores[i] = context.device.createSemaphore({});
        inFlightFences[i] = context.device.createFence({ vk::FenceCreateFlagBits::eSignaled });
    }

    size_t currentFrame = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        context.device.waitForFences(inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        context.device.resetFences(inFlightFences[currentFrame]);

        uint32_t imageIndex = context.device.acquireNextImageKHR(context.swapchain, UINT64_MAX,
                                                                 imageAvailableSemaphores[currentFrame], {}).value;

        commandBuffers[imageIndex].reset({});
        vk::CommandBufferBeginInfo beginInfo;
        commandBuffers[imageIndex].begin(beginInfo);
        std::array<vk::ClearValue,1> clearValues = { vk::ClearColorValue(std::array<float,4>{0.1f,0.1f,0.1f,1.0f}) };
        vk::RenderPassBeginInfo renderPassInfo(context.renderPass, context.framebuffers[imageIndex],
                                               {{0,0}, context.swapchainExtent}, clearValues.size(), clearValues.data());
        commandBuffers[imageIndex].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Hello, Vulkan");
        ImGui::Text("This is myvkhl template!");
        ImGui::End();

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[imageIndex]);

        commandBuffers[imageIndex].endRenderPass();
        commandBuffers[imageIndex].end();

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        vk::SubmitInfo submitInfo(imageAvailableSemaphores[currentFrame], waitStage,
                                  commandBuffers[imageIndex], renderFinishedSemaphores[currentFrame]);
        context.graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

        vk::PresentInfoKHR presentInfo(renderFinishedSemaphores[currentFrame], context.swapchain, imageIndex);
        context.graphicsQueue.presentKHR(presentInfo);

        currentFrame = (currentFrame + 1) % commandBuffers.size();
    }

    context.device.waitIdle();

    for (size_t i = 0; i < commandBuffers.size(); i++) {
        context.device.destroySemaphore(imageAvailableSemaphores[i]);
        context.device.destroySemaphore(renderFinishedSemaphores[i]);
        context.device.destroyFence(inFlightFences[i]);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    context.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

