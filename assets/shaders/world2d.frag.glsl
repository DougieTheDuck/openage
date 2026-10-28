#version 330

in vec2 vert_uv;

layout(location=0) out vec4 col;

uniform sampler2D tex;
uniform bool cbits = false;
// Below 1 for things remembered in explored fog rather than seen now.
uniform float brightness = 1.0;

// position (top left corner) and size: (x, y, width, height)
uniform vec4 tile_params;

vec2 uv = vec2(
	vert_uv.x * tile_params.z + tile_params.x,
	vert_uv.y * tile_params.w + tile_params.y
);

const float shadow_rgb_max = 8.0 / 255.0;

bool is_shadow(vec4 value, int alpha) {
	return alpha < 250
	    && max(max(value.r, value.g), value.b) <= shadow_rgb_max;
}

void main() {
	vec4 tex_val = texture(tex, uv);
	int alpha = int(round(tex_val.a * 255));
	// Texture export noise must not write depth through transparent sprite padding.
	// Keep this cutoff synchronized with ALPHA_MASK_CUTOFF in texture_data.h.
	if (alpha <= 20) {
		discard;
	}
	// Translucent effects and dark shadow pixels belong behind opaque sprites.
	// Dense rendered shadows can be more than 50% opaque, so alpha alone does
	// not identify them. Values 250 and above include team-color markers.
	gl_FragDepth = alpha < 128 || is_shadow(tex_val, alpha)
	             ? 1.0
	             : gl_FragCoord.z;
	if (!cbits) {
		col = vec4(tex_val.rgb * brightness, tex_val.a);
		return;
	}
	switch (alpha) {
		case 254:
			col = vec4(1.0f, 0.0f, 0.0f, 1.0f);
			break;
		case 252:
			col = vec4(0.0f, 1.0f, 0.0f, 1.0f);
			break;
		case 250:
			col = vec4(0.0f, 0.0f, 1.0f, 1.0f);
			break;
		default:
			col = tex_val;
			break;
	}
	col.rgb *= brightness;
}
