// Copyright 2014-2025 the openage authors. See copying.md for legal info.

#pragma once

#include <sstream>
#include <string>
#include <type_traits>

#include "../error/error.h"

namespace openage {
namespace testing {


/**
 * To be used by all tests to indicate an error condition.
 *
 * An optional error message may be passed as part of the message,
 * but the file name and line number are of primary concern.
 */
class TestError : public Error {
	using Error::Error;
};


/**
 * Raises TestError with the message msg.
 * Mostly designed for usage with the TESTFAIL macro.
 */
bool fail(const log::message &msg);

namespace detail {

/** Describe assertion operands without requiring their types to support output. */
template <typename T>
std::string describe_comparison_value(const T &value, const char *expression) {
	if constexpr (std::is_enum_v<T> or requires(std::ostream &stream) { stream << value; }) {
		std::ostringstream stream{};
		if constexpr (std::is_enum_v<T>) {
			stream << static_cast<std::underlying_type_t<T>>(value);
		}
		else {
			stream << value;
		}
		return stream.str();
	}
	else {
		return std::string{"<"} + expression + ">";
	}
}

} // namespace detail


/**
 * Convenience macro, for usage in test functions:
 *
 * test_stuff() or TESTFAIL;
 */
#define TESTFAIL ::openage::testing::fail(MSG(err) << __FILE__ << ":" << __LINE__)


/**
 * As above, but allows printing a message:
 *
 * test_stuff() or TESTFAIL("lolnope");
 */
#define TESTFAILMSG(...) ::openage::testing::fail(MSG(err) << __VA_ARGS__)


/**
 * Asserts that the left expression equals the right expression,
 * and that no exception is thrown.
 * Evaluates each operand once and accepts const and non-printable values.
 */
#define TESTEQUALS(left, right) \
	do { \
		try { \
			auto &&test_result_left = (left); \
			auto &&test_result_right = (right); \
			if (test_result_left != test_result_right) { \
				TESTFAILMSG(__FILE__ << ":" << __LINE__ << ": Expected " \
				                     << ::openage::testing::detail::describe_comparison_value(test_result_left, #left) << " and " \
				                     << ::openage::testing::detail::describe_comparison_value(test_result_right, #right) << " to be equal"); \
			} \
		} \
		catch (::openage::testing::TestError & e) { \
			throw; \
		} \
		catch (::openage::error::Error & e) { \
			TESTFAILMSG("unexpected exception: " << e); \
		} \
	} \
	while (0)

/**
 * Asserts that the left expression does not equal the right expression,
 * and that no exception is thrown.
 * Evaluates each operand once and accepts const and non-printable values.
 */
#define TESTNOTEQUALS(left, right) \
	do { \
		try { \
			auto &&test_result_left = (left); \
			auto &&test_result_right = (right); \
			if (test_result_left == test_result_right) { \
				TESTFAILMSG(__FILE__ << ":" << __LINE__ << ": Expected " \
				                     << ::openage::testing::detail::describe_comparison_value(test_result_left, #left) << " and " \
				                     << ::openage::testing::detail::describe_comparison_value(test_result_right, #right) << " to be NOT equal"); \
			} \
		} \
		catch (::openage::testing::TestError & e) { \
			throw; \
		} \
		catch (::openage::error::Error & e) { \
			TESTFAILMSG("unexpected exception: " << e); \
		} \
	} \
	while (0)

/**
 * Asserts that the left expression equals the right expression,
 * within a margin of error epsilon, and that no exception is thrown.
 */
#define TESTEQUALS_FLOAT(left, right, epsilon) \
	do { \
		try { \
			auto &&test_result_left = (left); \
			auto &&test_result_right = (right); \
			if ((test_result_left < (test_result_right - epsilon)) or (test_result_left > (test_result_right + epsilon))) { \
				TESTFAILMSG(__FILE__ << ":" << __LINE__ << ": Expected " << (test_result_left) << " and " << (test_result_right) << " to be equal"); \
			} \
		} \
		catch (::openage::testing::TestError & e) { \
			throw; \
		} \
		catch (::openage::error::Error & e) { \
			TESTFAILMSG("unexpected exception: " << e); \
		} \
	} \
	while (0)


/**
 * Asserts that the expression throws an exception.
 */
#define TESTTHROWS(expression) \
	do { \
		bool expr_has_thrown = false; \
		try { \
			expression; \
		} \
		catch (::openage::error::Error & e) { \
			expr_has_thrown = true; \
		} \
		if (not expr_has_thrown) { \
			TESTFAILMSG(__FILE__ << ":" << __LINE__ << ": no exception"); \
		} \
	} \
	while (0)


/**
 * Assets that the expression throws no exception (mostly for void expressions).
 */
#define TESTNOEXCEPT(expression) \
	do { \
		try { \
			expression; \
		} \
		catch (::openage::error::Error & e) { \
			TESTFAILMSG(__FILE__ << ":" << __LINE__ << ": unexpected exception"); \
		} \
	} \
	while (0)


} // namespace testing
} // namespace openage
