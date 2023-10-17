#pragma once
#include <unordered_map>
#include "renderer/renderer.h"
#include "shaders/global_definition.glsl.h"

namespace engine {
namespace game_object {

class GameCamera {

    static std::shared_ptr<renderer::DescriptorSet> update_game_camera_desc_set_;
    static std::shared_ptr<renderer::DescriptorSetLayout> update_game_camera_desc_set_layout_;
    static std::shared_ptr<renderer::PipelineLayout> update_game_camera_pipeline_layout_;
    static std::shared_ptr<renderer::Pipeline> update_game_camera_pipeline_;
    static std::shared_ptr<renderer::BufferInfo> game_camera_buffer_;

public:
    GameCamera() = delete;
    GameCamera(
        const std::shared_ptr<renderer::Device>& device,
        const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool);

    void update(const std::shared_ptr<renderer::Device>& device, const float& time);

    static void createGameCameraUpdateDescSet(
        const std::shared_ptr<renderer::Device>& device,
        const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool);

    static void initGameCameraBuffer(
        const std::shared_ptr<renderer::Device>& device);

    static void initStaticMembers(
        const std::shared_ptr<renderer::Device>& device,
        const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool,
        const renderer::DescriptorSetLayoutList& global_desc_set_layouts);

    static void createStaticMembers(
        const std::shared_ptr<renderer::Device>& device);

    static void recreateStaticMembers(
        const std::shared_ptr<renderer::Device>& device);

    static void generateDescriptorSet(
        const std::shared_ptr<renderer::Device>& device,
        const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool);

    static void destroyStaticMembers(
        const std::shared_ptr<renderer::Device>& device);

    static void updateGameCameraBuffer(
        const std::shared_ptr<renderer::CommandBuffer>& cmd_buf,
        const glsl::GameCameraParams& game_camera_params);

    static glsl::GameCameraInfo readCameraInfo(
        const std::shared_ptr<renderer::Device>& device,
        const uint32_t& idx);

    static std::shared_ptr<renderer::BufferInfo> getGameCameraBuffer();
};

} // namespace game_object
} // namespace engine