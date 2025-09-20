#include "object_store.hpp"
#include <openssl/sha.h>
#include <string>
#include <string_view>
#include <vector>

struct Entry {
    std::string mode;
    std::string name;
    Oid oid;

   std::string get_type() const {
        if (mode == "040000") return "tree";       // directory
        if (mode == "100644" || mode == "100755") return "blob"; // file
        if (mode == "120000") return "blob";       // symlink (still stored as blob)
        if (mode == "160000") return "commit";     // submodule

        return "unknown"; // fallback for unexpected modes
    }
};

class EntryParser {
public:
    // payload = bytes after "tree <size>\0"
    explicit EntryParser(std::string_view payload) : payload_(payload) {}

    // Returns true and fills `out` if an entry was parsed; false = no more.
    // On corruption, return false and set an error flag (or throw—your call).
    bool next(Entry& out);

    // Convenience: parse all remaining entries.
    std::vector<Entry> parse_all();  // uses internal cursor

    // Optional: error reporting if you don’t throw
    bool ok() const { return ok_; }
    std::string_view error() const { return err_; }

private:
    std::string_view payload_;
    std::size_t pos_ = 0;
    bool ok_ = true;
    std::string_view err_{};
};

