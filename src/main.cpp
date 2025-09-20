#include <cstdlib>
#include <filesystem>
#include <ios>
#include <iostream>
#include <ostream>

#include "lib/commands.hpp"
#include "lib/entry.hpp"
#include "lib/i_object_codec.hpp"
#include "lib/object_store.hpp"

#include <openssl/sha.h>
#include <zlib.h>

namespace fs = std::filesystem;

fs::path find_repo_root(fs::path start = fs::current_path()) {
  auto dir = start;
  while (true) {
    if (fs::exists(dir / ".git")) {
      return dir / ".git";
    }

    if (dir.has_parent_path()) {
      dir = dir.parent_path();
    } else {
      throw std::runtime_error("Not a git repository (or any parent up to /)");
    }
  }
}

int main(int argc, char *argv[]) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  auto git_dir = find_repo_root();
  ObjectStore store{make_zlib_codec(), git_dir / "objects"};

  auto cmd = make_command(argv[1]);

  if (!cmd) {
    std::cerr << "Unknown command: " << argv[1] << "\n";
    return EXIT_FAILURE;
  }

  return cmd->execute(argc, argv, store);
}
