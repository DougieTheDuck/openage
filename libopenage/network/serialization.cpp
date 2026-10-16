// Copyright 2026 the openage authors. See copying.md for legal info.

#include "serialization.h"

#include <cstddef>
#include <cstring>

namespace openage::network {

void append_u16(std::vector<uint8_t> &output, uint16_t value) {
	output.push_back(static_cast<uint8_t>(value >> 8));
	output.push_back(static_cast<uint8_t>(value));
}

void append_u32(std::vector<uint8_t> &output, uint32_t value) {
	output.push_back(static_cast<uint8_t>(value >> 24));
	output.push_back(static_cast<uint8_t>(value >> 16));
	output.push_back(static_cast<uint8_t>(value >> 8));
	output.push_back(static_cast<uint8_t>(value));
}

void append_u64(std::vector<uint8_t> &output, uint64_t value) {
	for (int shift = 56; shift >= 0; shift -= 8) {
		output.push_back(static_cast<uint8_t>(value >> shift));
	}
}

void append_i64(std::vector<uint8_t> &output, int64_t value) {
	uint64_t bits{};
	std::memcpy(&bits, &value, sizeof(bits));
	append_u64(output, bits);
}

uint16_t read_u16(const uint8_t *input) {
	return static_cast<uint16_t>(input[0]) << 8
	     | static_cast<uint16_t>(input[1]);
}

uint32_t read_u32(const uint8_t *input) {
	return static_cast<uint32_t>(input[0]) << 24
	     | static_cast<uint32_t>(input[1]) << 16
	     | static_cast<uint32_t>(input[2]) << 8
	     | static_cast<uint32_t>(input[3]);
}

uint64_t read_u64(const uint8_t *input) {
	uint64_t result{0};
	for (size_t i = 0; i < sizeof(uint64_t); ++i) {
		result = result << 8 | input[i];
	}
	return result;
}

int64_t read_i64(const uint8_t *input) {
	auto bits = read_u64(input);
	int64_t value{};
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

} // namespace openage::network
