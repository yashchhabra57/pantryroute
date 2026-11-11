// Solver benchmark: synthetic delivery days of increasing size.
//
// For each size, solves several seeded instances and reports the median solve
// time plus how much local search shortened the savings construction.
//
// Usage: pantryroute_bench [--sizes 25,50,100,200] [--seeds 5] [--max-ms N]
// With --max-ms, exits non-zero if any median exceeds the budget (used in CI as
// a coarse performance regression guard).

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "pantryroute/generator.hpp"
#include "pantryroute/route.hpp"
#include "pantryroute/solver.hpp"

using namespace pantryroute;

namespace {

double median(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  const std::size_t n = v.size();
  return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

std::vector<std::size_t> parse_sizes(const std::string& s) {
  std::vector<std::size_t> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) out.push_back(static_cast<std::size_t>(std::stoul(item)));
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::size_t> sizes{25, 50, 100, 200};
  unsigned seeds = 5;
  double max_ms = 0;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string flag = argv[i];
    const std::string value = argv[i + 1];
    if (flag == "--sizes") sizes = parse_sizes(value);
    else if (flag == "--seeds") seeds = static_cast<unsigned>(std::stoul(value));
    else if (flag == "--max-ms") max_ms = std::stod(value);
    else {
      std::fprintf(stderr, "unknown flag %s\n", flag.c_str());
      return 64;
    }
  }

  std::printf("| stops | vehicles | median solve ms | construction km | final km | improvement | moves | unserved | valid |\n");
  std::printf("|---:|---:|---:|---:|---:|---:|---:|---:|:---:|\n");
  bool over_budget = false;
  bool all_valid = true;
  for (const std::size_t n : sizes) {
    // Roughly 10 stops per vehicle, with room in the van for ~12.
    const Fleet fleet{static_cast<int>(std::max<std::size_t>(1, n / 10)), 45, 40.0, 1.3};
    std::vector<double> times;
    double construction_km = 0;
    double final_km = 0;
    std::size_t moves = 0;
    std::size_t unserved = 0;
    bool valid = true;
    for (unsigned seed = 1; seed <= seeds; ++seed) {
      const Problem p = generate_problem(GeneratorOptions{n, seed, 0.3, 0.15}, fleet);
      const SolveReport r = solve(p);
      times.push_back(r.elapsed_ms);
      construction_km += r.construction.km;
      final_km += r.final.km;
      moves += r.moves.total();
      unserved += r.final.unserved;
      valid = valid && validate_solution(p, r.solution).empty();
    }
    const double med = median(times);
    const double improvement = construction_km > 0 ? 100.0 * (construction_km - final_km) / construction_km : 0;
    std::printf("| %zu | %d | %.2f | %.1f | %.1f | %.1f%% | %zu | %.1f | %s |\n", n, fleet.vehicles, med,
                construction_km / seeds, final_km / seeds, improvement, moves / seeds,
                static_cast<double>(unserved) / seeds, valid ? "yes" : "NO");
    if (max_ms > 0 && med > max_ms) over_budget = true;
    all_valid = all_valid && valid;
  }
  if (!all_valid) {
    std::fprintf(stderr, "benchmark produced an invalid solution\n");
    return 1;
  }
  if (over_budget) {
    std::fprintf(stderr, "median solve time exceeded %.0f ms\n", max_ms);
    return 2;
  }
  return 0;
}
