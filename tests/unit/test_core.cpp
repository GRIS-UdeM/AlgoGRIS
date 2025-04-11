#include <catch2/catch_all.hpp>

TEST_CASE("check things are setup", "[core]")
{
  GIVEN("A test suite running")
  {
    THEN("The tests run")
    {
      REQUIRE(true);
    }
  }
}