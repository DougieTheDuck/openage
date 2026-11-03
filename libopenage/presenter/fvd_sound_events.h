// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "audio/fvd_audio.h"
#include "coord/phys.h"
#include "gamestate/types.h"
#include "network/construction_presentation.h"
#include "network/world_delta.h"

namespace openage::presenter {

/** Sound for a host-confirmed hit or death, if that kind of entity has one. */
std::optional<audio::Cue> combat_cue(const network::ConfirmedCombatEffect &effect);

/** A construction sound and where it happened. */
struct ConstructionSound {
	audio::Cue cue;
	coord::phys3 position;
	/** Feedback for the local player's own action; not placed in the world. */
	bool local;
};

/**
 * Turns successive recipient worlds into construction sounds:
 * - FOUNDATION_PLACED when one of the local player's own sites appears;
 * - TOWER_BUILT when a visible site that was being built becomes a tower.
 *
 * Sites are matched by owner and tile, not entity ID, so it does not matter
 * whether a foundation becomes a tower in place or is replaced by one. Only
 * transitions make sounds: the first world, a reset and a change of viewer
 * (solo role switching) are recorded silently. Fog memories are ignored, so
 * a tower revealed from fog is not mistaken for a newly finished one.
 */
class ConstructionSoundTracker {
public:
	std::vector<ConstructionSound> update(const network::WorldState &world,
	                                      gamestate::player_id_t local_player,
	                                      bool reset);

private:
	using site_key_t = std::pair<gamestate::player_id_t, std::pair<coord::tile_t, coord::tile_t>>;
	std::map<site_key_t, network::SitePresentationPhase> phases{};
	std::optional<gamestate::player_id_t> viewer{};
};

} // namespace openage::presenter
