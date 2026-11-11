#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "pantryroute/model.hpp"

namespace pantryroute {

enum class Command { Help, Version, Check, Solve };

struct CliOptions {
  Command command = Command::Help;
  std::string input;
  Fleet fleet;
  bool fleet_vehicles_set = false;
  bool fleet_capacity_set = false;
  std::optional<std::string> json_out;
  std::optional<std::string> svg_out;
  std::optional<std::string> manifest_out;
  bool local_search = true;
  std::size_t max_rounds = 200;
  bool quiet = false;
};

struct ParseResult {
  std::optional<CliOptions> options;
  std::string error;
};

/// Parses argv (without the program name). Never exits; errors come back as text.
ParseResult parse_args(const std::vector<std::string>& args);

/// Exit codes, so scripts can tell bad input from unschedulable stops.
enum ExitCode : int {
  kOk = 0,
  kInputError = 2,   // CSV couldn't be read or failed validation
  kUnserved = 3,     // plan produced, but some stops could not be scheduled
  kUsage = 64,       // bad command-line arguments (BSD sysexits EX_USAGE)
};

/// Runs the CLI end to end, writing to the given streams. Returns an exit code.
int run_cli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

std::string usage_text();

}  // namespace pantryroute
