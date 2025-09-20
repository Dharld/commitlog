#include "object_store.hpp"
#include <map>

namespace fs = std::filesystem;

struct IndexEntry {
  std::string path;
  std::string mode;
  Oid oid;
};

class Index {
public:
  explicit Index(fs::path index_path);

  void load();
  void upsert(const IndexEntry &e);
  void flush();

  const std::map<std::string, IndexEntry> &entries() const;

private:
  fs::path path_;
  std::map<std::string, IndexEntry> by_path_;
};
