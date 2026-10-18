// Copyright 2021-2023 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>

#include "renderer/resources/texture_info.h"


namespace openage {
namespace util {
class Path;
}

namespace renderer::resources::parser {

/**
 * Containers for the raw data.
 *
 * Definition according to doc/media/openage/texture_format_spec.md.
 */
struct SizeData {
	uint32_t width;
	uint32_t height;
};

struct PixelFormatData {
	pixel_format format;
	bool cbits{false};
};

struct SubtextureData {
	int32_t xpos;
	int32_t ypos;
	uint32_t xsize;
	uint32_t ysize;
	int32_t xanchor;
	int32_t yanchor;
};

/**
 * Parse a texture definition from a .texture format file.
 *
 * @param file Path to the texture file.
 *
 * @return The corresponding texture definition.
 */
Texture2dInfo parse_texture_file(const util::Path &file);

} // namespace renderer::resources::parser
} // namespace openage
