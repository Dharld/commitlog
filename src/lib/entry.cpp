#include <vector>
#include "entry.hpp"

bool EntryParser::next(Entry& out) {
    if (!ok_) return false;                 // sticky error
    if (pos_ >= payload_.size()) return false; // no more entries

    // 1) mode: ASCII until ' '
    const std::size_t mode_begin = pos_;
    const std::size_t sp = payload_.find(' ', mode_begin);
    if (sp == std::string_view::npos || sp == mode_begin) {
        ok_ = false; err_ = "corrupt tree: missing or empty mode";
        return false;
    }
    out.mode.assign(payload_.substr(mode_begin, sp - mode_begin));

    // 2) name: bytes until '\0'
    const std::size_t name_begin = sp + 1;
    if (name_begin >= payload_.size()) {
        ok_ = false; err_ = "corrupt tree: missing name";
        return false;
    }
    const std::size_t nul = payload_.find('\0', name_begin);
    if (nul == std::string_view::npos) {
        ok_ = false; err_ = "corrupt tree: missing NUL before OID";
        return false;
    }
    out.name.assign(payload_.substr(name_begin, nul - name_begin));

    // 3) 20 raw bytes of OID follow the NUL
    const std::size_t oid_begin = nul + 1;
    constexpr std::size_t OID_LEN = 20;
    if (payload_.size() - oid_begin < OID_LEN) {
        ok_ = false; err_ = "corrupt tree: truncated OID";
        return false;
    }
    // copy raw bytes into Oid
    const unsigned char* src = reinterpret_cast<const unsigned char*>(payload_.data() + oid_begin);
    std::copy(src, src + OID_LEN, out.oid.bytes);

    // 4) advance cursor
    pos_ = oid_begin + OID_LEN;
    return true;
}

std::vector<Entry> EntryParser::parse_all() {
    std::vector<Entry> out;
    out.reserve(32); // heuristic
    Entry e;
    while (next(e)) {
        out.push_back(e);
        e = Entry{}; // reset for next iteration
    }
    return out;
}
