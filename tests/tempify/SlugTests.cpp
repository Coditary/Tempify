#include "TestHarness.h"
#include "tempify/support/Slug.h"

#include <string>

TEST_CASE(Slugify_normalizes_separators_and_empty_input) {
    REQUIRE_EQ(tempify::slugify("Hello World!!"), std::string("hello-world"));
    REQUIRE_EQ(tempify::slugify("---leading-and-trailing---"), std::string("leading-and-trailing"));
    REQUIRE_EQ(tempify::slugify("!!!"), std::string("app"));
}
