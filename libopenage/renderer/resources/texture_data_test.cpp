// Copyright 2026 the openage authors. See copying.md for legal info.

#include "renderer/resources/texture_data.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "renderer/resources/texture_info.h"
#include "testing/testing.h"


namespace openage::renderer::resources::tests {

void alpha_mask() {
	constexpr size_t width = 5;
	constexpr size_t height = 3;
	std::vector<uint8_t> pixels(width * height * 4, 0);
	const std::vector<std::pair<size_t, size_t>> opaque{
		{0, 0}, {4, 0}, {2, 1}, {0, 2}, {3, 2}};
	for (auto [x, y] : opaque) {
		pixels[(y * width + x) * 4 + 3] = 1;
	}

	Texture2dInfo info{width, height, pixel_format::rgba8};
	Texture2dData texture{info, std::move(pixels)};
	AlphaMask mask{texture};

	auto mask_size = mask.get_size();
	TESTEQUALS(mask_size.first, width);
	TESTEQUALS(mask_size.second, height);
	TESTEQUALS(mask.get_byte_size(), 2);
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto expected = std::find(opaque.begin(), opaque.end(), std::pair{x, y}) != opaque.end();
			TESTEQUALS(mask.is_opaque(x, y), expected);
		}
	}
	TESTEQUALS(mask.is_opaque(width, 0), false);
	TESTEQUALS(mask.is_opaque(0, height), false);
}

} // namespace openage::renderer::resources::tests
