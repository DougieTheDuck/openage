// Copyright 2026 the openage authors. See copying.md for legal info.

#include "testing.h"

#include <string>
#include <vector>

namespace openage::testing::tests {

namespace {

class EqualityOnly {
public:
	explicit EqualityOnly(int value) :
		value{value} {}

	EqualityOnly(const EqualityOnly &) = delete;
	EqualityOnly &operator=(const EqualityOnly &) = delete;

	bool operator==(const EqualityOnly &other) const {
		return this->value == other.value;
	}

	bool operator!=(const EqualityOnly &other) const {
		return not(*this == other);
	}

private:
	int value;
};

template <typename Assertion>
std::string failure_message(const Assertion &assertion) {
	try {
		assertion();
	}
	catch (const TestError &error) {
		return error.msg.text;
	}
	throw TestError{MSG(err) << "Expected a comparison assertion to fail"};
}

} // namespace

void comparison() {
	const std::vector<int> actual{1, 2, 3};
	const std::vector<int> expected{1, 2, 3};
	const std::vector<int> different{1, 2, 4};
	TESTEQUALS(actual, expected);
	TESTEQUALS(actual, (std::vector<int>{1, 2, 3}));
	// Member references from temporary containers must stay valid
	// throughout each assertion (including its failure diagnostics).
	TESTEQUALS((std::vector<int>{42}).front(), 42);
	TESTNOTEQUALS((std::vector<int>{42}).front(), 41);
	const auto temporary_member_message = failure_message([&]() {
		TESTEQUALS((std::vector<int>{42}).front(), 41);
	});
	TESTEQUALS(temporary_member_message.find("Expected 42 and 41") != std::string::npos, true);
	TESTNOTEQUALS(actual, different);
	const auto vector_message = failure_message([&]() { TESTEQUALS(actual, different); });
	TESTEQUALS(vector_message.find("Expected <actual> and <different> to be equal") != std::string::npos, true);
	TESTEQUALS(vector_message.find("testing_test.cpp:") != std::string::npos, true);
	const auto inequality_message = failure_message([&]() { TESTNOTEQUALS(actual, expected); });
	TESTEQUALS(inequality_message.find("Expected <actual> and <expected> to be NOT equal") != std::string::npos, true);

	const EqualityOnly first{7};
	const EqualityOnly equal{7};
	const EqualityOnly other{9};
	TESTEQUALS(first, equal);
	TESTNOTEQUALS(first, other);
	const auto opaque_message = failure_message([&]() { TESTEQUALS(first, other); });
	TESTEQUALS(opaque_message.find("<first> and <other>") != std::string::npos, true);

	const int seven{7};
	const int nine{9};
	const auto numeric_message = failure_message([&]() { TESTEQUALS(seven, nine); });
	TESTEQUALS(numeric_message.find("Expected 7 and 9 to be equal") != std::string::npos, true);

	int left_calls{0};
	int right_calls{0};
	auto left = [&]() {
		++left_calls;
		return 7;
	};
	auto right = [&]() {
		++right_calls;
		return 9;
	};
	failure_message([&]() { TESTEQUALS(left(), right()); });
	TESTEQUALS(left_calls, 1);
	TESTEQUALS(right_calls, 1);
	left_calls = 0;
	right_calls = 0;
	failure_message([&]() { TESTNOTEQUALS(left(), right() - 2); });
	TESTEQUALS(left_calls, 1);
	TESTEQUALS(right_calls, 1);
}

} // namespace openage::testing::tests
