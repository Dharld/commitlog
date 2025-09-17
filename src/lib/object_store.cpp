#include "object_store.hpp"
#include "zstr.hpp"
#include <filesystem>
#include <fstream>
#include <openssl/sha.h>
#include <zlib.h>


static std::string zlib_compress(const unsigned char* data, size_t len, int level = Z_DEFAULT_COMPRESSION) {
    auto cap = compressBound(len);        // upper bound for compressed size
    std::string out;
    out.resize(cap);

    int ret = compress2(reinterpret_cast<Bytef*>(&out[0]), &cap,
                        reinterpret_cast<const Bytef*>(data), len, level);
    if (ret != Z_OK) throw std::runtime_error("zlib compress2 failed");
    out.resize(cap);
    return out;
}

Oid ObjectStore::compute_oid(std::string_view object_bytes) {
    Oid oid{};

    SHA1(
        reinterpret_cast<const unsigned char*>(object_bytes.data()), 
        static_cast<const unsigned long>(object_bytes.size()), 
        oid.bytes
    );

    return oid;
}

// constructors
ObjectStore::ObjectStore(std::filesystem::path root): root_(root) {}

// Private methods
std::filesystem::path ObjectStore::loose_path_for(const Oid& oid) const {
    const std::string hex = oid.to_hex();
    return root_ / hex.substr(0, 2) / hex.substr(2);
}

std::filesystem::path ObjectStore::objects_dir_for(const Oid& oid) const {
    const std::string hex = oid.to_hex();
    return root_ / hex.substr(0, 2);
}

// Public methods
ParsedHeader ObjectStore::parse_header(std::string_view object_bytes) {
    const std::size_t sp = object_bytes.find(' ');
    if (sp == std::string_view::npos) {
        throw std::runtime_error("invalid object: missing space after type");
    }

    ParsedHeader h;
    h.type = std::string(object_bytes.substr(0, sp));

    const std::size_t nul = object_bytes.find('\0', sp + 1);
    if (nul == std::string_view::npos) {
        throw std::runtime_error("invalid object: missing NUL after size");
    }

    // Parse decimal size
    std::size_t declared_size = 0;
    for (std::size_t i = sp + 1; i < nul; ++i) {
        char c = object_bytes[i];
        if (c < '0' || c > '9') {
            throw std::runtime_error("invalid object: size not decimal");
        }
        declared_size = declared_size * 10 + static_cast<std::size_t>(c - '0');
    }

    h.size = declared_size;
    h.header_len = nul + 1;

    // quick consistency check
    if (object_bytes.size() < h.header_len + h.size) {
        throw std::runtime_error("invalid object: size mismatch");
    }

    return h;
}

PutObjectResult ObjectStore::put_object_if_absent(std::string_view object_bytes) {
    ParsedHeader h = ObjectStore::parse_header(object_bytes);
    const Oid oid = ObjectStore::compute_oid(object_bytes);

    // Write the object to the store
    auto dir = objects_dir_for(oid);
    auto file = loose_path_for(oid);
    
    // The object has already been created
    if(std::filesystem::exists(file)) {
        return PutObjectResult{oid, false, h.type, h.size};
    }
    
    // Store the object
    std::filesystem::create_directories(dir);
    
    std::string compressed = zlib_compress(
            reinterpret_cast<const unsigned char*>(object_bytes.data()), 
            object_bytes.size());
    std::filesystem::path tmp = file / ".tmp";
 
    {
        std::ofstream out(tmp, std::ios_base::binary);
        if(!out) throw std::runtime_error("cannot open tmp object for write");

        out.write(compressed.data(), compressed.size());

        if (!out) throw std::runtime_error("write failed");
        out.close();
    }

    std::filesystem::rename(tmp, file);
    return PutObjectResult{oid, true, h.type, h.header_len};
}

std::optional<ReadObjectResult> ObjectStore::read_object(const Oid& oid) const {
    // 1. Compute loose object path from OID
    auto file = loose_path_for(oid);

    // 2. If it doesn’t exist, bail
    if (!std::filesystem::exists(file)) {
        return std::nullopt;
    }

    // 3. Read compressed bytes
    zstr::ifstream in(file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open object for read: " + file.string());
    }
    std::string object_bytes((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());

    

    // 5. Parse header (type, size, header_len)
    ParsedHeader h = ObjectStore::parse_header(object_bytes);

    // 6. Slice out the raw content
    std::string content(object_bytes.substr(h.header_len));

    // 7. Return structured result
    return ReadObjectResult{h.type, h.size, std::move(content)};
}

std::vector<Oid> ObjectStore::get_all_objects() const {
    std::vector<Oid> oids;

    for(const auto& subdir: std::filesystem::directory_iterator(root_)) {
        if (subdir.is_directory()) continue;

        const std::string dir_name = subdir.path().filename().string();
        if(dir_name.size() != 2) continue;

        for(const auto& file: std::filesystem::directory_iterator(subdir)) {
            if (!file.is_regular_file()) continue;

            const std::string file_name = file.path().filename().string();
            const std::string hex = dir_name + file_name; // 2 + 38 = 40 chars

            if (hex.size() == SHA_DIGEST_LENGTH * 2) {
                if (auto oid = Oid::from_hex(hex)) {
                    oids.push_back(*oid);
                }
            }
        }
    }
    
    return oids;
}

bool ObjectStore::has_object(const Oid& oid) const {
    auto file = loose_path_for(oid);
    return std::filesystem::exists(file);
}


