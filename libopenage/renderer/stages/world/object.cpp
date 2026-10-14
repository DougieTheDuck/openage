// Copyright 2022-2024 the openage authors. See copying.md for legal info.

#include "object.h"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

#include <eigen3/Eigen/Dense>

#include "renderer/camera/camera.h"
#include "renderer/camera/frustum_2d.h"
#include "renderer/definitions.h"
#include "renderer/resources/animation/angle_info.h"
#include "renderer/resources/animation/animation_info.h"
#include "renderer/resources/animation/frame_info.h"
#include "renderer/resources/animation/layer_info.h"
#include "renderer/resources/assets/asset_manager.h"
#include "renderer/resources/assets/texture_manager.h"
#include "renderer/resources/frame_timing.h"
#include "renderer/resources/mesh_data.h"
#include "renderer/resources/texture_data.h"
#include "renderer/resources/texture_info.h"
#include "renderer/resources/texture_subinfo.h"
#include "renderer/stages/world/render_entity.h"
#include "renderer/uniform_input.h"
#include "util/fixed_point.h"
#include "util/vector.h"


namespace openage::renderer::world {

WorldObject::WorldObject(const std::shared_ptr<renderer::resources::AssetManager> &asset_manager) :
	require_renderable{true},
	changed{false},
	asset_manager{asset_manager},
	render_entity{nullptr},
	ref_id{0},
	position{nullptr, 0, "", nullptr, SCENE_ORIGIN},
	angle{nullptr, 0, "", nullptr, 0},
	animation_info{nullptr, 0},
	layer_uniforms{},
	rendered_position{SCENE_ORIGIN},
	rendered_layers{},
	last_update{0.0} {
}

void WorldObject::set_render_entity(const std::shared_ptr<RenderEntity> &entity) {
	this->render_entity = entity;
	this->fetch_updates();
}

void WorldObject::fetch_updates(const time::time_t &time) {
	// TODO: Calling this once per frame is very expensive
	auto layer_count = this->get_required_layer_count(time);
	if (this->layer_uniforms.size() != layer_count) {
		// The number of layers changed, so we need to update the renderables
		this->require_renderable = true;
	}

	if (not this->render_entity->is_changed()) {
		// exit early because there is nothing to update
		return;
	}

	// Get data from render entity
	this->ref_id = this->render_entity->get_id();

	// Thread-safe access to curves needs a lock on the render entity's mutex
	auto read_lock = this->render_entity->get_read_lock();
	this->position.sync(this->render_entity->get_position());
	this->animation_info.sync(this->render_entity->get_animation_path(),
	                          std::function<std::shared_ptr<renderer::resources::Animation2dInfo>(const std::string &)>(
								  [&](const std::string &path) {
									  if (path.empty()) {
										  auto placeholder = this->asset_manager->get_placeholder_animation();
										  if (placeholder) {
											  return (*placeholder).second;
										  }
										  return std::shared_ptr<renderer::resources::Animation2dInfo>{nullptr};
									  }
									  return this->asset_manager->request_animation(path);
								  }),
	                          this->last_update);
	this->angle.sync(this->render_entity->get_angle(), this->last_update);

	// Unlock mutex of the render entity
	read_lock.unlock();

	// Set self to changed so that world renderer can update the renderable
	this->changed = true;
	this->render_entity->clear_changed_flag();
	this->last_update = time;
}

void WorldObject::update_uniforms(const time::time_t &time) {
	// TODO: Only update uniforms that changed since last update
	if (this->layer_uniforms.empty()) [[unlikely]] {
		return;
	}

	// Object world position
	auto current_pos = this->position.get(time);
	this->rendered_position = current_pos;

	// Direction angle the object is facing towards currently
	auto angle_degrees = this->angle.get(time).to_float();

	// Animation information
	auto [last_update, animation_info] = this->animation_info.frame(time);

	this->rendered_layers.clear();
	this->rendered_layers.reserve(this->layer_uniforms.size());
	for (size_t layer_idx = 0; layer_idx < this->layer_uniforms.size(); ++layer_idx) {
		auto &layer_unifs = this->layer_uniforms.at(layer_idx);
		layer_unifs->update(this->obj_world_position, current_pos.to_world_space());

		// Frame subtexture
		auto &layer = animation_info->get_layer(layer_idx);
		auto &angle = layer.get_direction_angle(angle_degrees);

		// Flip subtexture horizontally if angle is mirrored
		auto flip_x = angle->is_mirrored();
		if (flip_x) {
			layer_unifs->update(this->flip_x, true);
		}
		else {
			layer_unifs->update(this->flip_x, false);
		}

		// Current frame index considering current time
		size_t frame_idx;
		switch (layer.get_display_mode()) {
		case renderer::resources::display_mode::ONCE:
		case renderer::resources::display_mode::LOOP: {
			// ONCE and LOOP are animated based on time
			auto &timing = layer.get_frame_timing();
			frame_idx = timing->get_frame(time, last_update);
		} break;
		case renderer::resources::display_mode::OFF:
		default:
			// OFF only shows the first frame
			frame_idx = 0;
			break;
		}

		// Index of texture and subtexture where the frame's pixels are located
		auto &frame_info = angle->get_frame(frame_idx);
		auto tex_idx = frame_info->get_texture_idx();
		auto subtex_idx = frame_info->get_subtexture_idx();

		auto &tex_info = animation_info->get_texture(tex_idx);
		auto &tex_manager = this->asset_manager->get_texture_manager();
		auto &texture = tex_manager->request(tex_info->get_image_path().value());
		auto &alpha_mask = tex_manager->request_mask(tex_info->get_image_path().value());
		layer_unifs->update(this->tex, texture);
		layer_unifs->update(this->cbits, tex_info->get_cbits());

		// Subtexture coordinates.inside texture
		auto coords = tex_info->get_subtex_info(subtex_idx).get_subtex_coords();
		layer_unifs->update(this->tile_params, coords);

		// Animation scale factor
		// Scales the subtex up or down in the shader
		auto scale = animation_info->get_scalefactor();
		layer_unifs->update(this->scale, scale);

		// Subtexture size in pixels
		auto subtex_size = tex_info->get_subtex_info(subtex_idx).get_size();
		Eigen::Vector2f subtex_size_vec{
			static_cast<float>(subtex_size[0]),
			static_cast<float>(subtex_size[1])};
		layer_unifs->update(this->subtex_size, subtex_size_vec);

		// Anchor point offset (in pixels)
		// moves the subtex in the shader so that the anchor point is at the object's position
		auto anchor = tex_info->get_subtex_info(subtex_idx).get_anchor_params();
		Eigen::Vector2f anchor_offset{
			static_cast<float>(anchor[0]),
			static_cast<float>(anchor[1])};
		layer_unifs->update(this->anchor_offset, anchor_offset);

		this->rendered_layers.push_back(RenderedLayer{
			alpha_mask,
			tex_info->get_subtex_info(subtex_idx).get_pos(),
			subtex_size,
			anchor,
			scale,
			flip_x,
			false,
			layer.get_position(),
		});
	}
}

uint32_t WorldObject::get_id() {
	return this->ref_id;
}

const renderer::resources::MeshData WorldObject::get_mesh() {
	return resources::MeshData::make_quad();
}

const Eigen::Matrix4f WorldObject::get_model_matrix() {
	return Eigen::Matrix4f::Identity();
}

bool WorldObject::requires_renderable() const {
	return this->require_renderable;
}

void WorldObject::clear_requires_renderable() {
	this->require_renderable = false;
}

size_t WorldObject::get_required_layer_count(const time::time_t &time) const {
	auto animation_info = this->animation_info.get(time);
	if (not animation_info) {
		return 0;
	}

	return animation_info->get_layer_count();
}

std::vector<size_t> WorldObject::get_layer_positions(const time::time_t &time) const {
	auto animation_info = this->animation_info.get(time);
	if (not animation_info) {
		return {};
	}

	std::vector<size_t> positions;
	for (size_t i = 0; i < animation_info->get_layer_count(); ++i) {
		auto layer = animation_info->get_layer(i);
		positions.push_back(layer.get_position());
	}

	return positions;
}

bool WorldObject::is_changed() {
	return this->changed;
}

void WorldObject::clear_changed_flag() {
	this->changed = false;
}

void WorldObject::set_uniforms(std::vector<std::shared_ptr<renderer::UniformInput>> &&uniforms) {
	this->layer_uniforms = std::move(uniforms);
}

bool WorldObject::is_visible(const camera::Frustum2d &frustum,
                             const time::time_t &time) {
	static const Eigen::Matrix4f model_matrix = this->get_model_matrix();
	Eigen::Vector3f current_pos = this->position.get(time).to_world_space();
	auto animation_info = this->animation_info.get(time);
	return frustum.in_frustum(current_pos,
	                          model_matrix,
	                          animation_info->get_scalefactor(),
	                          animation_info->get_max_bounds());
}

std::optional<WorldObject::HitTestResult> WorldObject::hit_test(
	const coord::input &position,
	const std::shared_ptr<renderer::camera::Camera> &camera) const {
	auto viewport = camera->get_viewport_size();
	if (viewport[0] == 0 or viewport[1] == 0) {
		return std::nullopt;
	}

	Eigen::Vector2f cursor_ndc{
		2.0f * static_cast<float>(position.x) / static_cast<float>(viewport[0]) - 1.0f,
		1.0f - 2.0f * static_cast<float>(position.y) / static_cast<float>(viewport[1])};
	auto world_position = this->rendered_position.to_world_space();
	Eigen::Vector4f clip_position = camera->get_projection_matrix()
	                                * camera->get_view_matrix()
	                                * Eigen::Vector4f{world_position.x(), world_position.y(), world_position.z(), 1.0f};
	clip_position /= clip_position.w();

	std::optional<HitTestResult> result;
	for (const auto &layer : this->rendered_layers) {
		auto zoom_scale = layer.scale / camera->get_zoom();
		Eigen::Vector2f sprite_scale{
			zoom_scale * static_cast<float>(layer.subtexture_size.x()) / static_cast<float>(viewport[0]),
			zoom_scale * static_cast<float>(layer.subtexture_size.y()) / static_cast<float>(viewport[1])};
		Eigen::Vector2f anchor_scale{
			zoom_scale * static_cast<float>(layer.anchor_offset.x()) / static_cast<float>(viewport[0]),
			zoom_scale * static_cast<float>(layer.anchor_offset.y()) / static_cast<float>(viewport[1])};
		if (layer.flip_x) {
			anchor_scale.x() *= -1.0f;
		}
		if (layer.flip_y) {
			anchor_scale.y() *= -1.0f;
		}

		Eigen::Vector2f local{
			(cursor_ndc.x() - clip_position.x() - anchor_scale.x()) / sprite_scale.x(),
			(cursor_ndc.y() - clip_position.y() - anchor_scale.y()) / sprite_scale.y()};
		if (local.x() < -1.0f or local.x() >= 1.0f
		    or local.y() < -1.0f or local.y() >= 1.0f) {
			continue;
		}

		float u = (local.x() + 1.0f) * 0.5f;
		float v = (local.y() + 1.0f) * 0.5f;
		if (layer.flip_x) {
			u = 1.0f - u;
		}
		if (not layer.flip_y) {
			v = 1.0f - v;
		}

		auto pixel_x = static_cast<size_t>(layer.subtexture_pos.x())
		               + static_cast<size_t>(u * static_cast<float>(layer.subtexture_size.x()));
		auto pixel_y = static_cast<size_t>(layer.subtexture_pos.y())
		               + static_cast<size_t>(v * static_cast<float>(layer.subtexture_size.y()));
		if (layer.alpha_mask->is_opaque(pixel_x, pixel_y)
		    and (not result.has_value() or layer.layer_position >= result->layer_position)) {
			result = HitTestResult{clip_position.z(), layer.layer_position};
		}
	}

	return result;
}

} // namespace openage::renderer::world
