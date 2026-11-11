#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "pantryroute/cli.hpp"

using namespace pantryroute;
using Catch::Matchers::ContainsSubstring;

namespace {

struct TempDir {
  std::filesystem::path path;
  TempDir() {
    path = std::filesystem::temp_directory_path() /
           ("pantryroute-test-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
    std::filesystem::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
  std::string write(const std::string& name, const std::string& content) const {
    const auto file = path / name;
    std::ofstream(file, std::ios::binary) << content;
    return file.string();
  }
};

struct Run {
  int code;
  std::string out;
  std::string err;
};

Run run(const std::vector<std::string>& args) {
  std::ostringstream out;
  std::ostringstream err;
  const int code = run_cli(args, out, err);
  return {code, out.str(), err.str()};
}

const char* kGoodCsv =
    "id,kind,name,lat,lon,demand,open,close,service\n"
    "HUB,depot,Food Shelf,46.720,-92.104,,08:00,16:00,\n"
    "A,stop,Ann,46.730,-92.110,3,09:00,12:00,5\n"
    "B,stop,Bo,46.700,-92.080,4,,,5\n"
    "C,stop,Cy,46.745,-92.090,2,13:00,15:00,5\n";

}  // namespace

TEST_CASE("argument parsing") {
  SECTION("solve with all options") {
    const auto r = parse_args({"solve", "in.csv", "--vehicles", "3", "--capacity", "40", "--speed", "35.5",
                               "--road-factor", "1.4", "--json", "o.json", "--no-improve", "-q"});
    REQUIRE(r.options);
    REQUIRE(r.options->command == Command::Solve);
    REQUIRE(r.options->fleet.vehicles == 3);
    REQUIRE(r.options->fleet.speed_kmh == 35.5);
    REQUIRE(r.options->json_out == "o.json");
    REQUIRE_FALSE(r.options->local_search);
    REQUIRE(r.options->quiet);
  }
  SECTION("errors") {
    CHECK_THAT(parse_args({"plan"}).error, ContainsSubstring("unknown command"));
    CHECK_THAT(parse_args({"solve", "x.csv"}).error, ContainsSubstring("--vehicles and --capacity"));
    CHECK_THAT(parse_args({"solve", "x.csv", "--vehicles"}).error, ContainsSubstring("needs a value"));
    CHECK_THAT(parse_args({"solve", "x.csv", "--vehicles", "zero"}).error, ContainsSubstring("invalid value"));
    CHECK_THAT(parse_args({"solve", "x.csv", "--road-factor", "0.5"}).error, ContainsSubstring("invalid value"));
    CHECK_THAT(parse_args({"check"}).error, ContainsSubstring("missing"));
    CHECK_THAT(parse_args({"check", "a", "b"}).error, ContainsSubstring("unexpected"));
    CHECK_THAT(parse_args({"check", "a", "--wat"}).error, ContainsSubstring("unknown option"));
  }
}

TEST_CASE("help, version and usage errors") {
  REQUIRE(run({}).code == kOk);
  REQUIRE_THAT(run({"--help"}).out, ContainsSubstring("Usage:"));
  REQUIRE_THAT(run({"--version"}).out, ContainsSubstring("pantryroute 0.1.0"));
  const Run bad = run({"frobnicate"});
  REQUIRE(bad.code == kUsage);
  REQUIRE_THAT(bad.err, ContainsSubstring("unknown command"));
}

TEST_CASE("check validates a file") {
  TempDir dir;
  const Run ok = run({"check", dir.write("good.csv", kGoodCsv)});
  REQUIRE(ok.code == kOk);
  REQUIRE_THAT(ok.out, ContainsSubstring("3 stops, 9 units"));

  const Run bad = run({"check", dir.write("bad.csv", "id,lat,lon\nHUB,46.7,-92.1\nA,46.7,east\n")});
  REQUIRE(bad.code == kInputError);
  REQUIRE_THAT(bad.err, ContainsSubstring("line 3: lon"));

  REQUIRE(run({"check", (dir.path / "missing.csv").string()}).code == kInputError);
}

TEST_CASE("solve writes every requested output") {
  TempDir dir;
  const std::string csv = dir.write("stops.csv", kGoodCsv);
  const std::string json = (dir.path / "plan.json").string();
  const std::string svg = (dir.path / "map.svg").string();
  const std::string manifest = (dir.path / "drivers.txt").string();
  const Run r = run({"solve", csv, "--vehicles", "2", "--capacity", "10", "--json", json, "--svg", svg,
                     "--manifest", manifest});
  REQUIRE(r.code == kOk);
  REQUIRE_THAT(r.out, ContainsSubstring("After local search:"));
  REQUIRE(std::filesystem::file_size(json) > 100);
  REQUIRE(std::filesystem::file_size(svg) > 100);
  REQUIRE(std::filesystem::file_size(manifest) > 50);
}

TEST_CASE("solve exits 3 and explains unschedulable stops") {
  TempDir dir;
  const std::string csv = dir.write("stops.csv", kGoodCsv);
  const Run r = run({"solve", csv, "--vehicles", "1", "--capacity", "4", "--quiet"});
  REQUIRE(r.code == kUnserved);
  REQUIRE(r.out.empty());
  REQUIRE_THAT(r.err, ContainsSubstring("could not be scheduled"));
}
