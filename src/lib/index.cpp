#include "index.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

Index::Index(fs::path index_path) {
  path_ = index_path;
}

void Index::upsert(const IndexEntry& e) {
  by_path_[e.path] = e;
}

void Index::load() {
  std::ifstream in(path_);

  if (!in) {
    return;
  }

  std::string line;
  while (std::getline(in, line)) {
    if(line.empty()) continue;
    
    std::istringstream iss(line);
    std::string mode, hex_oid, rel_path;
   
    if (!(iss >> mode >> hex_oid >> rel_path)) {
      throw std::runtime_error("malformed index line: " + line);
    };
    
    auto oid_opt = Oid::from_hex(hex_oid);
    if (!oid_opt) {
      throw std::runtime_error("invalid OID: " + hex_oid);
    }

    IndexEntry e{rel_path, mode, *oid_opt};
    by_path_[rel_path] = e;
  } 
}

void Index::flush() {
  auto tmp = path_;
  tmp += ".tmp";

  std::ofstream out(tmp, std::ios::trunc);
  if (!out) {
    throw std::runtime_error("cannot open index temp file: " + tmp.string());
  }

  for (auto const& [path, entry]: by_path_) {
    out << entry.mode << ' '
        << entry.oid.to_hex() << ' '
        << entry.path << '\n';
  }

  if(!out) {
    throw std::runtime_error("Error while writing to the temp file.");
  }

  out.close();

  fs::rename(tmp, path_);
}
