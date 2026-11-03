// Copyright 2023-2024 the openage authors. See copying.md for legal info.

#include "manager.h"

#include <algorithm>
#include <eigen3/Eigen/Dense>
#include <numbers>

#include "renderer/uniform_buffer.h"
#include "renderer/uniform_input.h"

namespace openage::renderer::camera {

CameraManager::CameraManager(const std::shared_ptr<renderer::camera::Camera> &camera,
                             const CameraBoundaries &camera_boundaries) :
	camera{camera},
	move_motion_directions{static_cast<int>(MoveDirection::NONE)},
	zoom_motion_direction{static_cast<int>(ZoomDirection::NONE)},
	// Scene units per second. The old fixed 0.2 per frame was tuned at
	// 60 fps and ran several times too fast on high-refresh machines.
	move_motion_speed{24.0f},
	zoom_motion_speed{0.05f},
	camera_boundaries{camera_boundaries} {
	this->uniforms = this->camera->get_uniform_buffer()->new_uniform_input(
		"view",
		camera->get_view_matrix(),
		"proj",
		camera->get_projection_matrix());
}

void CameraManager::update() {
	const auto now = std::chrono::steady_clock::now();
	float seconds = 0.0f;
	if (this->last_update) {
		// A long stall (loading, a dragged window) must not jump the view.
		seconds = std::clamp(std::chrono::duration<float>(now - *this->last_update).count(),
		                     0.0f, MAX_MOTION_STEP);
	}
	this->last_update = now;
	this->update_motion(seconds);
	this->update_uniforms();
}

void CameraManager::move_frame(MoveDirection direction, float speed) {
	switch (direction) {
	case MoveDirection::LEFT:
		// half the speed because the relationship between forward/back and
		// left/right is 1:2 in our ortho projection.
		this->camera->move_rel(Eigen::Vector3f(-1.0f, 0.0f, 1.0f), speed / 2, this->camera_boundaries);
		break;
	case MoveDirection::RIGHT:
		// half the speed because the relationship between forward/back and
		// left/right is 1:2 in our ortho projection.
		this->camera->move_rel(Eigen::Vector3f(1.0f, 0.0f, -1.0f), speed / 2, this->camera_boundaries);
		break;
	case MoveDirection::FORWARD:
		this->camera->move_rel(Eigen::Vector3f(-1.0f, 0.0f, -1.0f), speed, this->camera_boundaries);
		break;
	case MoveDirection::BACKWARD:
		this->camera->move_rel(Eigen::Vector3f(1.0f, 0.0f, 1.0f), speed, this->camera_boundaries);
		break;

	default:
		break;
	}
}

void CameraManager::zoom_frame(ZoomDirection direction, float speed) {
	switch (direction) {
	case ZoomDirection::IN:
		this->camera->zoom_in(speed);
		break;
	case ZoomDirection::OUT:
		this->camera->zoom_out(speed);
		break;

	default:
		break;
	}
}

void CameraManager::set_camera_boundaries(const CameraBoundaries &camera_boundaries) {
	this->camera_boundaries = camera_boundaries;
}

void CameraManager::update_motion(float seconds) {
	if (this->move_motion_directions != static_cast<int>(MoveDirection::NONE)) {
		Eigen::Vector3f move_dir{0.0f, 0.0f, 0.0f};

		// Sideways is halved, as in move_frame(): forward/back and left/right
		// are 1:2 on screen in our projection, so all four directions now
		// scroll at the same on-screen speed.
		if (this->move_motion_directions & static_cast<int>(MoveDirection::LEFT)) {
			move_dir += Eigen::Vector3f(-0.5f, 0.0f, 0.5f);
		}
		if (this->move_motion_directions & static_cast<int>(MoveDirection::RIGHT)) {
			move_dir += Eigen::Vector3f(0.5f, 0.0f, -0.5f);
		}
		if (this->move_motion_directions & static_cast<int>(MoveDirection::FORWARD)) {
			move_dir += Eigen::Vector3f(-1.0f, 0.0f, -1.0f);
		}
		if (this->move_motion_directions & static_cast<int>(MoveDirection::BACKWARD)) {
			move_dir += Eigen::Vector3f(1.0f, 0.0f, 1.0f);
		}

		this->camera->move_rel(move_dir, this->move_motion_speed * seconds, this->camera_boundaries);
	}

	if (this->zoom_motion_direction != static_cast<int>(ZoomDirection::NONE)) {
		if (this->zoom_motion_direction & static_cast<int>(ZoomDirection::IN)) {
			this->camera->zoom_in(this->zoom_motion_speed);
		}
		else if (this->zoom_motion_direction & static_cast<int>(ZoomDirection::OUT)) {
			this->camera->zoom_out(this->zoom_motion_speed);
		}
	}
}

void CameraManager::update_uniforms() {
	// transformation matrices
	this->uniforms->update(
		"view",
		this->camera->get_view_matrix(),
		"proj",
		this->camera->get_projection_matrix());

	// zoom scaling
	this->uniforms->update(
		"inv_zoom",
		1.0f / this->camera->get_zoom());

	auto viewport_size = this->camera->get_viewport_size();
	Eigen::Vector2f viewport_size_vec{
		1.0f / static_cast<float>(viewport_size[0]),
		1.0f / static_cast<float>(viewport_size[1])};
	this->uniforms->update("inv_viewport_size", viewport_size_vec);

	// update the uniform buffer
	this->camera->get_uniform_buffer()->update_uniforms(this->uniforms);
}

void CameraManager::set_move_motion_dirs(int directions) {
	this->move_motion_directions = directions;
}

void CameraManager::set_zoom_motion_dir(int direction) {
	this->zoom_motion_direction = direction;
}

void CameraManager::set_move_motion_speed(float speed) {
	this->move_motion_speed = speed;
}

void CameraManager::set_zoom_motion_speed(float speed) {
	this->zoom_motion_speed = speed;
}

} // namespace openage::renderer::camera
