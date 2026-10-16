// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <vector>

namespace openage::network {

/** Append an unsigned 16-bit integer in network byte order. */
void append_u16(std::vector<uint8_t> &output, uint16_t value);

/** Append an unsigned 32-bit integer in network byte order. */
void append_u32(std::vector<uint8_t> &output, uint32_t value);

/** Append an unsigned 64-bit integer in network byte order. */
void append_u64(std::vector<uint8_t> &output, uint64_t value);

/** Append a signed 64-bit integer in network byte order. */
void append_i64(std::vector<uint8_t> &output, int64_t value);

/** Read an unsigned 16-bit integer from network byte order. */
uint16_t read_u16(const uint8_t *input);

/** Read an unsigned 32-bit integer from network byte order. */
uint32_t read_u32(const uint8_t *input);

/** Read an unsigned 64-bit integer from network byte order. */
uint64_t read_u64(const uint8_t *input);

/** Read a signed 64-bit integer from network byte order. */
int64_t read_i64(const uint8_t *input);

} // namespace openage::network
