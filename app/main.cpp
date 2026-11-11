#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "pantryroute/cli.hpp"

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  try {
    return pantryroute::run_cli(args, std::cout, std::cerr);
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
}
