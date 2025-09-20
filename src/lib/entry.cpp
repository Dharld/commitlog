#include <algorithm>
#include <cstddef>
#include <cstring>
#include <openssl/sha.h>
#include <string_view>
#include <vector>
#include "entry.hpp"

bool EntryParser::next(Entry& out) {
    if(!ok_) return false; // sticky
    if(pos_ >= payload_.size()) return false; // passed the limit
    
    // Parse the mode
    const std::size_t mode_begin = pos_;
    const std::size_t sp = payload_.find(" ", mode_begin);

    if (sp == std::string_view::npos || sp == mode_begin) {
        ok_ = false;
        err_ = "Missing or corrupted mode";
        return false;
    }

    out.mode.assign(payload_, mode_begin, sp - mode_begin);
    
    // Parse the name
    const std::size_t name_begin =sp + 1;
    const std::size_t nul = payload_.find('\0', mode_begin);

    if (nul == std::string_view::npos) {
        ok_ = false;
        err_ = "Missing or corrupted name";
        return false;
    }

    out.name.assign(payload_, name_begin, nul - name_begin);

    // Parse the 20 bits Oid
    const size_t oid_begin = nul + 1;
    const size_t oid_length = SHA_DIGEST_LENGTH;

    if (oid_begin + oid_length >= payload_.size()) {
        ok_ = false;
        err_ = "Corrupted Oid";
        return false;
    }
    
    const unsigned char* ptr_ = reinterpret_cast<const unsigned char*>(payload_.data() + oid_begin);
    std::copy(ptr_, ptr_ + oid_length, out.oid.bytes);
    pos_ = oid_begin + oid_length;
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
