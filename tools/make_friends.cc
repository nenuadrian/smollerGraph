// Writes a made-up social graph for benchmarks: people who each know a number
// of others, picked at random, as smollerGraph facts and as CSV files that
// Neo4j and Kùzu can import, along with the people to start queries from.
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace {

bool ParseCount(std::string_view text, uint32_t& count) {
  auto [end, failed] =
      std::from_chars(text.data(), text.data() + text.size(), count);
  return failed == std::errc() && end == text.data() + text.size() && count > 0;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t people = 0;
  uint32_t friends = 0;
  if (argc != 4 || !ParseCount(argv[1], people) ||
      !ParseCount(argv[2], friends)) {
    std::cerr << "usage: make_friends <people> <friends each> <folder>\n";
    return 1;
  }
  std::string folder = argv[3];
  std::error_code failed;
  std::filesystem::create_directories(folder, failed);
  if (failed) {
    std::cerr << "make_friends: can't make " << folder << ": "
              << failed.message() << '\n';
    return 1;
  }

  std::ofstream facts(folder + "/friends.txt");
  std::ofstream nodes(folder + "/people.csv");
  std::ofstream relationships(folder + "/knows.csv");
  std::ofstream starts(folder + "/starts.txt");
  for (uint32_t i = 1; i <= people; ++i) nodes << "Person " << i << '\n';

  // A fixed seed, and the generator's raw numbers, which the standard defines
  // to the bit, so every run on every machine writes the same graph.
  std::mt19937 random(42);
  for (uint32_t i = 1; i <= people; ++i) {
    for (uint32_t f = 0; f < friends; ++f) {
      // Nobody is their own friend, so a walk never uses one relationship
      // twice, which Neo4j's patterns don't allow.
      uint32_t other = 0;
      do {
        other = 1 + random() % people;
      } while (other == i && people > 1);
      facts << "(Person " << i << ")-[:KNOWS]->(Person " << other << ")\n";
      relationships << "Person " << i << ",Person " << other << '\n';
    }
  }
  for (int q = 0; q < 1000; ++q) {
    starts << "Person " << 1 + random() % people << '\n';
  }
  std::cout << people << " people, " << uint64_t{people} * friends
            << " relationships in " << folder << '\n';
  return 0;
}
