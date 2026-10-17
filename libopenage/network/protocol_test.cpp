// Copyright 2026 the openage authors. See copying.md for legal info.

#include "protocol.h"

#include <cstdint>
#include <vector>

#include "testing/testing.h"

namespace openage::network::tests {

void protocol() {
	Frame frame{};
	frame.message_type = MessageType::COMMAND_INTENT;
	frame.session_id = 0x0102030405060708;
	frame.sequence = 0x1112131415161718;
	frame.payload = {0x21, 0x22, 0x23};

	const std::vector<uint8_t> expected{
		0x46, 0x56, 0x44, 0x4e,
		0x00, 0x03,
		0x00, 0x00,
		0x00, 0x06,
		0x00, 0x00,
		0x00, 0x00, 0x00, 0x03,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x21, 0x22, 0x23};
	auto encoded = encode_frame(frame);
	encoded == expected or TESTFAIL;

	auto decoded = decode_frame(encoded);
	TESTEQUALS(decoded.protocol_major, PROTOCOL_MAJOR);
	TESTEQUALS(decoded.protocol_minor, PROTOCOL_MINOR);
	decoded.message_type == MessageType::COMMAND_INTENT or TESTFAIL;
	TESTEQUALS(decoded.flags, 0);
	TESTEQUALS(decoded.session_id, frame.session_id);
	TESTEQUALS(decoded.sequence, frame.sequence);
	decoded.payload == frame.payload or TESTFAIL;

	Frame second{};
	second.message_type = MessageType::PING;
	second.session_id = frame.session_id;
	second.sequence = frame.sequence + 1;
	auto second_encoded = encode_frame(second);

	FrameParser parser{};
	std::vector<uint8_t> first_chunk{encoded.begin(), encoded.begin() + 7};
	auto parsed = parser.push(first_chunk);
	TESTEQUALS(parsed.empty(), true);
	TESTEQUALS(parser.buffered_size(), first_chunk.size());

	std::vector<uint8_t> second_chunk{encoded.begin() + 7, encoded.end()};
	second_chunk.insert(second_chunk.end(), second_encoded.begin(), second_encoded.end());
	parsed = parser.push(second_chunk);
	TESTEQUALS(parsed.size(), 2);
	parsed[0].message_type == MessageType::COMMAND_INTENT or TESTFAIL;
	parsed[0].payload == frame.payload or TESTFAIL;
	parsed[1].message_type == MessageType::PING or TESTFAIL;
	TESTEQUALS(parsed[1].sequence, second.sequence);
	TESTEQUALS(parser.buffered_size(), 0);

	auto invalid_magic = encoded;
	invalid_magic[0] = 0;
	TESTTHROWS(decode_frame(invalid_magic));

	auto invalid_flags = encoded;
	invalid_flags[11] = 1;
	TESTTHROWS(decode_frame(invalid_flags));

	auto oversized = encoded;
	oversized[12] = 0x00;
	oversized[13] = 0x10;
	oversized[14] = 0x00;
	oversized[15] = 0x01;
	TESTTHROWS(decode_frame(oversized));

	auto trailing_data = encoded;
	trailing_data.push_back(0);
	TESTTHROWS(decode_frame(trailing_data));

	std::vector<uint8_t> incomplete_header(encoded.begin(), encoded.begin() + FRAME_HEADER_SIZE - 1);
	TESTTHROWS(decode_frame(incomplete_header));
}

} // namespace openage::network::tests
