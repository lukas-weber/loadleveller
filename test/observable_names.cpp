#include <catch2/catch.hpp>
#include "measurements.h"

using namespace loadl;

TEST_CASE("illegal observable names") {
	measurements meas(1);

	CHECK_THROWS(meas.add("A/B.", 1));
	CHECK_THROWS(meas.add("A/B", 1));
	CHECK_THROWS(meas.add(".AB", 1));
	CHECK_NOTHROW(meas.add("AB", 1));
}
