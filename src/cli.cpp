#include "pantryroute/cli.hpp"

#include <charconv>
#include <fstream>
#include <ostream>
#include <sstream>

#include "pantryroute/loader.hpp"
#include "pantryroute/report.hpp"
#include "pantryroute/solver.hpp"

namespace pantryroute {

namespace {

constexpr const char* kVersion = "0.1.0";

template <typename T>
bool parse_value(const std::string& s, T& out) {
  const char* end = s.data() + s.size();
  const auto [ptr, ec] = std::from_chars(s.data(), end, out);
  return !s.empty() && ec == std::errc{} && ptr == end;
}

std::optional<std::string> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

bool write_file(const std::string& path, const std::string& content, std::ostream& err) {
  std::ofstream f(path, std::ios::binary);
  if (!f) {
    err << "error: cannot write " << path << "\n";
    return false;
  }
  f << content;
  return static_cast<bool>(f);
}

}  // namespace

std::string usage_text() {
  return R"(pantryroute - plan delivery routes for food pantry volunteer drivers

Usage:
  pantryroute check <stops.csv>
  pantryroute solve <stops.csv> --vehicles N --capacity C [options]

Options for solve:
  --vehicles N        number of drivers/vehicles available (required)
  --capacity C        units (boxes) one vehicle can carry (required)
  --speed KMH         average driving speed, default 40
  --road-factor F     multiplier on straight-line distance, default 1.3
  --json FILE         write the full plan as JSON
  --svg FILE          write a route map as SVG
  --manifest FILE     write printable driver sheets
  --no-improve        skip local search (construction heuristic only)
  --max-rounds N      cap on local search rounds, default 200
  --quiet             don't print the summary

CSV columns: id, lat, lon (required); kind (depot|stop), name, demand,
open, close (HH:MM), service (minutes), notes (optional).

Exit codes: 0 ok, 2 invalid input, 3 some stops unscheduled, 64 bad arguments.
)";
}

ParseResult parse_args(const std::vector<std::string>& args) {
  ParseResult result;
  CliOptions opt;
  if (args.empty() || args[0] == "-h" || args[0] == "--help" || args[0] == "help") {
    opt.command = Command::Help;
    result.options = opt;
    return result;
  }
  if (args[0] == "--version" || args[0] == "-V") {
    opt.command = Command::Version;
    result.options = opt;
    return result;
  }
  if (args[0] == "check") {
    opt.command = Command::Check;
  } else if (args[0] == "solve") {
    opt.command = Command::Solve;
  } else {
    result.error = "unknown command '" + args[0] + "'";
    return result;
  }

  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto next = [&](const char* flag) -> std::optional<std::string> {
      if (i + 1 >= args.size()) {
        result.error = std::string(flag) + " needs a value";
        return std::nullopt;
      }
      return args[++i];
    };
    auto bad = [&](const std::string& flag, const std::string& v) {
      result.error = "invalid value '" + v + "' for " + flag;
    };

    if (a == "--vehicles" || a == "--capacity" || a == "--max-rounds") {
      const auto v = next(a.c_str());
      if (!v) return result;
      long n = 0;
      if (!parse_value(*v, n) || n < 1 || n > 100000) {
        bad(a, *v);
        return result;
      }
      if (a == "--vehicles") {
        opt.fleet.vehicles = static_cast<int>(n);
        opt.fleet_vehicles_set = true;
      } else if (a == "--capacity") {
        opt.fleet.capacity = static_cast<int>(n);
        opt.fleet_capacity_set = true;
      } else {
        opt.max_rounds = static_cast<std::size_t>(n);
      }
    } else if (a == "--speed" || a == "--road-factor") {
      const auto v = next(a.c_str());
      if (!v) return result;
      double d = 0;
      const bool ok = parse_value(*v, d) && (a == "--speed" ? d > 0 && d <= 200 : d >= 1.0 && d <= 5.0);
      if (!ok) {
        bad(a, *v);
        return result;
      }
      (a == "--speed" ? opt.fleet.speed_kmh : opt.fleet.road_factor) = d;
    } else if (a == "--json" || a == "--svg" || a == "--manifest") {
      const auto v = next(a.c_str());
      if (!v) return result;
      (a == "--json" ? opt.json_out : a == "--svg" ? opt.svg_out : opt.manifest_out) = *v;
    } else if (a == "--no-improve") {
      opt.local_search = false;
    } else if (a == "--quiet" || a == "-q") {
      opt.quiet = true;
    } else if (!a.empty() && a[0] == '-') {
      result.error = "unknown option '" + a + "'";
      return result;
    } else if (opt.input.empty()) {
      opt.input = a;
    } else {
      result.error = "unexpected argument '" + a + "'";
      return result;
    }
  }

  if (opt.input.empty()) {
    result.error = "missing <stops.csv>";
    return result;
  }
  if (opt.command == Command::Solve && (!opt.fleet_vehicles_set || !opt.fleet_capacity_set)) {
    result.error = "solve needs --vehicles and --capacity";
    return result;
  }
  result.options = opt;
  return result;
}

int run_cli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  const ParseResult parsed = parse_args(args);
  if (!parsed.options) {
    err << "error: " << parsed.error << "\n\n" << usage_text();
    return kUsage;
  }
  const CliOptions& opt = *parsed.options;
  if (opt.command == Command::Help) {
    out << usage_text();
    return kOk;
  }
  if (opt.command == Command::Version) {
    out << "pantryroute " << kVersion << "\n";
    return kOk;
  }

  const auto text = read_file(opt.input);
  if (!text) {
    err << "error: cannot read " << opt.input << "\n";
    return kInputError;
  }
  const LoadResult loaded = load_stops_csv(*text);
  if (!loaded.ok()) {
    err << opt.input << ": " << loaded.errors.size() << " problem(s) found\n";
    for (const auto& e : loaded.errors) err << "  " << e << "\n";
    return kInputError;
  }

  if (opt.command == Command::Check) {
    int total = 0;
    for (const auto& s : loaded.data->stops) total += s.demand;
    out << opt.input << ": OK - depot '" << loaded.data->depot.name << "', " << loaded.data->stops.size()
        << " stops, " << total << " units to deliver\n";
    return kOk;
  }

  const Problem problem = build_problem(loaded.data->depot, loaded.data->stops, opt.fleet);
  const SolveReport report = solve(problem, SolveOptions{opt.local_search, opt.max_rounds});

  if (!opt.quiet) out << summary_text(problem, report);
  bool wrote = true;
  if (opt.json_out) wrote &= write_file(*opt.json_out, plan_json(problem, report), err);
  if (opt.svg_out) wrote &= write_file(*opt.svg_out, route_svg(problem, report.solution), err);
  if (opt.manifest_out) wrote &= write_file(*opt.manifest_out, driver_manifest(problem, report), err);
  if (!wrote) return kInputError;

  if (!report.solution.unserved.empty()) {
    err << report.solution.unserved.size() << " stop(s) could not be scheduled:\n";
    for (const Node n : report.solution.unserved) {
      err << "  " << problem.node(n).id << ": " << unserved_reason(problem, n) << "\n";
    }
    return kUnserved;
  }
  return kOk;
}

}  // namespace pantryroute
