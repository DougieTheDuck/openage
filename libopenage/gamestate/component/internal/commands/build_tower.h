// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>

#include "coord/tile.h"
#include "gamestate/component/internal/commands/base_command.h"
#include "gamestate/types.h"

namespace openage::gamestate::component::command {

/**
 * Accepted FVD construction order, retained by the simulation until
 * movement and completion consume it or another order cancels it.
 */
class BuildTowerCommand : public Command {
public:
	/**
	 * @param id Unique order ID.
	 * @param owner Player requesting the construction.
	 * @param builder Builder assigned to the order.
	 * @param target Reserved tower tile.
	 */
	BuildTowerCommand(uint64_t id,
	                  player_id_t owner,
	                  entity_id_t builder,
	                  const coord::tile &target);

	command_t get_type() const override;

	/** Get the unique order ID. */
	uint64_t get_id() const;

	/** Get the requesting player. */
	player_id_t get_owner() const;

	/** Get the assigned Builder. */
	entity_id_t get_builder() const;

	/** Get the reserved tower tile. */
	const coord::tile &get_target() const;

private:
	uint64_t id;
	player_id_t owner;
	entity_id_t builder;
	coord::tile target;
};

} // namespace openage::gamestate::component::command
