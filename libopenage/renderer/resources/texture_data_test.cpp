// Copyright 2026 the openage authors. See copying.md for legal info.

#include "renderer/resources/texture_data.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "pyinterface/pyobject.h"
#include "renderer/resources/palette_info.h"
#include "renderer/resources/parser/parse_texture.h"
#include "renderer/resources/texture_info.h"
#include "renderer/resources/texture_subinfo.h"
#include "testing/testing.h"
#include "util/path.h"


namespace openage::renderer::resources::tests {

void alpha_mask() {
	constexpr size_t width = 5;
	constexpr size_t height = 3;
	std::vector<uint8_t> pixels(width * height * 4, 0);
	const std::vector<std::pair<size_t, size_t>> opaque{
		{0, 0}, {4, 0}, {2, 1}, {0, 2}, {3, 2}};
	for (auto [x, y] : opaque) {
		pixels[(y * width + x) * 4 + 3] = ALPHA_MASK_CUTOFF + 1;
	}
	pixels[(0 * width + 1) * 4 + 3] = 1;
	pixels[(1 * width + 3) * 4 + 3] = ALPHA_MASK_CUTOFF;

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

void texture_metadata() {
	auto temporary = py::import("tempfile").getattr("TemporaryDirectory").call();
	auto directory = py::import("openage.util.fslike.directory")
	                     .getattr("Directory").call(temporary.getattr("name"));
	util::Path root{directory};
	auto path = root / "test.texture";
	auto parse = [&](const std::string &attributes) {
		auto file = path.open_w();
		file.write("version 1\nimagefile \"test.png\"\npxformat rgba8\n" + attributes);
		file.close();
		return parser::parse_texture_file(path);
	};

	auto texture = parse("size 16 16\nsubtex -1 -2 4 8 -3 2\n");
	TESTEQUALS(texture.get_subtex_count(), size_t{1});
	const auto &subtexture = texture.get_subtex_info(0);
	TESTEQUALS(subtexture.get_pos()[0], -1);
	TESTEQUALS(subtexture.get_pos()[1], -2);
	TESTEQUALS(subtexture.get_size()[0], uint32_t{4});
	TESTEQUALS(subtexture.get_size()[1], uint32_t{8});
	TESTEQUALS(subtexture.get_anchor_pos()[0], -3);
	TESTEQUALS(subtexture.get_anchor_pos()[1], 2);
	TESTTHROWS(parse("size 4294967296 16\n"));
	TESTTHROWS(parse("size 16 4294967296\n"));
	TESTTHROWS(parse("size 16 16\nsubtex 0 0 4294967296 8 0 0\n"));
	TESTTHROWS(parse("size 16 16\nsubtex 0 0 4 4294967296 0 0\n"));
	TESTTHROWS(parse("size -1 16\n"));
	TESTTHROWS(parse("size 16 16\nsubtex 0 0 -1 8 0 0\n"));
}

void palette_colors() {
	PaletteInfo palette{std::vector<uint8_t>{0, 64, 128, 255}};
	auto color = palette.get_color(0);
	TESTEQUALS(color[0], 0.0f);
	TESTEQUALS(color[1], 0.25f);
	TESTEQUALS(color[2], 0.5f);
	TESTEQUALS(color[3], 255.0f / 256.0f);
}

} // namespace openage::renderer::resources::tests
