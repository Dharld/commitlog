#include "lib/commands.hpp"
#include "lib/object_store.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Return the REPO ROOT (parent of .git), stop at filesystem root.
fs::path find_repo_root(fs::path start = fs::current_path()) {
  auto dir = start;
  while (true) {
    if (fs::exists(dir / ".git") && fs::is_directory(dir / ".git"))
      return dir; // <-- repo root
    auto parent = dir.parent_path();
    if (parent == dir)
      break; // <-- stop at '/'
    dir = parent;
  }
  throw std::runtime_error("Not a git repository");
}

int main(int argc, char *argv[]) {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  if (argc < 2) {
    std::cerr << "usage: git <command> [args...]\n";
    return EXIT_FAILURE;
  }
  std::string cmd_name = argv[1];
  auto cmd = make_command(cmd_name);
  if (!cmd) {
    std::cerr << "Unknown command: " << cmd_name << "\n";
    return EXIT_FAILURE;
  }

  // Build codec for the store
  auto codec = make_zlib_codec();

  // Special-case init: don't try to discover a repo before it exists.
  if (cmd_name == "init") {
    fs::path objects =
        fs::current_path() / ".git" / "objects";  // may not exist yet
    ObjectStore store{std::move(codec), objects}; // ctor should be lazy
    return cmd->execute(argc, argv, store);
  }

  // All other commands require an existing repo
  fs::path repo_root;
  try {
    repo_root = find_repo_root();
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
  ObjectStore store{std::move(codec), repo_root / ".git" / "objects"};
  return cmd->execute(argc, argv, store);
}
