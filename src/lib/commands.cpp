// commands.cpp
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <openssl/sha.h>        // for SHA1 in hash-object (no-write path)

#include "commands.hpp"
#include "object_store.hpp"
#include "entry.hpp"

namespace fs = std::filesystem;
struct ICommand;

// -------------------- Small helpers (local to this TU) --------------------
static std::vector<unsigned char> read_file_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        throw std::runtime_error("could not open file: " + p.string());
    }
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)),
                                       std::istreambuf_iterator<char>());
}

static std::string sha1_hex(std::string_view bytes) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(bytes.data()),
         bytes.size(), digest);
    // hex encode
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.resize(SHA_DIGEST_LENGTH * 2);
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        out[2*i]   = kHex[(digest[i] >> 4) & 0xF];
        out[2*i+1] = kHex[digest[i] & 0xF];
    }
    return out;
}

// ------------------------------ init -------------------------------------

struct InitCommand : ICommand {
  const char* name() const override { return "init"; }
  int execute(int /*argc*/, char** /*argv*/, ObjectStore& /*store*/) override {
    try {
      fs::create_directory(".git");
      fs::create_directory(".git/objects");
      fs::create_directory(".git/refs");

      std::ofstream head(".git/HEAD");
      if (!head) {
        std::cerr << "Failed to create .git/HEAD\n";
        return EXIT_FAILURE;
      }
      head << "ref: refs/heads/main\n";
      std::cout << "Initialized git directory\n";
      return EXIT_SUCCESS;
    } catch (const fs::filesystem_error& e) {
      std::cerr << e.what() << '\n';
      return EXIT_FAILURE;
    }
  }
};

// ---------------------------- cat-file -----------------------------------

struct CatFileCommand : ICommand {
  const char* name() const override { return "cat-file"; }
  int execute(int argc, char** argv, ObjectStore& store) override {
    // parse flags/args (skip program name and command)
    bool print_payload = false, print_type = false;
    std::string oid_hex;

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-p") print_payload = true;
      else if (arg == "-t") print_type = true;
      else oid_hex = std::move(arg);
    }

    if (print_payload == print_type) {
      std::cerr << "cat-file: need exactly one of -p or -t\n";
      return EXIT_FAILURE;
    }
    if (oid_hex.size() != 40) {
      std::cerr << "Invalid oid length (need 40 hex chars)\n";
      return EXIT_FAILURE;
    }
    auto maybe_oid = Oid::from_hex(oid_hex);
    if (!maybe_oid) {
      std::cerr << "Invalid oid\n";
      return EXIT_FAILURE;
    }

    auto obj = store.read_object(*maybe_oid);
    if (!obj) {
      std::cerr << "Object not found\n";
      return EXIT_FAILURE;
    }

    if (print_type) {
      std::cout << obj->type << "\n";
      return EXIT_SUCCESS;
    }

    // binary-safe print for payload (trees contain NULs)
    std::cout.write(obj->content.data(),
                    static_cast<std::streamsize>(obj->content.size()));
    std::cout.flush();
    return EXIT_SUCCESS;
  }
};

// --------------------------- hash-object ---------------------------------

struct HashObjectCommand : ICommand {
  const char* name() const override { return "hash-object"; }
  int execute(int argc, char** argv, ObjectStore& store) override {
    bool write = false;
    std::string file_name;

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "-w") write = true;
      else file_name = std::move(arg);
    }

    if (file_name.empty()) {
      std::cerr << "usage: hash-object [-w] <path>\n";
      return EXIT_FAILURE;
    }

    const fs::path full = fs::absolute(file_name);
    std::vector<unsigned char> bytes;
    
    try {
      bytes = read_file_bytes(full);
    } catch (const std::exception& e) {
      std::cerr << e.what() << "\n";
      return EXIT_FAILURE;
    }

    // Build UNCOMPRESSED object: "blob <size>\0" + content
    const std::string header = "blob " + std::to_string(bytes.size()) + '\0';
    std::string object_bytes;
    object_bytes.reserve(header.size() + bytes.size());
    object_bytes.append(header);
    object_bytes.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    if (write) {
      auto res = store.put_object_if_absent(object_bytes);
      std::cout << res.oid.to_hex() << "\n";
      return EXIT_SUCCESS;
    } else {
      // compute sha1 (hex) without writing
      std::cout << sha1_hex(object_bytes) << "\n";
      return EXIT_SUCCESS;
    }
  }
};

// ----------------------------- ls-tree -----------------------------------

struct LsTreeCommand : ICommand {
  const char* name() const override { return "ls-tree"; }
  int execute(int argc, char** argv, ObjectStore& store) override {
    bool name_only = false;
    std::string oid_hex;

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg == "--name-only") name_only = true;
      else if (arg == "-r") { /* reserved; optional */ }
      else oid_hex = std::move(arg);
    }

    if (oid_hex.size() != 40) {
      std::cerr << "usage: ls-tree [--name-only] <40-hex-oid>\n";
      return EXIT_FAILURE;
    }

    auto maybe_oid = Oid::from_hex(oid_hex);
    if (!maybe_oid) {
      std::cerr << "invalid oid\n";
      return EXIT_FAILURE;
    }

    auto obj = store.read_object(*maybe_oid);
    if (!obj) {
      std::cerr << "object not found\n";
      return EXIT_FAILURE;
    }
    if (obj->type != "tree") {
      std::cerr << "not a tree object\n";
      return EXIT_FAILURE;
    }

    std::string_view payload{ obj->content.data(), obj->content.size() };
    EntryParser parser{ payload };
    std::vector<Entry> entries = parser.parse_all();

    for (const auto& e : entries) {
      if (name_only) {
        std::cout << e.name << "\n";
      } else {
        std::cout << e.mode << " " << e.get_type()
                  << " " << e.oid.to_hex() << " " << e.name << "\n";
      }
    }
    return EXIT_SUCCESS;
  }
};

// ---------------------------- write-tree ---------------------------------

struct WriteTreeCommand : ICommand {
  const char* name() const override { return "write-tree"; }
  int execute(int argc, char** argv, ObjectStore& store) override {
    return EXIT_SUCCESS;  
  }
};

// ------------------------------- Factory ---------------------------------

static std::unique_ptr<ICommand> make_cmd(const std::string& name) {
  if (name == "init")        return std::make_unique<InitCommand>();
  if (name == "cat-file")    return std::make_unique<CatFileCommand>();
  if (name == "hash-object") return std::make_unique<HashObjectCommand>();
  if (name == "ls-tree")     return std::make_unique<LsTreeCommand>();
  if (name == "write-tree")  return std::make_unique<WriteTreeCommand>();
  return nullptr;
}

// Expose a single entry point for main.cpp
std::unique_ptr<ICommand> make_command(const std::string& name) {
  return make_cmd(name);
}
