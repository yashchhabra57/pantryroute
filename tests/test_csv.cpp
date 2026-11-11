#include <catch2/catch_test_macros.hpp>

#include "pantryroute/csv.hpp"

using namespace pantryroute;

TEST_CASE("parses simple rows and tracks line numbers") {
  const auto rows = parse_csv("a,b,c\n1,2,3\n");
  REQUIRE(rows.size() == 2);
  REQUIRE(rows[0].fields == std::vector<std::string>{"a", "b", "c"});
  REQUIRE(rows[1].fields == std::vector<std::string>{"1", "2", "3"});
  REQUIRE(rows[1].line == 2);
}

TEST_CASE("handles quotes, embedded commas, escaped quotes and newlines") {
  const auto rows = parse_csv("name,note\n\"Smith, Ann\",\"said \"\"hi\"\"\nthen left\"\nnext,row\n");
  REQUIRE(rows.size() == 3);
  REQUIRE(rows[1].fields[0] == "Smith, Ann");
  REQUIRE(rows[1].fields[1] == "said \"hi\"\nthen left");
  REQUIRE(rows[2].line == 4);
}

TEST_CASE("CRLF endings, BOM, blank lines and empty fields") {
  const auto rows = parse_csv("\xEF\xBB\xBFid,x\r\n\r\n1,\r\n,2");
  REQUIRE(rows.size() == 3);
  REQUIRE(rows[0].fields[0] == "id");
  REQUIRE(rows[1].fields == std::vector<std::string>{"1", ""});
  REQUIRE(rows[2].fields == std::vector<std::string>{"", "2"});
  REQUIRE(rows[2].line == 4);
}

TEST_CASE("malformed quoting raises CsvError with the line") {
  REQUIRE_THROWS_AS(parse_csv("a\n\"open"), CsvError);
  REQUIRE_THROWS_AS(parse_csv("ab\"c\n"), CsvError);
  try {
    parse_csv("ok\n\"x\"y\n");
    FAIL("expected throw");
  } catch (const CsvError& e) {
    REQUIRE(e.line() == 2);
  }
}
