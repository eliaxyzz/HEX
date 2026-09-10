/**
 * @file test_framework.h
 * @brief Assertion micro-framework for the suite.
 *
 * Deliberately minimal: no external dependency, one output line per assertion,
 * and a failure counter that becomes the exit code.
 */

#ifndef HEX_TEST_FRAMEWORK_H
#define HEX_TEST_FRAMEWORK_H

#include <iostream>

/** @brief Failed assertions, reported one per line. */
inline int failures = 0;

/**
 * @brief Failures inside a loop, counted without printing.
 *
 * Used when the same property is checked over thousands of cases: the total is
 * inspected once instead of producing thousands of lines.
 *
 * @warning Must be reset to zero after each use.
 */
inline int quiet_failures = 0;

/** @brief Silent check: increments quiet_failures without printing. */
#define CHECK_QUIET(cond) do { if (!(cond)) ++quiet_failures; } while (0)

/** @brief Reported check: one line per assertion. */
#define CHECK(cond, msg) do { \
        if (!(cond)) { std::cout << "FAIL: " << msg << "\n"; ++failures; } \
        else         { std::cout << "ok  : " << msg << "\n"; } \
    } while (0)

#endif //HEX_TEST_FRAMEWORK_H
