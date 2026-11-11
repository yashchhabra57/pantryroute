#include <catch2/catch_test_macros.hpp>

#include "pantryroute/clock.hpp"

using namespace pantryroute;

TEST_CASE("parse_clock accepts valid 24h times") {
  REQUIRE(parse_clock("00:00") == 0);
  REQUIRE(parse_clock("9:05") == 545);
  REQUIRE(parse_clock("09:05") == 545);
  REQUIRE(parse_clock("17:30") == 1050);
  REQUIRE(parse_clock("24:00") == 1440);
}

TEST_CASE("parse_clock rejects malformed input") {
  for (const char* bad : {"", "9", "9:5", "25:00", "24:01", "12:60", "ab:cd", "123:00", ":30", "12:3a"}) {
    INFO(bad);
    REQUIRE_FALSE(parse_clock(bad).has_value());
  }
}

TEST_CASE("format_clock rounds and zero-pads") {
  REQUIRE(format_clock(0) == "00:00");
  REQUIRE(format_clock(545) == "09:05");
  REQUIRE(format_clock(599.6) == "10:00");
}
