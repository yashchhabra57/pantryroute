#include "pantryroute/report.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <sstream>

#include "pantryroute/clock.hpp"
#include "pantryroute/json.hpp"

namespace pantryroute {

namespace {

std::string fixed(double v, int digits = 1) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f", digits, v);
  return buf;
}

std::string xml_escape(const std::string& s) {
  std::string out;
  for (const char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out.push_back(c);
    }
  }
  return out;
}

// Colour-blind-friendly categorical palette (Okabe-Ito, minus black/yellow).
constexpr std::array<const char*, 6> kPalette{"#0072B2", "#D55E00", "#009E73", "#CC79A7", "#56B4E9", "#E69F00"};

void write_stats(JsonWriter& w, const SolutionStats& st) {
  w.begin_object()
      .key("km").value(st.km)
      .key("route_minutes").value(st.drive_minutes)
      .key("vehicles_used").value(st.vehicles_used)
      .key("served").value(st.served)
      .key("unserved").value(st.unserved)
      .end_object();
}

}  // namespace

std::string unserved_reason(const Problem& p, Node n) {
  const RouteEval alone = evaluate_route(p, {n});
  switch (alone.violation) {
    case Violation::Capacity:
      return "needs " + std::to_string(p.node(n).demand) + " units but a vehicle holds " +
             std::to_string(p.fleet.capacity);
    case Violation::TimeWindow:
      return "window closes at " + format_clock(p.node(n).close) +
             " but the earliest possible arrival is " + format_clock(p.depot.open + p.minutes(kDepot, n));
    case Violation::ShiftEnd:
      return "cannot get there and back before the depot closes at " + format_clock(p.depot.close);
    case Violation::None:
      break;
  }
  return "no vehicle had room or time left; add a vehicle or widen its window";
}

std::string plan_json(const Problem& p, const SolveReport& report) {
  JsonWriter w(true);
  w.begin_object();
  w.key("depot").begin_object()
      .key("id").value(p.depot.id)
      .key("name").value(p.depot.name)
      .key("lat").value(p.depot.pos.lat)
      .key("lon").value(p.depot.pos.lon)
      .key("open").value(format_clock(p.depot.open))
      .key("close").value(format_clock(p.depot.close))
      .end_object();
  w.key("fleet").begin_object()
      .key("vehicles").value(p.fleet.vehicles)
      .key("capacity").value(p.fleet.capacity)
      .key("speed_kmh").value(p.fleet.speed_kmh)
      .key("road_factor").value(p.fleet.road_factor)
      .end_object();
  w.key("summary").begin_object();
  w.key("construction");
  write_stats(w, report.construction);
  w.key("final");
  write_stats(w, report.final);
  w.key("improvement_pct").value(report.construction.km > 0
                                     ? 100.0 * (report.construction.km - report.final.km) / report.construction.km
                                     : 0.0);
  w.key("moves").begin_object()
      .key("two_opt").value(report.moves.two_opt)
      .key("relocate").value(report.moves.relocate)
      .key("or_opt").value(report.moves.or_opt)
      .key("swap").value(report.moves.swap)
      .key("cross").value(report.moves.cross)
      .key("rounds").value(report.moves.rounds)
      .end_object();
  w.key("solve_ms").value(report.elapsed_ms);
  w.end_object();

  w.key("routes").begin_array();
  for (std::size_t i = 0; i < report.solution.routes.size(); ++i) {
    const Route& r = report.solution.routes[i];
    const RouteEval ev = evaluate_route(p, r, true);
    w.begin_object()
        .key("vehicle").value(i + 1)
        .key("km").value(ev.km)
        .key("load").value(ev.load)
        .key("depart").value(format_clock(ev.depart_depot))
        .key("return").value(format_clock(ev.return_depot))
        .key("wait_minutes").value(ev.wait);
    w.key("stops").begin_array();
    for (const Visit& v : ev.visits) {
      const Stop& s = p.node(v.node);
      w.begin_object()
          .key("id").value(s.id)
          .key("name").value(s.name)
          .key("lat").value(s.pos.lat)
          .key("lon").value(s.pos.lon)
          .key("arrive").value(format_clock(v.arrival))
          .key("start").value(format_clock(v.start))
          .key("depart").value(format_clock(v.departure))
          .key("window").value(format_clock(s.open) + "-" + format_clock(s.close))
          .key("demand").value(s.demand)
          .key("load_after").value(v.load_after)
          .end_object();
    }
    w.end_array().end_object();
  }
  w.end_array();

  w.key("unserved").begin_array();
  for (const Node n : report.solution.unserved) {
    w.begin_object().key("id").value(p.node(n).id).key("reason").value(unserved_reason(p, n)).end_object();
  }
  w.end_array();
  w.end_object();
  return w.str() + "\n";
}

