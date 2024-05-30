#include <iostream>
#include <vector>
#include <map>
#include <limits>
#include <chrono>
#include <string>
#include <filesystem>
#include "Windows.h"

#include "renderer/renderer.h"
#include "renderer/renderer_helper.h"
#include "engine_helper.h"
#include "application.h"

namespace er = engine::renderer;
namespace ego = engine::game_object;

namespace {
constexpr int kWindowSizeX = 1920;
constexpr int kWindowSizeY = 1080;
static int s_update_frame_count = -1;
static bool s_render_prt_test = true;

// global pbr texture descriptor set layout.
std::shared_ptr<er::DescriptorSetLayout> createPbrLightingDescriptorSetLayout(
    const std::shared_ptr<er::Device>& device) {
    std::vector<er::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(5);

    bindings.push_back(er::helper::getTextureSamplerDescriptionSetLayoutBinding(
        GGX_LUT_INDEX,
        SET_FLAG_BIT(ShaderStage, FRAGMENT_BIT) | SET_FLAG_BIT(ShaderStage, COMPUTE_BIT)));
    bindings.push_back(er::helper::getTextureSamplerDescriptionSetLayoutBinding(
        CHARLIE_LUT_INDEX,
        SET_FLAG_BIT(ShaderStage, FRAGMENT_BIT) | SET_FLAG_BIT(ShaderStage, COMPUTE_BIT)));
    bindings.push_back(er::helper::getTextureSamplerDescriptionSetLayoutBinding(
        LAMBERTIAN_ENV_TEX_INDEX,
        SET_FLAG_BIT(ShaderStage, FRAGMENT_BIT) | SET_FLAG_BIT(ShaderStage, COMPUTE_BIT)));
    bindings.push_back(er::helper::getTextureSamplerDescriptionSetLayoutBinding(
        GGX_ENV_TEX_INDEX,
        SET_FLAG_BIT(ShaderStage, FRAGMENT_BIT) | SET_FLAG_BIT(ShaderStage, COMPUTE_BIT)));
    bindings.push_back(er::helper::getTextureSamplerDescriptionSetLayoutBinding(
        CHARLIE_ENV_TEX_INDEX,
        SET_FLAG_BIT(ShaderStage, FRAGMENT_BIT) | SET_FLAG_BIT(ShaderStage, COMPUTE_BIT)));

    return device->createDescriptorSetLayout(bindings);
}

std::shared_ptr<er::DescriptorSetLayout> createViewCameraDescriptorSetLayout(
    const std::shared_ptr<er::Device>& device) {
    std::vector<er::DescriptorSetLayoutBinding> bindings(1);
    bindings[0].binding = VIEW_CAMERA_BUFFER_INDEX;
    bindings[0].descriptor_count = 1;
    bindings[0].descriptor_type = er::DescriptorType::STORAGE_BUFFER;
    bindings[0].stage_flags =
        SET_FLAG_BIT(ShaderStage, VERTEX_BIT) |
        SET_FLAG_BIT(ShaderStage, MESH_BIT_EXT) |
        SET_FLAG_BIT(ShaderStage, FRAGMENT_BIT) |
        SET_FLAG_BIT(ShaderStage, GEOMETRY_BIT) |
        SET_FLAG_BIT(ShaderStage, COMPUTE_BIT);
    bindings[0].immutable_samplers = nullptr; // Optional

    return device->createDescriptorSetLayout(bindings);
}

}

namespace work {
namespace app {

void RealWorldApplication::run() {
    auto error_strings =
        eh::initCompileGlobalShaders(
            "src\\sim_engine\\shaders",
            "lib\\shaders",
            "src\\sim_engine\\third_parties\\vulkan_lib");
    if (error_strings.length() > 0) {
        MessageBoxA(NULL, error_strings.c_str(), "Shader Error!", MB_OK);
    }
    initWindow();
    initVulkan();
    initDrawFrame();
    mainLoop();
    cleanup();
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<work::app::RealWorldApplication*>(glfwGetWindowUserPointer(window));
    app->setFrameBufferResized(true);
}

static bool s_exit_game = false;
static bool s_game_paused = false;
static bool s_camera_paused = false;
static bool s_mouse_init = false;
static bool s_mouse_right_button_pressed = false;
static glm::vec2 s_last_mouse_pos;
static int s_key = 0;
static float s_mouse_wheel_offset = 0.0f;
const float s_camera_speed = 10.0f;

static void keyInputCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    s_key = 0;
    if (action != GLFW_RELEASE && !s_camera_paused) {
        s_key = key;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        s_exit_game = true;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_SPACE) {
        s_camera_paused = !s_camera_paused;
    }
}

void mouseInputCallback(GLFWwindow* window, double xpos, double ypos)
{
    glm::vec2 cur_mouse_pos = glm::vec2(xpos, ypos);
    if (!s_mouse_init)
    {
        s_last_mouse_pos = cur_mouse_pos;
        s_mouse_init = true;
    }

    s_last_mouse_pos = cur_mouse_pos;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS) {
            s_mouse_right_button_pressed = true;
        }
        else if (action == GLFW_RELEASE) {
            s_mouse_right_button_pressed = false;
        }
    }
}

void mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset) {
    s_mouse_wheel_offset = static_cast<float>(yoffset);
}

void RealWorldApplication::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(kWindowSizeX, kWindowSizeY, "Real World", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetKeyCallback(window_, keyInputCallback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursorPosCallback(window_, mouseInputCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetScrollCallback(window_, mouseWheelCallback);
}

void RealWorldApplication::createDepthResources(const glm::uvec2& display_size) {
    auto depth_format =
        er::Helper::findDepthFormat(device_);

    er::Helper::createDepthResources(
        device_,
        depth_format,
        display_size,
        depth_buffer_);

    er::Helper::create2DTextureImage(
        device_,
        er::Format::D32_SFLOAT,
        display_size,
        depth_buffer_copy_,
        SET_FLAG_BIT(ImageUsage, SAMPLED_BIT) |
        SET_FLAG_BIT(ImageUsage, DEPTH_STENCIL_ATTACHMENT_BIT) |
        SET_FLAG_BIT(ImageUsage, TRANSFER_DST_BIT),
        er::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
}

void RealWorldApplication::createHdrColorBuffer(const glm::uvec2& display_size) {
    er::Helper::create2DTextureImage(
        device_,
        hdr_format_,
        display_size,
        hdr_color_buffer_,
        SET_FLAG_BIT(ImageUsage, SAMPLED_BIT) |
        SET_FLAG_BIT(ImageUsage, STORAGE_BIT) |
        SET_FLAG_BIT(ImageUsage, COLOR_ATTACHMENT_BIT) |
        SET_FLAG_BIT(ImageUsage, TRANSFER_SRC_BIT),
        er::ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
}

void RealWorldApplication::createColorBufferCopy(const glm::uvec2& display_size) {
    er::Helper::create2DTextureImage(
        device_,
        hdr_format_,
        display_size,
        hdr_color_buffer_copy_,
        SET_FLAG_BIT(ImageUsage, SAMPLED_BIT) |
        SET_FLAG_BIT(ImageUsage, TRANSFER_DST_BIT),
        er::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
}

void RealWorldApplication::recreateRenderBuffer(const glm::uvec2& display_size) {
    createDepthResources(display_size);
    createHdrColorBuffer(display_size);
    createColorBufferCopy(display_size);
    createFramebuffers(display_size);
}

void RealWorldApplication::createRenderPasses() {
    assert(device_);
    final_render_pass_ = er::helper::createRenderPass(
        device_,
        swap_chain_info_.format,
        depth_format_,
        false,
        er::SampleCountFlagBits::SC_1_BIT,
        er::ImageLayout::PRESENT_SRC_KHR);
    hdr_render_pass_ = er::helper::createRenderPass(device_, hdr_format_, depth_format_, true);
    hdr_water_render_pass_ = er::helper::createRenderPass(device_, hdr_format_, depth_format_, false);
}

void RealWorldApplication::initVulkan() {
    static auto color_no_blend_attachment = er::helper::fillPipelineColorBlendAttachmentState();
    static auto color_blend_attachment =
        er::helper::fillPipelineColorBlendAttachmentState(
            SET_FLAG_BIT(ColorComponent, ALL_BITS),
            true,
            er::BlendFactor::ONE,
            er::BlendFactor::SRC_ALPHA,
            er::BlendOp::ADD,
            er::BlendFactor::ONE,
            er::BlendFactor::ZERO,
            er::BlendOp::ADD);
    static std::vector<er::PipelineColorBlendAttachmentState> color_no_blend_attachments(1, color_no_blend_attachment);
    static std::vector<er::PipelineColorBlendAttachmentState> color_blend_attachments(1, color_blend_attachment);
    static std::vector<er::PipelineColorBlendAttachmentState> cube_color_no_blend_attachments(6, color_no_blend_attachment);

    auto single_no_blend_state_info =
        std::make_shared<er::PipelineColorBlendStateCreateInfo>(
            er::helper::fillPipelineColorBlendStateCreateInfo(color_no_blend_attachments));

    auto single_blend_state_info =
        std::make_shared<er::PipelineColorBlendStateCreateInfo>(
            er::helper::fillPipelineColorBlendStateCreateInfo(color_blend_attachments));

    auto cube_no_blend_state_info =
        std::make_shared<er::PipelineColorBlendStateCreateInfo>(
            er::helper::fillPipelineColorBlendStateCreateInfo(cube_color_no_blend_attachments));

    auto cull_rasterization_info =
        std::make_shared<er::PipelineRasterizationStateCreateInfo>(
            er::helper::fillPipelineRasterizationStateCreateInfo());

    auto no_cull_rasterization_info =
        std::make_shared<er::PipelineRasterizationStateCreateInfo>(
            er::helper::fillPipelineRasterizationStateCreateInfo(
                false,
                false,
                er::PolygonMode::FILL,
                SET_FLAG_BIT(CullMode, NONE)));

    auto ms_info = std::make_shared<er::PipelineMultisampleStateCreateInfo>(
        er::helper::fillPipelineMultisampleStateCreateInfo());

    auto depth_stencil_info =
        std::make_shared<er::PipelineDepthStencilStateCreateInfo>(
            er::helper::fillPipelineDepthStencilStateCreateInfo());

    auto depth_no_write_stencil_info =
        std::make_shared<er::PipelineDepthStencilStateCreateInfo>(
            er::helper::fillPipelineDepthStencilStateCreateInfo(
                true, false));

    auto fs_depth_stencil_info =
        std::make_shared<er::PipelineDepthStencilStateCreateInfo>(
            er::helper::fillPipelineDepthStencilStateCreateInfo(
                false,
                false,
                er::CompareOp::ALWAYS));

    graphic_pipeline_info_.blend_state_info = single_no_blend_state_info;
    graphic_pipeline_info_.rasterization_info = cull_rasterization_info;
    graphic_pipeline_info_.ms_info = ms_info;
    graphic_pipeline_info_.depth_stencil_info = depth_stencil_info;

    graphic_double_face_pipeline_info_.blend_state_info = single_no_blend_state_info;
    graphic_double_face_pipeline_info_.rasterization_info = no_cull_rasterization_info;
    graphic_double_face_pipeline_info_.ms_info = ms_info;
    graphic_double_face_pipeline_info_.depth_stencil_info = depth_stencil_info;

    graphic_no_depth_write_pipeline_info_.blend_state_info = single_no_blend_state_info;
    graphic_no_depth_write_pipeline_info_.rasterization_info = cull_rasterization_info;
    graphic_no_depth_write_pipeline_info_.ms_info = ms_info;
    graphic_no_depth_write_pipeline_info_.depth_stencil_info = depth_no_write_stencil_info;
        
    graphic_fs_pipeline_info_.blend_state_info = single_no_blend_state_info;
    graphic_fs_pipeline_info_.rasterization_info = no_cull_rasterization_info;
    graphic_fs_pipeline_info_.ms_info = ms_info;
    graphic_fs_pipeline_info_.depth_stencil_info = fs_depth_stencil_info;

    graphic_fs_blend_pipeline_info_.blend_state_info = single_blend_state_info;
    graphic_fs_blend_pipeline_info_.rasterization_info = no_cull_rasterization_info;
    graphic_fs_blend_pipeline_info_.ms_info = ms_info;
    graphic_fs_blend_pipeline_info_.depth_stencil_info = fs_depth_stencil_info;

    graphic_cubemap_pipeline_info_.blend_state_info = cube_no_blend_state_info;
    graphic_cubemap_pipeline_info_.rasterization_info = no_cull_rasterization_info;
    graphic_cubemap_pipeline_info_.ms_info = ms_info;
    graphic_cubemap_pipeline_info_.depth_stencil_info = fs_depth_stencil_info;

    // the initialization order has to be strict.
    instance_ = er::Helper::createInstance();
    physical_devices_ = er::Helper::collectPhysicalDevices(instance_);
    surface_ = er::Helper::createSurface(instance_, window_);
    physical_device_ = er::Helper::pickPhysicalDevice(physical_devices_, surface_);
    queue_list_ = er::Helper::findQueueFamilies(physical_device_, surface_);
    device_ = er::Helper::createLogicalDevice(physical_device_, surface_, queue_list_);
    er::Helper::initRayTracingProperties(physical_device_, device_, rt_pipeline_properties_, as_features_);
    auto queue_list = queue_list_.getGraphicAndPresentFamilyIndex();
    assert(device_);
    depth_format_ = er::Helper::findDepthFormat(device_);
    graphics_queue_ = device_->getDeviceQueue(queue_list[0]);
    assert(graphics_queue_);
    present_queue_ = device_->getDeviceQueue(queue_list.back());
    er::Helper::createSwapChain(
        window_,
        device_,
        surface_,
        queue_list_,
        swap_chain_info_,
        SET_FLAG_BIT(ImageUsage, COLOR_ATTACHMENT_BIT)|
        SET_FLAG_BIT(ImageUsage, TRANSFER_DST_BIT));
    createRenderPasses();
    createImageViews();
    cubemap_render_pass_ =
        er::helper::createCubemapRenderPass(device_);

    pbr_lighting_desc_set_layout_ =
        createPbrLightingDescriptorSetLayout(device_);
    view_desc_set_layout_ =
        createViewCameraDescriptorSetLayout(device_);

    createCommandPool();
    assert(command_pool_);
    er::Helper::init(device_);

    eh::loadMtx2Texture(
        device_,
        cubemap_render_pass_,
        "assets/environments/doge2/lambertian/diffuse.ktx2",
        ibl_diffuse_tex_);
    eh::loadMtx2Texture(
        device_,
        cubemap_render_pass_,
        "assets/environments/doge2/ggx/specular.ktx2",
        ibl_specular_tex_);
    eh::loadMtx2Texture(
        device_,
        cubemap_render_pass_,
        "assets/environments/doge2/charlie/sheen.ktx2",
        ibl_sheen_tex_);
    recreateRenderBuffer(swap_chain_info_.extent);
    auto format = er::Format::R8G8B8A8_UNORM;
    eh::createTextureImage(device_, "assets/statue.jpg", format, sample_tex_);
    eh::createTextureImage(device_, "assets/brdfLUT.png", format, brdf_lut_tex_);
    eh::createTextureImage(device_, "assets/lut_ggx.png", format, ggx_lut_tex_);
    eh::createTextureImage(device_, "assets/lut_charlie.png", format, charlie_lut_tex_);
    eh::createTextureImage(device_, "assets/lut_thin_film.png", format, thin_film_lut_tex_);
    eh::createTextureImage(device_, "assets/map_mask.png", format, map_mask_tex_);
    eh::createTextureImage(device_, "assets/map.png", er::Format::R16_UNORM, heightmap_tex_);
//    eh::createTextureImage(device_, "assets/tile1.jpg", format, prt_base_tex_);
//    eh::createTextureImage(device_, "assets/tile1.tga", format, prt_bump_tex_);
//    eh::createTextureImage(device_, "assets/T_Mat4Mural_C.PNG", format, prt_base_tex_);
//    eh::createTextureImage(device_, "assets/T_Mat4Mural_H.PNG", format, prt_height_tex_);
//    eh::createTextureImage(device_, "assets/T_Mat4Mural_N.PNG", format, prt_normal_tex_);
//    eh::createTextureImage(device_, "assets/T_Mat4Mural_TRA.PNG", format, prt_orh_tex_);
//    eh::createTextureImage(device_, "assets/T_Mat1Ground_C.jpg", format, prt_base_tex_);
//    eh::createTextureImage(device_, "assets/T_Mat1Ground_ORH.jpg", format, prt_bump_tex_);
    eh::createTextureImage(device_, "assets/T_Mat2Mountains_C.jpg", format, prt_base_tex_);
    eh::createTextureImage(device_, "assets/T_Mat2Mountains_N.jpg", format, prt_normal_tex_);
    eh::createTextureImage(device_, "assets/T_Mat2Mountains_ORH.jpg", format, prt_orh_tex_);
    createTextureSampler();
    descriptor_pool_ = device_->createDescriptorPool();
    createCommandBuffers();
    createSyncObjects();

    auto desc_set_layouts = {
        pbr_lighting_desc_set_layout_,
        view_desc_set_layout_ };

    prt_shadow_gen_ =
        std::make_shared<es::PrtShadow>(
            device_,
            descriptor_pool_,
            texture_sampler_);

    conemap_obj_ =
        std::make_shared<ego::ConemapObj>(
            device_,
            descriptor_pool_,
            texture_sampler_,
            prt_orh_tex_,//prt_height_tex_,
            prt_shadow_gen_,
            2,//0,
            true,
            0.05f,
            0.1f,
            8.0f / 256.0f);

    unit_plane_ =
        std::make_shared<ego::Plane>(device_);

    conemap_test_ =
        std::make_shared<ego::ConemapTest>(
            device_,
            descriptor_pool_,
            hdr_render_pass_,
            graphic_pipeline_info_,
            desc_set_layouts,
            texture_sampler_,
            prt_base_tex_,
            prt_normal_tex_,
            prt_orh_tex_,
            conemap_obj_,
            swap_chain_info_.extent,
            unit_plane_);

    clear_values_.resize(2);
    clear_values_[0].color = { 50.0f / 255.0f, 50.0f / 255.0f, 50.0f / 255.0f, 1.0f };
    clear_values_[1].depth_stencil = { 1.0f, 0 };

    conemap_gen_ =
        std::make_shared<es::Conemap>(
            device_,
            descriptor_pool_,
            texture_sampler_,
            prt_orh_tex_,//prt_height_tex_,
            conemap_obj_);

    ibl_creator_ = std::make_shared<es::IblCreator>(
        device_,
        descriptor_pool_,
        cubemap_render_pass_,
        graphic_cubemap_pipeline_info_,
        texture_sampler_,
        kCubemapSize);

    for (auto& image : swap_chain_info_.images) {
        er::Helper::transitionImageLayout(
            device_,
            image,
            swap_chain_info_.format,
            er::ImageLayout::UNDEFINED,
            er::ImageLayout::PRESENT_SRC_KHR);
    }

    ego::GameCamera::initGameCameraBuffer(device_);

    ego::GameCamera::initStaticMembers(
        device_,
        descriptor_pool_,
        desc_set_layouts);

    createDescriptorSets();
}

void RealWorldApplication::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    device_->waitIdle();

    cleanupSwapChain();

    er::Helper::createSwapChain(
        window_,
        device_,
        surface_,
        queue_list_,
        swap_chain_info_,
        SET_FLAG_BIT(ImageUsage, COLOR_ATTACHMENT_BIT) |
        SET_FLAG_BIT(ImageUsage, TRANSFER_DST_BIT));

    createRenderPasses();
    createImageViews();
    auto desc_set_layouts = {
        pbr_lighting_desc_set_layout_,
        view_desc_set_layout_ };
    ego::GameCamera::recreateStaticMembers(
        device_);
    recreateRenderBuffer(swap_chain_info_.extent);
    descriptor_pool_ = device_->createDescriptorPool();
    createCommandBuffers();

    ibl_creator_->recreate(
        device_,
        descriptor_pool_,
        texture_sampler_);

    createDescriptorSets();

    for (auto& image : swap_chain_info_.images) {
        er::Helper::transitionImageLayout(
            device_,
            image,
            swap_chain_info_.format,
            er::ImageLayout::UNDEFINED,
            er::ImageLayout::PRESENT_SRC_KHR);
    }
}

void RealWorldApplication::createImageViews() {
    swap_chain_info_.image_views.resize(swap_chain_info_.images.size());
    for (uint64_t i_img = 0; i_img < swap_chain_info_.images.size(); i_img++) {
        swap_chain_info_.image_views[i_img] = device_->createImageView(
            swap_chain_info_.images[i_img],
            er::ImageViewType::VIEW_2D,
            swap_chain_info_.format,
            SET_FLAG_BIT(ImageAspect, COLOR_BIT));
    }
}

void RealWorldApplication::createFramebuffers(const glm::uvec2& display_size) {
    swap_chain_info_.framebuffers.resize(swap_chain_info_.image_views.size());
    assert(depth_buffer_.view);
    assert(final_render_pass_);
    for (uint64_t i = 0; i < swap_chain_info_.image_views.size(); i++) {
        assert(swap_chain_info_.image_views[i]);
        std::vector<std::shared_ptr<er::ImageView>> attachments(2);
        attachments[0] = swap_chain_info_.image_views[i];
        attachments[1] = depth_buffer_.view;

        swap_chain_info_.framebuffers[i] =
            device_->createFrameBuffer(final_render_pass_, attachments, display_size);
    }

    assert(hdr_render_pass_);
    assert(hdr_color_buffer_.view);
    std::vector<std::shared_ptr<er::ImageView>> attachments(2);
    attachments[0] = hdr_color_buffer_.view;
    attachments[1] = depth_buffer_.view;

    hdr_frame_buffer_ =
        device_->createFrameBuffer(hdr_render_pass_, attachments, display_size);

    assert(hdr_water_render_pass_);
    hdr_water_frame_buffer_ =
        device_->createFrameBuffer(hdr_water_render_pass_, attachments, display_size);
}

void RealWorldApplication::createCommandPool() {
    command_pool_ = device_->createCommandPool(
        queue_list_.getGraphicAndPresentFamilyIndex()[0],
        SET_FLAG_BIT(CommandPoolCreate, RESET_COMMAND_BUFFER_BIT));
}

void RealWorldApplication::createCommandBuffers() {
    command_buffers_ = 
        device_->allocateCommandBuffers(
            command_pool_,
            static_cast<uint32_t>(swap_chain_info_.framebuffers.size()));
}

void RealWorldApplication::createSyncObjects() {
    image_available_semaphores_.resize(kMaxFramesInFlight);
    render_finished_semaphores_.resize(kMaxFramesInFlight);
    in_flight_fences_.resize(kMaxFramesInFlight);
    images_in_flight_.resize(swap_chain_info_.images.size(), VK_NULL_HANDLE);
    init_semaphore_ = device_->createSemaphore();

    assert(device_);
    for (uint64_t i = 0; i < kMaxFramesInFlight; i++) {
        image_available_semaphores_[i] = device_->createSemaphore();
        render_finished_semaphores_[i] = device_->createSemaphore();
        in_flight_fences_[i] = device_->createFence(true);
    }
}

er::WriteDescriptorList RealWorldApplication::addGlobalTextures(
    const std::shared_ptr<er::DescriptorSet>& description_set)
{
    er::WriteDescriptorList descriptor_writes;
    descriptor_writes.reserve(5);
    er::Helper::addOneTexture(
        descriptor_writes,
        description_set,
        er::DescriptorType::COMBINED_IMAGE_SAMPLER,
        GGX_LUT_INDEX,
        texture_sampler_,
        ggx_lut_tex_.view,
        er::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    er::Helper::addOneTexture(
        descriptor_writes,
        description_set,
        er::DescriptorType::COMBINED_IMAGE_SAMPLER,
        CHARLIE_LUT_INDEX,
        texture_sampler_,
        charlie_lut_tex_.view,
        er::ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    ibl_creator_->addToGlobalTextures(
        descriptor_writes,
        description_set,
        texture_sampler_);

    return descriptor_writes;
}

void RealWorldApplication::createDescriptorSets() {
    auto buffer_count = swap_chain_info_.images.size();

    {
        pbr_lighting_desc_set_ =
            device_->createDescriptorSets(
                descriptor_pool_,
                pbr_lighting_desc_set_layout_,
                1)[0];

        // create a global ibl texture descriptor set.
        auto pbr_lighting_descs = addGlobalTextures(pbr_lighting_desc_set_);
        device_->updateDescriptorSets(pbr_lighting_descs);
    }

    {
        view_desc_set_ = device_->createDescriptorSets(descriptor_pool_, view_desc_set_layout_, 1)[0];
        er::WriteDescriptorList buffer_descs;
        buffer_descs.reserve(1);
        er::Helper::addOneBuffer(
            buffer_descs,
            view_desc_set_,
            er::DescriptorType::STORAGE_BUFFER,
            VIEW_CAMERA_BUFFER_INDEX,
            ego::GameCamera::getGameCameraBuffer()->buffer,
            sizeof(glsl::GameCameraInfo));
        device_->updateDescriptorSets(buffer_descs);
    }
}

void RealWorldApplication::createTextureSampler() {
    texture_sampler_ = device_->createSampler(
        er::Filter::LINEAR,
        er::SamplerAddressMode::CLAMP_TO_EDGE,
        er::SamplerMipmapMode::LINEAR, 16.0f);

    texture_point_sampler_ = device_->createSampler(
        er::Filter::NEAREST,
        er::SamplerAddressMode::REPEAT,
        er::SamplerMipmapMode::LINEAR, 16.0f);

    repeat_texture_sampler_ = device_->createSampler(
        er::Filter::LINEAR,
        er::SamplerAddressMode::REPEAT,
        er::SamplerMipmapMode::LINEAR, 16.0f);

    mirror_repeat_sampler_ = device_->createSampler(
        er::Filter::LINEAR,
        er::SamplerAddressMode::MIRRORED_REPEAT,
        er::SamplerMipmapMode::LINEAR, 16.0f);
}

void RealWorldApplication::mainLoop() {
    while (!glfwWindowShouldClose(window_) && !s_exit_game) {
        glfwPollEvents();
        drawFrame();
    }

    device_->waitIdle();
}

void RealWorldApplication::drawScene(
    std::shared_ptr<er::CommandBuffer> command_buffer,
    const er::SwapChainInfo& swap_chain_info,
    const std::shared_ptr<er::DescriptorSet>& view_desc_set,
    const glm::uvec2& screen_size,
    uint32_t image_index,
    float delta_t,
    float current_time) {

    auto frame_buffer = swap_chain_info.framebuffers[image_index];
    auto src_color = swap_chain_info.image_views[image_index];

    auto& cmd_buf = command_buffer;

    static int s_dbuf_idx = 0;

    ibl_creator_->drawEnvmapFromPanoramaImage(
        cmd_buf,
        cubemap_render_pass_,
        clear_values_,
        kCubemapSize);

    ibl_creator_->createIblDiffuseMap(
        cmd_buf,
        cubemap_render_pass_,
        clear_values_,
        kCubemapSize);

    ibl_creator_->createIblSpecularMap(
        cmd_buf,
        cubemap_render_pass_,
        clear_values_,
        kCubemapSize);

    ibl_creator_->createIblSheenMap(
        cmd_buf,
        cubemap_render_pass_,
        clear_values_,
        kCubemapSize);
 
    er::DescriptorSetList desc_sets{ pbr_lighting_desc_set_, view_desc_set };

    // this has to be happened after tile update, or you wont get the right height info.
    {
        static std::chrono::time_point s_last_time = std::chrono::steady_clock::now();
        auto cur_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = cur_time - s_last_time;
        auto delta_t = static_cast<float>(elapsed_seconds.count());
        s_last_time = cur_time;

        glsl::GameCameraParams game_camera_params;
        game_camera_params.world_min = glm::vec2(0);
        game_camera_params.inv_world_range = glm::vec2(0);
        if (s_render_prt_test) {
            game_camera_params.init_camera_pos = glm::vec3(0, 5.0f, 0);
            game_camera_params.init_camera_dir = glm::vec3(0.0f, -1.0f, 0.0f);
            game_camera_params.init_camera_up = glm::vec3(1, 0, 0);
            game_camera_params.camera_speed = 0.01f;
        }
        else {
            game_camera_params.init_camera_pos = glm::vec3(0, 500.0f, 0);
            game_camera_params.init_camera_dir = glm::vec3(1.0f, 0.0f, 0.0f);
            game_camera_params.init_camera_up = glm::vec3(0, 1, 0);
            game_camera_params.camera_speed = s_camera_speed;
        }
        game_camera_params.z_near = 0.1f;
        game_camera_params.z_far = 40000.0f;
        game_camera_params.yaw = 0.0f;
        game_camera_params.pitch = 0.0f;
        game_camera_params.camera_follow_dist = 5.0f;
        game_camera_params.key = s_key;
        game_camera_params.frame_count = s_update_frame_count;
        game_camera_params.delta_t = delta_t;
        game_camera_params.mouse_pos = s_last_mouse_pos;
        game_camera_params.fov = glm::radians(45.0f);
        game_camera_params.aspect = swap_chain_info_.extent.x / (float)swap_chain_info_.extent.y;
        game_camera_params.sensitivity = 0.2f;
        game_camera_params.num_game_objs = 0;
        game_camera_params.game_obj_idx = 0;
        game_camera_params.camera_rot_update = (!s_camera_paused && s_mouse_right_button_pressed) ? 1 : 0;
        game_camera_params.mouse_wheel_offset = s_mouse_wheel_offset;
        s_mouse_wheel_offset = 0;

        ego::GameCamera::updateGameCameraBuffer(
            cmd_buf,
            game_camera_params);

        if (s_update_frame_count >= 0) {
            s_update_frame_count++;
        }
    }

    {
        cmd_buf->beginRenderPass(
            hdr_render_pass_,
            hdr_frame_buffer_,
            screen_size,
            clear_values_);

        conemap_test_->draw(
            device_,
            cmd_buf,
            desc_sets,
            unit_plane_,
            conemap_obj_);

        cmd_buf->endRenderPass();
    }

    er::ImageResourceInfo src_info = {
        er::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
        SET_FLAG_BIT(Access, COLOR_ATTACHMENT_WRITE_BIT),
        SET_FLAG_BIT(PipelineStage, COLOR_ATTACHMENT_OUTPUT_BIT)};

    er::ImageResourceInfo dst_info = {
        er::ImageLayout::PRESENT_SRC_KHR,
        SET_FLAG_BIT(Access, COLOR_ATTACHMENT_WRITE_BIT),
        SET_FLAG_BIT(PipelineStage, COLOR_ATTACHMENT_OUTPUT_BIT) };

    er::Helper::blitImage(
        cmd_buf,
        hdr_color_buffer_.image,
        swap_chain_info.images[image_index],
        src_info,
        src_info,
        dst_info,
        dst_info,
        SET_FLAG_BIT(ImageAspect, COLOR_BIT),
        SET_FLAG_BIT(ImageAspect, COLOR_BIT),
        glm::ivec3(screen_size.x, screen_size.y, 1));

    s_dbuf_idx = 1 - s_dbuf_idx;
}

void RealWorldApplication::initDrawFrame() {
    const auto full_buffer_size =
        conemap_obj_->getConemapTexture()->size;

    // generate minmax depth buffer.
    {
        const auto& cmd_buf =
            device_->setupTransientCommandBuffer();
        conemap_obj_->update(
            cmd_buf,
            conemap_obj_->getConemapTexture()->size);
        device_->submitAndWaitTransientCommandBuffer();
    }

    // conemap generation.
    {
        auto conemap_start_point_ =
            std::chrono::high_resolution_clock::now();

        auto dispatch_block_size =
            glm::uvec2(kConemapGenBlockSizeX, kConemapGenBlockSizeY);

        auto dispatch_block_count =
            (glm::uvec2(full_buffer_size) + dispatch_block_size - glm::uvec2(1)) / dispatch_block_size;

        auto num_passes =
            dispatch_block_count.x * dispatch_block_count.y;

        const uint32_t pass_step = 4;
        for (uint32_t i_pass = 0; i_pass < num_passes; i_pass+= pass_step) {
            auto pass_end = std::min(i_pass + pass_step, num_passes);
            std::cout <<
                "conemap generation pass: " <<
                i_pass <<
                ", " <<
                pass_end <<
                " of " <<
                num_passes <<
                std::endl;
            const auto& cmd_buf =
                device_->setupTransientCommandBuffer();
            conemap_gen_->update(
                cmd_buf,
                conemap_obj_,
                i_pass,
                pass_end);
            device_->submitAndWaitTransientCommandBuffer();
        }

        auto conemap_end_point_ =
            std::chrono::high_resolution_clock::now();
        float delta_t_ =
            std::chrono::duration<float, std::chrono::seconds::period>(
                conemap_end_point_ - conemap_start_point_).count();
        std::cout << "conemap generation time: " << delta_t_ << "s" << std::endl;
    }

    // prt shadow generation.
    if (0)
    {
        auto prt_start_point_ =
            std::chrono::high_resolution_clock::now();
        const auto& prt_gen_cmd_buf =
            device_->setupTransientCommandBuffer();
        prt_shadow_gen_->update(
            prt_gen_cmd_buf,
            conemap_obj_);
        device_->submitAndWaitTransientCommandBuffer();
        auto prt_end_point_ =
            std::chrono::high_resolution_clock::now();
        delta_t_ =
            std::chrono::duration<float, std::chrono::seconds::period>(
                prt_end_point_ - prt_start_point_).count();
        std::cout << "prt generation time: " << delta_t_ << "s" << std::endl;
    }

    prt_shadow_gen_->destroy(device_);
}

void RealWorldApplication::drawFrame() {
    device_->waitForFences({ in_flight_fences_[current_frame_] });
    device_->resetFences({ in_flight_fences_[current_frame_] });

    uint32_t image_index = 0;
    bool need_recreate_swap_chain = er::Helper::acquireNextImage(
        device_,
        swap_chain_info_.swap_chain,
        image_available_semaphores_[current_frame_],
        image_index);

    if (need_recreate_swap_chain) {
        recreateSwapChain();
        return;
    }

    if (images_in_flight_[image_index] != VK_NULL_HANDLE) {
        device_->waitForFences({ images_in_flight_[image_index] });
    }
    // Mark the image as now being in use by this frame
    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

    time_t now = time(0);
    tm localtm;
    gmtime_s(&localtm, &now);

    float latitude = 37.4419f;
    float longtitude = -122.1430f; // west.

    gpu_game_camera_info_ =
        ego::GameCamera::readCameraInfo(
            device_,
            0);


    auto command_buffer = command_buffers_[image_index];
    std::vector<std::shared_ptr<er::CommandBuffer>>command_buffers(1, command_buffer);

    if (current_time_ == 0) {
        last_frame_time_point_ = std::chrono::high_resolution_clock::now();
    }

    auto current_time_point = std::chrono::high_resolution_clock::now();
    delta_t_ = std::chrono::duration<float, std::chrono::seconds::period>(
                    current_time_point - last_frame_time_point_).count();

    current_time_ += delta_t_;
    last_frame_time_point_ = current_time_point;

    command_buffer->reset(0);
    command_buffer->beginCommandBuffer(SET_FLAG_BIT(CommandBufferUsage, ONE_TIME_SUBMIT_BIT));

    drawScene(command_buffer,
        swap_chain_info_,
        view_desc_set_,
        swap_chain_info_.extent,
        image_index,
        delta_t_,
        current_time_);

    command_buffer->endCommandBuffer();

    er::Helper::submitQueue(
        graphics_queue_,
        in_flight_fences_[current_frame_],
        { image_available_semaphores_[current_frame_] },
        { command_buffer },
        { render_finished_semaphores_[current_frame_] },
        { });

    need_recreate_swap_chain = er::Helper::presentQueue(
        present_queue_,
        { swap_chain_info_.swap_chain },
        { render_finished_semaphores_[current_frame_] },
        image_index,
        framebuffer_resized_);

    if (need_recreate_swap_chain) {
        recreateSwapChain();
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
    if (s_update_frame_count < 0) {
        s_update_frame_count = 0;
    }
}

void RealWorldApplication::cleanupSwapChain() {
    assert(device_);
    depth_buffer_.destroy(device_);
    depth_buffer_copy_.destroy(device_);
    hdr_color_buffer_.destroy(device_);
    hdr_color_buffer_copy_.destroy(device_);
    device_->destroyFramebuffer(hdr_frame_buffer_);
    device_->destroyFramebuffer(hdr_water_frame_buffer_);

    for (auto framebuffer : swap_chain_info_.framebuffers) {
        device_->destroyFramebuffer(framebuffer);
    }

    device_->freeCommandBuffers(command_pool_, command_buffers_);
    device_->destroyRenderPass(final_render_pass_);
    device_->destroyRenderPass(hdr_render_pass_);
    device_->destroyRenderPass(hdr_water_render_pass_);

    for (auto image_view : swap_chain_info_.image_views) {
        device_->destroyImageView(image_view);
    }

    device_->destroySwapchain(swap_chain_info_.swap_chain);

    device_->destroyDescriptorPool(descriptor_pool_);
}

void RealWorldApplication::cleanup() {
    cleanupSwapChain();

    device_->destroyRenderPass(cubemap_render_pass_);

    assert(device_);
    device_->destroySampler(texture_sampler_);
    device_->destroySampler(mirror_repeat_sampler_);
    sample_tex_.destroy(device_);
    er::Helper::destroy(device_);
    ggx_lut_tex_.destroy(device_);
    brdf_lut_tex_.destroy(device_);
    charlie_lut_tex_.destroy(device_);
    thin_film_lut_tex_.destroy(device_);
    map_mask_tex_.destroy(device_);
    heightmap_tex_.destroy(device_);
    prt_base_tex_.destroy(device_);
    prt_normal_tex_.destroy(device_);
    prt_height_tex_.destroy(device_);
    prt_orh_tex_.destroy(device_);
    ibl_diffuse_tex_.destroy(device_);
    ibl_specular_tex_.destroy(device_);
    ibl_sheen_tex_.destroy(device_);
    device_->destroySampler(texture_sampler_);
    device_->destroySampler(texture_point_sampler_);
    device_->destroySampler(repeat_texture_sampler_);
    device_->destroySampler(mirror_repeat_sampler_);
    device_->destroyDescriptorSetLayout(view_desc_set_layout_);
    device_->destroyDescriptorSetLayout(pbr_lighting_desc_set_layout_);
    
    ego::GameCamera::destroyStaticMembers(device_);
    ibl_creator_->destroy(device_);
    unit_plane_->destroy(device_);
    conemap_obj_->destroy(device_);
    conemap_gen_->destroy(device_);
    conemap_test_->destroy(device_);

    er::helper::clearCachedShaderModules(device_);

    device_->destroySemaphore(init_semaphore_);

    for (uint64_t i = 0; i < kMaxFramesInFlight; i++) {
        device_->destroySemaphore(render_finished_semaphores_[i]);
        device_->destroySemaphore(image_available_semaphores_[i]);
        device_->destroyFence(in_flight_fences_[i]);
    }

    device_->destroyCommandPool(command_pool_);
    device_->destroy();

    instance_->destroySurface(surface_);
    instance_->destroy();

    glfwDestroyWindow(window_);
    glfwTerminate();
}

}//namespace app
}//namespace work
