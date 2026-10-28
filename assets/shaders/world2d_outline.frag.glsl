#version 330

in vec2 vert_uv;

layout(location=0) out vec4 col;

uniform sampler2D tex;
uniform vec4 outline_color;

// position (top left corner) and size: (x, y, width, height)
uniform vec4 tile_params;

const float silhouette_alpha = 0.625;
const float shadow_rgb_max = 8.0 / 255.0;

float sprite_alpha(ivec2 texel, ivec2 tile_min, ivec2 tile_max) {
	if (any(lessThan(texel, tile_min)) || any(greaterThanEqual(texel, tile_max))) {
		return 0.0;
	}
	vec4 value = texelFetch(tex, texel, 0);
	int alpha = int(round(value.a * 255));
	if (alpha < 250
	    && max(max(value.r, value.g), value.b) <= shadow_rgb_max) {
		return 0.0;
	}
	return value.a;
}

void main() {
	vec2 uv = vec2(
		vert_uv.x * tile_params.z + tile_params.x,
		vert_uv.y * tile_params.w + tile_params.y
	);
	ivec2 texture_size = textureSize(tex, 0);
	ivec2 tile_min = ivec2(round(tile_params.xy * vec2(texture_size)));
	ivec2 tile_max = tile_min + ivec2(round(tile_params.zw * vec2(texture_size)));
	ivec2 texel = ivec2(floor(uv * vec2(texture_size)));
	if (sprite_alpha(texel, tile_min, tile_max) >= silhouette_alpha) {
		discard;
	}

	float neighbor_alpha = 0.0;
	for (int y = -3; y <= 3; ++y) {
		for (int x = -3; x <= 3; ++x) {
			if (x == 0 && y == 0) {
				continue;
			}
			neighbor_alpha = max(
				neighbor_alpha,
				sprite_alpha(texel + ivec2(x, y), tile_min, tile_max)
			);
		}
	}
	if (neighbor_alpha < silhouette_alpha) {
		discard;
	}

	col = outline_color;
}