std::string driver_manifest(const Problem& p, const SolveReport& report) {
  std::ostringstream out;
  for (std::size_t i = 0; i < report.solution.routes.size(); ++i) {
    const RouteEval ev = evaluate_route(p, report.solution.routes[i], true);
    out << "==== Vehicle " << (i + 1) << " ====\n";
    out << "Load " << ev.load << " units at " << p.depot.name << ", leave " << format_clock(ev.depart_depot)
        << ", back by " << format_clock(ev.return_depot) << " (" << fixed(ev.km) << " km)\n\n";
    int seq = 1;
    for (const Visit& v : ev.visits) {
      const Stop& s = p.node(v.node);
      out << "  " << seq++ << ". " << format_clock(v.start) << "  " << s.name << " [" << s.id << "]  drop "
          << s.demand << "  (window " << format_clock(s.open) << "-" << format_clock(s.close) << ")";
      if (v.start > v.arrival + 0.5) out << "  wait " << fixed(v.start - v.arrival, 0) << " min";
      out << "\n";
      if (!s.notes.empty()) out << "       note: " << s.notes << "\n";
    }
    out << "\n";
  }
  if (!report.solution.unserved.empty()) {
    out << "==== Not scheduled ====\n";
    for (const Node n : report.solution.unserved) {
      out << "  - " << p.node(n).name << " [" << p.node(n).id << "]: " << unserved_reason(p, n) << "\n";
    }
  }
  return out.str();
}

std::string summary_text(const Problem& p, const SolveReport& report) {
  std::ostringstream out;
  const auto& c = report.construction;
  const auto& f = report.final;
  out << "Stops: " << p.stops.size() << "  Vehicles: " << p.fleet.vehicles << " x " << p.fleet.capacity
      << " units\n";
  out << "Savings construction: " << fixed(c.km) << " km, " << c.vehicles_used << " vehicles, " << c.unserved
      << " unserved\n";
  out << "After local search:   " << fixed(f.km) << " km, " << f.vehicles_used << " vehicles, " << f.unserved
      << " unserved";
  if (c.km > 0 && f.unserved == c.unserved) out << "  (" << fixed(100.0 * (c.km - f.km) / c.km) << "% shorter)";
  out << "\n";
  out << "Moves: " << report.moves.two_opt << " 2-opt, " << report.moves.relocate << " relocate, " << report.moves.or_opt << " or-opt, "
      << report.moves.swap << " swap, " << report.moves.cross << " cross in " << report.moves.rounds
      << " rounds, " << fixed(report.elapsed_ms, 2) << " ms\n";
  for (std::size_t i = 0; i < report.solution.routes.size(); ++i) {
    const RouteEval ev = evaluate_route(p, report.solution.routes[i]);
    out << "  Vehicle " << (i + 1) << ": " << report.solution.routes[i].size() << " stops, " << ev.load << "/"
        << p.fleet.capacity << " units, " << fixed(ev.km) << " km, " << format_clock(ev.depart_depot) << "-"
        << format_clock(ev.return_depot) << "\n";
  }
  return out.str();
}

