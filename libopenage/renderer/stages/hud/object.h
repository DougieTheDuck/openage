// Copyright 2023-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "coord/pixel.h"
#include "curve/continuous.h"
#include "time/time.h"


namespace openage::renderer {
class Geometry;
class UniformInput;

namespace camera {
class Camera;
}

namespace resources {
class AssetManager;
class Animation2dInfo;
} // namespace resources

namespace world {
class RenderEntity;
}

namespace hud {
class DragRenderEntity;

/**
 * Stores the state of a renderable object in the HUD render stage.
 */
class HudDragObject {
public:
	/**
	 * Create a new object for the HUD render stage.
	 *
	 * @param asset_manager Asset manager for loading resources.
	 */
	HudDragObject(const std::shared_ptr<renderer::resources::AssetManager> &asset_manager);
	~HudDragObject() = default;

	/**
	 * Set the world render entity.
	 *
	 * @param entity New world render entity.
	 */
	void set_render_entity(const std::shared_ptr<DragRenderEntity> &entity);

	/**
	 * Set the current camera of the scene.
	 *
	 * @param camera Camera object viewing the scene.
	 */
	void set_camera(const std::shared_ptr<renderer::camera::Camera> &camera);

	/**
	 * Fetch updates from the render entity.
	 *
	 * @param time Current simulation time.
	 */
	void fetch_updates(const time::time_t &time = 0.0);

	/**
	 * Update the uniforms of the renderable associated with this object.
	 *
	 * @param time Current simulation time.
	 */
	void update_uniforms(const time::time_t &time = 0.0);

	/**
	 * Update the geometry of the renderable associated with this object.
	 *
	 * @param time Current simulation time.
	 */
	void update_geometry(const time::time_t &time = 0.0);

	/**
	 * Check whether a new renderable needs to be created for this mesh.
	 *
	 * If true, the old renderable should be removed from the render pass.
	 * The updated uniforms and geometry should be passed to this mesh.
	 * Afterwards, clear the requirement flag with \p clear_requires_renderable().
	 *
	 * @return true if a new renderable is required, else false.
	 */
	bool requires_renderable();

	/**
	 * Indicate to this mesh that a new renderable has been created.
	 */
	void clear_requires_renderable();

	/**
	 * Check whether the object was changed by \p update().
	 *
	 * @return true if changes were made, else false.
	 */
	bool is_changed();

	/**
	 * Clear the update flag by setting it to false.
	 */
	void clear_changed_flag();

	/**
	 * Set the reference to the uniform inputs of the renderable
	 * associated with this object. Relevant uniforms are updated
	 * when calling \p update().
	 *
	 * @param uniforms Uniform inputs of this object's renderable.
	 */
	void set_uniforms(const std::shared_ptr<renderer::UniformInput> &uniforms);

	/**
	 * Set the geometry of the renderable associated with this object.
	 *
	 * The geometry is updated when calling \p update().
	 *
	 * @param geometry Geometry of this object's renderable.
	 */
	void set_geometry(const std::shared_ptr<renderer::Geometry> &geometry);

private:
	/**
	 * Stores whether a new renderable for this object needs to be created
	 * for the render pass.
	 */
	bool require_renderable;

	/**
	 * Stores whether the \p update() call changed the object.
	 */
	bool changed;

	/**
	 * Camera for model uniforms.
	 */
	std::shared_ptr<renderer::camera::Camera> camera;

	/**
	 * Asset manager for central accessing and loading asset resources.
	 */
	std::shared_ptr<renderer::resources::AssetManager> asset_manager;

	/**
	 * Source for positional and texture data.
	 */
	std::shared_ptr<DragRenderEntity> render_entity;

	/**
	 * Position of the dragged corner.
	 */
	curve::Continuous<coord::input> drag_pos;

	/**
	 * Position of the start corner.
	 */
	coord::input drag_start;

	/**
	 * Shader uniforms for the renderable in the HUD render pass.
	 */
	std::shared_ptr<renderer::UniformInput> uniforms;

	/**
	 * Geometry of the renderable in the HUD render pass.
	 */
	std::shared_ptr<renderer::Geometry> geometry;

	/**
	 * Time of the last update call.
	 */
	time::time_t last_update;
};

/**
 * Stores the HUD state for a game entity health bar.
 */
class HudHealthObject {
public:
	/**
	 * Create a health bar object backed by a world render entity.
	 *
	 * @param render_entity Source for position and health data.
	 */
	explicit HudHealthObject(const std::shared_ptr<renderer::world::RenderEntity> &render_entity);
	~HudHealthObject() = default;

	/**
	 * Check whether the health bar needs renderables.
	 *
	 * @return true if renderables must be created, else false.
	 */
	bool requires_renderable();

	/**
	 * Indicate that renderables were created.
	 */
	void clear_requires_renderable();

	/**
	 * Require new renderables after the HUD render pass was cleared.
	 */
	void require_renderable();

	/**
	 * Set the background and fill uniforms used by the health bar.
	 *
	 * @param background Background uniforms.
	 * @param fill Health fill uniforms.
	 */
	void set_uniforms(const std::shared_ptr<renderer::UniformInput> &background,
	                  const std::shared_ptr<renderer::UniformInput> &fill);

	/**
	 * Update the health bar position, fill amount, and visibility.
	 *
	 * @param time Current simulation time.
	 * @param selected Whether the corresponding entity is selected.
	 */
	void update_uniforms(const time::time_t &time, bool selected);

	/**
	 * Get the ID of the corresponding game entity.
	 *
	 * @return Game entity ID.
	 */
	uint32_t get_id();

private:
	bool require_renderables;
	std::shared_ptr<renderer::world::RenderEntity> render_entity;
	std::shared_ptr<renderer::UniformInput> background_uniforms;
	std::shared_ptr<renderer::UniformInput> fill_uniforms;
};
} // namespace hud
} // namespace openage::renderer
