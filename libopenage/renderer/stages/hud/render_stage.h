// Copyright 2023-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

#include "gamestate/types.h"
#include "time/time.h"
#include "util/path.h"

namespace openage {

namespace time {
class Clock;
}

namespace renderer {
class Renderer;
class RenderPass;
class ShaderProgram;
class Geometry;
class Texture2d;
class Window;

namespace camera {
class Camera;
}

namespace resources {
class AssetManager;
}

namespace hud {
class HudDragObject;
class HudHealthObject;
class DragRenderEntity;

} // namespace hud

namespace world {
class RenderEntity;
}

namespace hud {

/**
 * Renderer for the "Heads-Up Display" (HUD).
 * Draws UI elements that are not part of the GUI, e.g. health bars, selection boxes, minimap, etc.
 *
 */
class HudRenderStage {
public:
	/**
	 * Create a new render stage for the HUD.
	 *
	 * @param window openage window targeted for rendering.
	 * @param renderer openage low-level renderer.
	 * @param camera Camera used for the rendered scene.
	 * @param shaderdir Directory containing the shader source files.
	 * @param asset_manager Asset manager for loading resources.
	 * @param clock Simulation clock for timing animations.
	 */
	HudRenderStage(const std::shared_ptr<Window> &window,
	               const std::shared_ptr<renderer::Renderer> &renderer,
	               const std::shared_ptr<renderer::camera::Camera> &camera,
	               const util::Path &shaderdir,
	               const std::shared_ptr<renderer::resources::AssetManager> &asset_manager,
	               const std::shared_ptr<time::Clock> clock);
	~HudRenderStage() = default;

	/**
	 * Get the render pass of the HUD renderer.
	 *
	 * @return Render pass for HUD drawing.
	 */
	std::shared_ptr<renderer::RenderPass> get_render_pass();

	/**
	 * Add a new render entity for drag selection.
	 *
	 * @param render_entity New render entity.
	 */
	void add_drag_entity(const std::shared_ptr<DragRenderEntity> entity);

	/**
	 * Remove the render object for drag selection.
	 */
	void remove_drag_entity();

	/**
	 * Add a world render entity as a possible health bar source.
	 *
	 * @param render_entity World render entity.
	 */
	void add_health_entity(const std::shared_ptr<renderer::world::RenderEntity> &render_entity);

	/**
	 * Set the entities whose health bars should be visible.
	 *
	 * @param entity_ids Selected game entity IDs.
	 */
	void set_selected(const std::vector<gamestate::entity_id_t> &entity_ids);

	/**
	 * Update the render entities and render positions.
	 *
	 * @param presentation_time Override world time for replicated health bars.
	 *                          Drag selection continues to use the local clock.
	 */
	void update(std::optional<time::time_t> presentation_time = std::nullopt);

	/**
	 * Resize the FBO for the HUD rendering. This basically updates the output
	 * texture size.
	 *
	 * @param width New width of the FBO.
	 * @param height New height of the FBO.
	 */
	void resize(size_t width, size_t height);

private:
	/**
	 * Create the render pass for HUD drawing.
	 *
	 * Called during initialization of the HUD renderer.
	 *
	 * @param width Width of the FBO.
	 * @param height Height of the FBO.
	 * @param shaderdir Directory containing the shader source files.
	 */
	void initialize_render_pass(size_t width,
	                            size_t height,
	                            const util::Path &shaderdir);

	/**
	 * Reference to the openage renderer.
	 */
	std::shared_ptr<renderer::Renderer> renderer;

	/**
	 * Camera for model uniforms.
	 */
	std::shared_ptr<renderer::camera::Camera> camera;

	/**
	 * Texture manager for loading assets.
	 */
	std::shared_ptr<renderer::resources::AssetManager> asset_manager;

	/**
	 * Render pass for the HUD drawing.
	 */
	std::shared_ptr<renderer::RenderPass> render_pass;

	/**
	 * Render object for the drag select rectangle.
	 */
	std::shared_ptr<HudDragObject> drag_object;

	/**
	 * Shader for rendering the drag select rectangle.
	 */
	std::shared_ptr<renderer::ShaderProgram> drag_select_shader;

	/**
	 * Render objects that can display health bars.
	 */
	std::vector<std::shared_ptr<HudHealthObject>> health_objects;

	/**
	 * IDs of entities whose health bars are visible.
	 */
	std::unordered_set<gamestate::entity_id_t> selected_entities;

	/**
	 * Shader for rendering health bars.
	 */
	std::shared_ptr<renderer::ShaderProgram> health_bar_shader;

	/**
	 * Shared quad geometry for health bar backgrounds and fills.
	 */
	std::shared_ptr<renderer::Geometry> health_bar_geometry;

	/**
	 * Simulation clock for timing animations.
	 */
	std::shared_ptr<time::Clock> clock;

	/**
	 * Output texture.
	 */
	std::shared_ptr<renderer::Texture2d> output_texture;

	/**
	 * Depth texture.
	 */
	std::shared_ptr<renderer::Texture2d> depth_texture;

	/**
	 * Mutex for protecting threaded access.
	 */
	std::shared_mutex mutex;
};
} // namespace hud
} // namespace renderer
} // namespace openage
