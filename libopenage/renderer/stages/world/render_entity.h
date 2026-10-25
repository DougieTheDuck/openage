// Copyright 2022-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "coord/phys.h"
#include "coord/scene.h"
#include "curve/continuous.h"
#include "curve/discrete.h"
#include "curve/segmented.h"
#include "renderer/stages/render_entity.h"


namespace openage::renderer::world {

/**
 * Render entity for pushing updates to the World renderer.
 */
class RenderEntity final : public renderer::RenderEntity {
public:
	/**
	 * Create a world render entity.
	 *
	 * @param pickable Whether pixel picking may return this entity.
	 */
	explicit RenderEntity(bool pickable = true);
	~RenderEntity() = default;

	/**
	 * Update the render entity with information from the gamestate.
	 *
	 * Updating the render entity with this method is thread-safe.
	 *
	 * @param ref_id Game entity ID.
	 * @param position Position of the game entity inside the game world.
	 * @param angle Angle of the game entity inside the game world.
	 * @param animation_path Path to the animation definition.
	 * @param time Simulation time of the update.
	 */
	void update(const uint32_t ref_id,
	            const curve::Continuous<coord::phys3> &position,
	            const curve::Segmented<coord::phys_angle_t> &angle,
	            const std::string animation_path,
	            const time::time_t time = 0.0,
	            std::optional<time::time_t> animation_started_at = std::nullopt);

	/** Update with animation variants that may change after each completed loop. */
	void update(const uint32_t ref_id,
	            const curve::Continuous<coord::phys3> &position,
	            const curve::Segmented<coord::phys_angle_t> &angle,
	            const std::vector<std::string> &animation_paths,
	            const time::time_t time = 0.0,
	            std::optional<time::time_t> animation_started_at = std::nullopt);

	/**
	 * This function is for DEBUGGING and should not be used.
	 *
	 * Update the render entity with information from the gamestate.
	 *
	 * Updating the render entity with this method is thread-safe.
	 *
	 * @param ref_id Game entity ID.
	 * @param position Position of the game entity inside the game world.
	 * @param animation_path Path to the animation definition.
	 * @param time Simulation time of the update.
	 */
	void update(const uint32_t ref_id,
	            const coord::phys3 position,
	            const std::string animation_path,
	            const time::time_t time = 0.0);

	/**
	 * Append an authoritative position sample to the presentation curve.
	 *
	 * Retains the presented position and unplayed keyframes from interpolate_from.
	 * Without interpolate_from, initializes a stationary curve (spawn or recovery).
	 * Discards played position history so storage does not grow with match length.
	 * Repeated animation paths preserve their animation start time.
	 * Updating the render entity with this method is thread-safe.
	 *
	 * @param ref_id Game entity ID.
	 * @param position Authoritative position at time.
	 * @param animation_path Path to the animation definition.
	 * @param time Authoritative time of the new sample.
	 * @param interpolate_from Current presentation time, or none to reset.
	 * @param future_keys Host-authored future movement keyframes; no
	 *                    guesswork or extrapolation is necessary between keys.
	 */
	void update_sample(uint32_t ref_id,
	                   const coord::phys3 &position,
	                   const std::string &animation_path,
	                   time::time_t time,
	                   std::optional<time::time_t> interpolate_from,
	                   const std::vector<std::pair<time::time_t, coord::phys3>> &future_keys = {},
	                   std::optional<time::time_t> animation_started_at = std::nullopt);

	/** Append a position sample with interchangeable animation variants. */
	void update_sample(uint32_t ref_id,
	                   const coord::phys3 &position,
	                   const std::vector<std::string> &animation_paths,
	                   time::time_t time,
	                   std::optional<time::time_t> interpolate_from,
	                   const std::vector<std::pair<time::time_t, coord::phys3>> &future_keys = {},
	                   std::optional<time::time_t> animation_started_at = std::nullopt);

	/** Replace the replicated orientation with host-authored turn keys. */
	void update_facing(coord::phys_angle_t angle, time::time_t time,
	                   const std::vector<std::pair<time::time_t, coord::phys_angle_t>> &future_keys);

	/**
	 * Update the health displayed for the game entity.
	 *
	 * @param health Current health value curve.
	 * @param max_health Maximum health value.
	 * @param time Simulation time of the update.
	 */
	void update_health(const curve::Segmented<int64_t> &health,
	                   int64_t max_health,
	                   const time::time_t time = 0.0);

	/**
	 * Get the ID of the corresponding game entity.
	 *
	 * Accessing the game entity ID is thread-safe.
	 *
	 * @return Game entity ID.
	 */
	uint32_t get_id();

	/**
	 * Get the position of the entity inside the game world.
	 *
	 * Accessing the position curve REQUIRES a read lock on the render entity
	 * (using \p get_read_lock()) to ensure thread safety.
	 *
	 * @return Position curve of the entity.
	 */
	const curve::Continuous<coord::scene3> &get_position();

	/**
	 * Get the angle of the entity inside the game world.
	 *
	 * Accessing the angle curve REQUIRES a read lock on the render entity
	 * (using \p get_read_lock()) to ensure thread safety.
	 *
	 * @return Angle curve of the entity.
	 */
	const curve::Segmented<coord::phys_angle_t> &get_angle();

	/**
	 * Get the animation variants and the start time of their active state.
	 *
	 * Accessing the curve requires a read lock on the render entity
	 * (using \p get_read_lock()) to ensure thread safety.
	 */
	const curve::Discrete<std::vector<std::string>> &get_animation_paths();

	/**
	 * Check whether the entity has health data for HUD rendering.
	 *
	 * @return true if health data is available, else false.
	 */
	bool has_health();

	/** Mark this entity inactive so render stages stop displaying and picking it. */
	void deactivate(const time::time_t time);

	/** Whether this entity is still active. */
	bool is_active();

	/** Whether pixel picking may return this entity. */
	bool is_pickable() const;

	/**
	 * Get the entity position at a given time.
	 *
	 * @param time Query time.
	 * @return Position in renderer scene coordinates.
	 */
	coord::scene3 get_position_at(const time::time_t &time);

	/**
	 * Get the health value at a given time.
	 *
	 * @param time Query time.
	 * @return Current health value.
	 */
	int64_t get_health_at(const time::time_t &time);

	/**
	 * Get the maximum health value.
	 *
	 * @return Maximum health value.
	 */
	int64_t get_max_health();

private:
	/**
	 * ID of the game entity in the gamestate.
	 */
	uint32_t ref_id;

	/**
	 * Position inside the game world.
	 */
	curve::Continuous<coord::scene3> position;

	/**
	 * Angle of the entity inside the game world.
	 */
	curve::Segmented<coord::phys_angle_t> angle;

	/** Animation variants that may change after each completed loop. */
	curve::Discrete<std::vector<std::string>> animation_paths;

	/**
	 * Whether health data is available for this entity.
	 */
	bool health_available;

	/** Whether the corresponding game entity still exists. */
	bool active;

	/** Whether the corresponding game entity can be selected. */
	const bool pickable;

	/**
	 * Current health value.
	 */
	curve::Segmented<int64_t> health;

	/**
	 * Maximum health value.
	 */
	int64_t max_health;
};
} // namespace openage::renderer::world
