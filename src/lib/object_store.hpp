#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <openssl/sha.h>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <zlib.h>



struct Oid {
    unsigned char bytes[SHA_DIGEST_LENGTH];

    bool operator==(const Oid&) const noexcept = default;
    bool operator!=(const Oid& oid) { return !operator==(oid); }

    std::string to_hex() const {
        std::ostringstream ss;

        for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << bytes[i]; 
        }

        return ss.str();
    }

    static std::optional<Oid> from_hex(std::string_view hex) {
        if (hex.size() != SHA_DIGEST_LENGTH * 2) {
            return std::nullopt; // must be 40 characters for SHA-1
        }

        Oid oid{};
        auto hexval = [](char c) -> int {
            if ('0' <= c && c <= '9') return c - '0';
            if ('a' <= c && c <= 'f') return c - 'a' + 10;
            if ('A' <= c && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        for (std::size_t i = 0; i < SHA_DIGEST_LENGTH; ++i) {
            int hi = hexval(hex[2 * i]);
            int lo = hexval(hex[2 * i + 1]);
            if (hi < 0 || lo < 0) {
                return std::nullopt; // invalid hex digit
            }
            oid.bytes[i] = static_cast<unsigned char>((hi << 4) | lo);
        }

        return oid;
    }
};

struct PutObjectResult {
    Oid oid;
    bool inserted;
    std::string type;
    std::size_t size;
    
};

struct ReadObjectResult {
    std::string type;
    std::size_t size;
    std::string content;
};

struct ParsedHeader {
    std::string type;     // "blob" | "tree" | "commit"
    std::size_t size;     // content size
    std::size_t header_len; // number of bytes up to and including the NUL
};

class ObjectStore {
public:
    explicit ObjectStore(std::filesystem::path repo_root);
    PutObjectResult put_object_if_absent(std::string_view);
    std::optional<ReadObjectResult> read_object(const Oid&) const;

    // Existence check
    bool has_object(const Oid&) const; 

    // Get all the objects within ./git/objects
    std::vector<Oid> get_all_objects() const;
    static ParsedHeader parse_header(std::string_view);
private:
    static Oid compute_oid(std::string_view);

    std::filesystem::path loose_path_for(const Oid& oid) const;
    std::filesystem::path objects_dir_for(const Oid& oid) const;

    std::filesystem::path root_;
};
