// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <vector>

#include "network/messages.h"

namespace openage::network {

/** Visible construction state derived only from host-authored world records. */
enum class SitePresentationPhase {
	QUEUED,
	PLANNED,
	BUILDING,
	COMPLETE,
};

/** One world foundation or tower, with its authoritative owner and phase. */
struct SitePresentation {
	gamestate::entity_id_t entity_id;
	gamestate::player_id_t owner_id;
	gamestate::entity_id_t builder_id;
	coord::tile site;
	SitePresentationPhase phase;
};

/** A footprint tile reserved by an active or queued Builder order. */
struct ReservationPresentation {
	coord::tile tile;
	gamestate::player_id_t owner_id;
};

/** All visible construction markers for one authoritative world. */
struct ConstructionPresentation {
	std::vector<SitePresentation> sites{};
	std::vector<ReservationPresentation> reservations{};
};

/**
 * Interpret host-authoritative entity, order and reservation records for UI.
 *
 * Queued foundations do not have active ConstructionState records. They are
 * distinguished by their remaining Builder-owned reservation. Canceled
 * foundations and reservations disappear on the next accepted world update.
 * Does not inspect recipient-private kills or simulate construction locally.
 */
ConstructionPresentation make_construction_presentation(const WorldState &world);

} // namespace openage::network