std::string route_svg(const Problem& p, const Solution& s, int width, int height) {
  // Bounding box of every location, padded so markers aren't clipped.
  double min_lat = p.depot.pos.lat, max_lat = p.depot.pos.lat;
  double min_lon = p.depot.pos.lon, max_lon = p.depot.pos.lon;
  for (const Stop& st : p.stops) {
    min_lat = std::min(min_lat, st.pos.lat);
    max_lat = std::max(max_lat, st.pos.lat);
    min_lon = std::min(min_lon, st.pos.lon);
    max_lon = std::max(max_lon, st.pos.lon);
  }
  // Equirectangular: shrink longitude by cos(latitude) so the map isn't stretched.
  const double mid_lat = (min_lat + max_lat) / 2.0 * std::numbers::pi / 180.0;
  const double x_scale = std::cos(mid_lat);
  const double span_x = std::max((max_lon - min_lon) * x_scale, 1e-6);
  const double span_y = std::max(max_lat - min_lat, 1e-6);
  const double margin = 40.0;
  const double legend_h = 30.0;
  const double scale = std::min((width - 2 * margin) / span_x, (height - 2 * margin - legend_h) / span_y);
  auto px = [&](LatLon ll) {
    const double x = margin + (ll.lon - min_lon) * x_scale * scale;
    const double y = margin + legend_h + (max_lat - ll.lat) * scale;
    return std::pair{x, y};
  };

  std::ostringstream out;
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 )" << width << ' ' << height << R"(" width=")"
      << width << R"(" height=")" << height << R"(" font-family="system-ui, sans-serif" font-size="11">)" << "\n";
  out << R"(<rect width="100%" height="100%" fill="#fbfaf7"/>)" << "\n";

  for (std::size_t r = 0; r < s.routes.size(); ++r) {
    const char* colour = kPalette[r % kPalette.size()];
    out << R"(<polyline fill="none" stroke-width="2.5" stroke-linejoin="round" stroke=")" << colour
        << R"(" points=")";
    auto [dx, dy] = px(p.depot.pos);
    out << fixed(dx) << ',' << fixed(dy);
    for (const Node n : s.routes[r]) {
      auto [x, y] = px(p.node(n).pos);
      out << ' ' << fixed(x) << ',' << fixed(y);
    }
    out << ' ' << fixed(dx) << ',' << fixed(dy) << R"("/>)" << "\n";
    int seq = 1;
    for (const Node n : s.routes[r]) {
      auto [x, y] = px(p.node(n).pos);
      out << R"(<g><title>)" << xml_escape(p.node(n).name) << " (vehicle " << (r + 1) << ", stop " << seq
          << R"()</title><circle r="7" fill="white" stroke-width="2" stroke=")" << colour << R"(" cx=")"
          << fixed(x) << R"(" cy=")" << fixed(y) << R"("/><text text-anchor="middle" dy="3.5" font-size="9" x=")"
          << fixed(x) << R"(" y=")" << fixed(y) << R"(">)" << seq << "</text></g>\n";
      ++seq;
    }
  }
  for (const Node n : s.unserved) {
    auto [x, y] = px(p.node(n).pos);
    out << R"(<g><title>)" << xml_escape(p.node(n).name) << R"( (not scheduled)</title><path stroke="#888" stroke-width="2" d="M)"
        << fixed(x - 5) << ',' << fixed(y - 5) << " L" << fixed(x + 5) << ',' << fixed(y + 5) << " M"
        << fixed(x - 5) << ',' << fixed(y + 5) << " L" << fixed(x + 5) << ',' << fixed(y - 5) << R"("/></g>)"
        << "\n";
  }
  auto [dx, dy] = px(p.depot.pos);
  out << R"(<g><title>)" << xml_escape(p.depot.name) << R"( (depot)</title><rect width="14" height="14" fill="#222" x=")"
      << fixed(dx - 7) << R"(" y=")" << fixed(dy - 7) << R"("/></g>)" << "\n";

  // Legend
  double lx = margin;
  for (std::size_t r = 0; r < s.routes.size(); ++r) {
    out << R"(<rect width="14" height="4" y="22" fill=")" << kPalette[r % kPalette.size()] << R"(" x=")"
        << fixed(lx) << R"("/><text y="27" fill="#333" x=")" << fixed(lx + 18) << R"(">Vehicle )" << (r + 1)
        << "</text>\n";
    lx += 80;
  }
  if (!s.unserved.empty()) {
    out << R"(<text y="27" fill="#555" x=")" << fixed(lx) << R"(">x = not scheduled ()" << s.unserved.size()
        << ")</text>\n";
  }
  out << "</svg>\n";
  return out.str();
}

}  // namespace pantryroute
