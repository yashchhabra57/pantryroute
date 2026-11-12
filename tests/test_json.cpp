#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "pantryroute/json.hpp"

using namespace pantryroute;

TEST_CASE("compact output with nesting and commas") {
  JsonWriter w(false);
  w.begin_object()
      .key("name").value("Route 1")
      .key("stops").begin_array().value(1).value(2).end_array()
      .key("ok").value(true)
      .key("none").null()
      .key("empty").begin_object().end_object()
      .end_object();
  REQUIRE(w.str() == R"({"name":"Route 1","stops":[1,2],"ok":true,"none":null,"empty":{}})");
}

TEST_CASE("escapes quotes, backslashes and control characters") {
  REQUIRE(JsonWriter::escape("a\"b\\c\nd\x01") == R"("a\"b\\c\nd\u0001")");
}

TEST_CASE("doubles are rounded and trimmed; non-finite becomes null") {
  JsonWriter w(false);
  w.begin_array()
      .value(12.5)
      .value(3.0)
      .value(1.23456)
      .value(-0.0001)
      .value(std::numeric_limits<double>::infinity())
      .end_array();
  REQUIRE(w.str() == "[12.5,3,1.235,0,null]");
}

TEST_CASE("very large doubles are written in full, not truncated") {
  JsonWriter w(false);
  w.value(1e40);
  REQUIRE(w.str().size() == 41);  // 1 followed by 40 digits (no fraction left after trimming)
  REQUIRE(w.str().front() == '1');
}

TEST_CASE("pretty output indents nested containers") {
  JsonWriter w(true);
  w.begin_object().key("a").begin_array().value(1).end_array().end_object();
  REQUIRE(w.str() == "{\n  \"a\": [\n    1\n  ]\n}");
}
