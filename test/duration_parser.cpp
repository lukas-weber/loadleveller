#define CATCH_CONFIG_MAIN
#include "jobinfo.cpp"
#include <catch2/catch.hpp>

using namespace loadl;

TEST_CASE("parse durations") {
	REQUIRE(parse_duration("20") == 20);
	REQUIRE(parse_duration("10:03") == 10 * 60 + 3);
	REQUIRE(parse_duration("24:06:10") == 60 * 60 * 24 + 60 * 6 + 10);
}
