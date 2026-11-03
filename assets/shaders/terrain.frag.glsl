#version 330

in vec2 tex_pos;
in vec2 grid_pos;
in vec3 world_pos;

layout(location=0) out vec4 out_col;

uniform sampler2D tex;
uniform bool show_grid;

// Fog is a single nearest-filtered texel per tile (se, ne):
// 0 = never explored, 128 = explored, 255 = visible.
// The gameplay fog bitsets remain hard-edged. Only terrain brightness is
// interpolated; hidden units and selection still obey exact tile visibility.
uniform sampler2D fog;
uniform bool fog_enabled;
uniform float fog_map_size;
// Width of the visual transition across a tile boundary, measured in tiles.
// 0 restores hard edges; 0.55 is the default FVD softness.
uniform float fog_edge_width;

// Read a discrete fog tile without changing the shared GPU texture's filtering.
// Clamp to the map edges so fog never wraps around the opposite side.
float tile_fog_brightness(ivec2 tile) {
    ivec2 last = ivec2(int(fog_map_size) - 1);
    float level = texelFetch(fog, clamp(tile, ivec2(0), last), 0).r;
    return level > 0.75 ? 1.0 : (level > 0.25 ? 0.45 : 0.0);
}

float terrain_fog_brightness(vec2 tile_pos) {
    if (fog_edge_width <= 0.0) {
        return tile_fog_brightness(ivec2(floor(tile_pos)));
    }

    // A slight static wobble keeps long reveal boundaries from looking like
    // rulers. Its displacement is small compared with the blend width.
    float wave = sin(tile_pos.x * 5.3 + tile_pos.y * 1.7)
               * sin(tile_pos.y * 4.7 - tile_pos.x * 2.3);
    vec2 softened_pos = tile_pos + 0.055 * vec2(wave, -wave);

    // Fog texels represent *tile centres*, not corners. Interpolate between
    // adjacent centres but keep flat plateaus around each centre. This makes
    // the transition straddle the actual tile edge (rather than darkening
    // every point throughout the tile, as ordinary linear filtering would).
    vec2 at_centres = softened_pos - vec2(0.5);
    ivec2 base = ivec2(floor(at_centres));
    vec2 fraction = fract(at_centres);
    float half_width = 0.5 * clamp(fog_edge_width, 0.0, 1.0);
    vec2 blend = smoothstep(vec2(0.5 - half_width),
                            vec2(0.5 + half_width), fraction);

    float lower_left = tile_fog_brightness(base);
    float lower_right = tile_fog_brightness(base + ivec2(1, 0));
    float upper_left = tile_fog_brightness(base + ivec2(0, 1));
    float upper_right = tile_fog_brightness(base + ivec2(1, 1));

    return mix(mix(lower_left, lower_right, blend.x),
               mix(upper_left, upper_right, blend.x), blend.y);
}

void main()
{
    vec4 tex_val = texture(tex, tex_pos);
    if (show_grid) {
        vec2 distance_to_line = abs(fract(grid_pos - 0.5) - 0.5) / fwidth(grid_pos);
        float line = 1.0 - min(min(distance_to_line.x, distance_to_line.y), 1.0);
        tex_val.rgb = mix(tex_val.rgb, vec3(0.08, 0.12, 0.06), line * 0.55);
    }
    if (fog_enabled) {
        // World space is (se, up, -ne). Do not change unit visibility:
        // this interpolation is strictly a cosmetic terrain effect.
        vec2 tile_pos = vec2(world_pos.x, -world_pos.z);
        tex_val.rgb *= terrain_fog_brightness(tile_pos);
    }
    out_col = tex_val;
}
