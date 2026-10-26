// Copyright 2022-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

#include "coord/pixel.h"
#include "time/time.h"
#include "util/path.h"

namespace openage {

namespace time {
class Clock;
}

namespace renderer {
class Geometry;
class Renderer;
class RenderPass;
class ShaderProgram;
class Texture2d;
class Window;

namespace camera {
class Camera;
}

namespace resources {
class AssetManager;
}

namespace world {
class RenderEntity;
class WorldObject;

/**
 * Renderer for drawing and displaying entities in the game world (units, buildings, etc.)
 */
class WorldRenderStage {
public:
	/**
	 * Enable or disable frustum culling (default = false).
	 */
	static bool ENABLE_FRUSTUM_CULLING;

	/**
	 * Create a new render stage for the game world.
	 *
	 * @param window openage window targeted for rendering.
	 * @param renderer openage low-level renderer.
	 * @param camera Camera used for the rendered scene.
	 * @param shaderdir Directory containing the shader source files.
	 * @param asset_manager Asset manager for loading resources.
	 * @param clock Simulation clock for timing animations.
	 */
	WorldRenderStage(const std::shared_ptr<Window> &window,
	                 const std::shared_ptr<renderer::Renderer> &renderer,
	                 const std::shared_ptr<renderer::camera::Camera> &camera,
	                 const util::Path &shaderdir,
	                 const std::shared_ptr<renderer::resources::AssetManager> &asset_manager,
	                 const std::shared_ptr<time::Clock> clock);
	~WorldRenderStage() = default;

	/**
	 * Get the render pass of the world renderer.
	 *
	 * @return Render pass for world drawing.
	 */
	std::shared_ptr<renderer::RenderPass> get_render_pass();

	/**
	 * Add a new render entity of the world renderer.
	 *
	 * @param render_entity New render entity.
	 */
	void add_render_entity(const std::shared_ptr<RenderEntity> entity);

	/** Drop the old match's units and GPU draw calls on the render thread. */
	void clear_entities();

	/**
	 * Get the game entity under a window pixel.
	 *
	 * Transparent sprite pixels do not produce a result. If several sprites
	 * overlap, the visible sprite is returned.
	 *
	 * @param position Pixel position in window input coordinates.
	 * @param callback Called with the game entity ID, or no value if the pixel
	 *                 contains no world object.
	 */
	void pick_entity(const coord::input &position,
	                 const std::function<void(std::optional<uint32_t>)> &callback);

	/**
	 * Update the render entities and render positions.
	 *
	 * @param presentation_time Override the local clock for replicated worlds.
	 */
	void update(std::optional<time::time_t> presentation_time = std::nullopt);

	/**
	 * Resize the FBO for the world rendering. This basically updates the output
	 * texture size.
	 *
	 * @param width New width of the FBO.
	 * @param height New height of the FBO.
	 */
	void resize(size_t width, size_t height);

private:
	/**
	 * Create the render pass for world drawing.
	 *
	 * Called during initialization of the world renderer.
	 *
	 * @param width Width of the FBO.
	 * @param height Height of the FBO.
	 * @param shaderdir Directory containing the shader source files.
	 */
	void initialize_render_pass(size_t width, size_t height, const util::Path &shaderdir);

	/**
	 * Fetch the uniform IDs for the uniforms of the world shader from OpenGL
	 * and assign them to the WorldObject class.
	 *
	 * This method must be called after the shader program has been created but
	 * before any uniforms are set.
	 */
	void init_uniform_ids();

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
	 * Render pass for the world drawing.
	 */
	std::shared_ptr<renderer::RenderPass> render_pass;

	/**
	 * Render entities requested by the game world.
	 */
	std::vector<std::shared_ptr<WorldObject>> render_objects;

	/**
	 * Shader for rendering the world objects.
	 */
	std::shared_ptr<renderer::ShaderProgram> display_shader;

	/** Shader that writes opaque structure pixels to depth only. */
	std::shared_ptr<renderer::ShaderProgram> occluder_shader;

	/** Shader that draws player-colored unit contours behind structures. */
	std::shared_ptr<renderer::ShaderProgram> outline_shader;

	/**
	 * Simulation clock for timing animations.
	 */
	std::shared_ptr<time::Clock> clock;

	/**
	 * Default geometry for every world object.
	 *
	 * Since all world objects are sprites, their mesh is always quad
	 * with the same vertex info. Reusing the geometry allows us to
	 * use the same vetrex buffer for every object.
	 */
	const std::shared_ptr<renderer::Geometry> default_geometry;

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
} // namespace world
} // namespace renderer
} // namespace openage
