#include <iostream>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <chrono>

#include "engine_helper.h"
#include "game_object/camera.h"
#include "renderer/renderer_helper.h"

#include "tiny_gltf.h"
#include "tiny_mtx2.h"

namespace ego = engine::game_object;
namespace engine {

namespace {
renderer::WriteDescriptorList addGameCameraInfoBuffer(
    const std::shared_ptr<renderer::DescriptorSet>& description_set,
    const renderer::BufferInfo& game_camera_buffer) {
    renderer::WriteDescriptorList descriptor_writes;
    descriptor_writes.reserve(1);

    renderer::Helper::addOneBuffer(
        descriptor_writes,
        description_set,
        engine::renderer::DescriptorType::STORAGE_BUFFER,
        CAMERA_OBJECT_BUFFER_INDEX,
        game_camera_buffer.buffer,
        game_camera_buffer.buffer->getSize());

    return descriptor_writes;
}

static std::shared_ptr<renderer::DescriptorSetLayout> createUpdateGameCameraDescriptorSetLayout(
    const std::shared_ptr<renderer::Device>& device) {
    std::vector<renderer::DescriptorSetLayoutBinding> bindings(1);
    bindings[0] = renderer::helper::getBufferDescriptionSetLayoutBinding(
        CAMERA_OBJECT_BUFFER_INDEX,
        SET_FLAG_BIT(ShaderStage, COMPUTE_BIT),
        renderer::DescriptorType::STORAGE_BUFFER);

    return device->createDescriptorSetLayout(bindings);
}

static std::shared_ptr<renderer::PipelineLayout> createGameCameraPipelineLayout(
    const std::shared_ptr<renderer::Device>& device,
    const renderer::DescriptorSetLayoutList& desc_set_layouts) {
    renderer::PushConstantRange push_const_range{};
    push_const_range.stage_flags = SET_FLAG_BIT(ShaderStage, COMPUTE_BIT);
    push_const_range.offset = 0;
    push_const_range.size = sizeof(glsl::GameCameraParams);

    return device->createPipelineLayout(desc_set_layouts, { push_const_range });
}

} // namespace

namespace game_object {

// static member definition.
std::shared_ptr<renderer::DescriptorSet> GameCamera::update_game_camera_desc_set_;
std::shared_ptr<renderer::DescriptorSetLayout> GameCamera::update_game_camera_desc_set_layout_;
std::shared_ptr<renderer::PipelineLayout> GameCamera::update_game_camera_pipeline_layout_;
std::shared_ptr<renderer::Pipeline> GameCamera::update_game_camera_pipeline_;
std::shared_ptr<renderer::BufferInfo> GameCamera::game_camera_buffer_;

GameCamera::GameCamera(
    const std::shared_ptr<renderer::Device>& device,
    const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool) {
}

void GameCamera::createGameCameraUpdateDescSet(
    const std::shared_ptr<renderer::Device>& device,
    const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool) {
    // game objects buffer update set.
    update_game_camera_desc_set_ =
        device->createDescriptorSets(
            descriptor_pool, update_game_camera_desc_set_layout_, 1)[0];

    assert(game_camera_buffer_);
    auto write_descs = addGameCameraInfoBuffer(
        update_game_camera_desc_set_,
        *game_camera_buffer_);

    device->updateDescriptorSets(write_descs);
}

void GameCamera::initGameCameraBuffer(
    const std::shared_ptr<renderer::Device>& device) {
    if (!game_camera_buffer_) {
        game_camera_buffer_ = std::make_shared<renderer::BufferInfo>();
        device->createBuffer(
            sizeof(glsl::GameCameraInfo),
            SET_FLAG_BIT(BufferUsage, STORAGE_BUFFER_BIT),
            SET_FLAG_BIT(MemoryProperty, HOST_VISIBLE_BIT) |
            SET_FLAG_BIT(MemoryProperty, HOST_CACHED_BIT),
            0,
            game_camera_buffer_->buffer,
            game_camera_buffer_->memory);
    }
}

void GameCamera::initStaticMembers(
    const std::shared_ptr<renderer::Device>& device,
    const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool,
    const renderer::DescriptorSetLayoutList& global_desc_set_layouts) {

    if (update_game_camera_desc_set_layout_ == nullptr) {
        update_game_camera_desc_set_layout_ =
            createUpdateGameCameraDescriptorSetLayout(device);
    }

    createStaticMembers(device);

    createGameCameraUpdateDescSet(
        device,
        descriptor_pool);
}

void GameCamera::createStaticMembers(
    const std::shared_ptr<renderer::Device>& device) {

    {
        if (update_game_camera_pipeline_layout_) {
            device->destroyPipelineLayout(update_game_camera_pipeline_layout_);
            update_game_camera_pipeline_layout_ = nullptr;
        }

        if (update_game_camera_pipeline_layout_ == nullptr) {
            update_game_camera_pipeline_layout_ =
                createGameCameraPipelineLayout(
                    device,
                    { update_game_camera_desc_set_layout_ });
        }
    }

    {
        if (update_game_camera_pipeline_) {
            device->destroyPipeline(update_game_camera_pipeline_);
            update_game_camera_pipeline_ = nullptr;
        }

        if (update_game_camera_pipeline_ == nullptr) {
            assert(update_game_camera_pipeline_layout_);
            update_game_camera_pipeline_ =
                renderer::helper::createComputePipeline(
                    device,
                    update_game_camera_pipeline_layout_,
                    "update_camera_comp.spv");
        }
    }
}

void GameCamera::recreateStaticMembers(
    const std::shared_ptr<renderer::Device>& device) {

    createStaticMembers(device);
}

void GameCamera::generateDescriptorSet(
    const std::shared_ptr<renderer::Device>& device,
    const std::shared_ptr<renderer::DescriptorPool>& descriptor_pool) {

    createGameCameraUpdateDescSet(
        device,
        descriptor_pool);
}

void GameCamera::destroyStaticMembers(
    const std::shared_ptr<renderer::Device>& device) {
    game_camera_buffer_->destroy(device);
    device->destroyDescriptorSetLayout(update_game_camera_desc_set_layout_);
    device->destroyPipelineLayout(update_game_camera_pipeline_layout_);
    device->destroyPipeline(update_game_camera_pipeline_);
}

void GameCamera::updateGameCameraBuffer(
    const std::shared_ptr<renderer::CommandBuffer>& cmd_buf,
    const glsl::GameCameraParams& game_camera_params) {

    cmd_buf->bindPipeline(renderer::PipelineBindPoint::COMPUTE, update_game_camera_pipeline_);

    cmd_buf->pushConstants(
        SET_FLAG_BIT(ShaderStage, COMPUTE_BIT),
        update_game_camera_pipeline_layout_,
        &game_camera_params,
        sizeof(game_camera_params));
    assert(sizeof(game_camera_params) <= 128);

    cmd_buf->bindDescriptorSets(
        renderer::PipelineBindPoint::COMPUTE,
        update_game_camera_pipeline_layout_,
        { update_game_camera_desc_set_ });

    cmd_buf->dispatch(1, 1);

    cmd_buf->addBufferBarrier(
        game_camera_buffer_->buffer,
        { SET_FLAG_BIT(Access, SHADER_WRITE_BIT), SET_FLAG_BIT(PipelineStage, COMPUTE_SHADER_BIT) },
        { SET_FLAG_BIT(Access, SHADER_WRITE_BIT), SET_FLAG_BIT(PipelineStage, COMPUTE_SHADER_BIT) },
        game_camera_buffer_->buffer->getSize());
}

glsl::GameCameraInfo GameCamera::readCameraInfo(
    const std::shared_ptr<renderer::Device>& device,
    const uint32_t& idx) {

    glsl::GameCameraInfo camera_info;
    device->dumpBufferMemory(game_camera_buffer_->memory, sizeof(glsl::GameCameraInfo), &camera_info);

    return camera_info;
}

void GameCamera::update(
    const std::shared_ptr<renderer::Device>& device,
    const float& time) {
}

std::shared_ptr<renderer::BufferInfo> GameCamera::getGameCameraBuffer() {
    return game_camera_buffer_;
}

} // game_object
} // engine