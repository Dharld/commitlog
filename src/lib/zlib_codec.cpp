#include "i_object_codec.hpp"
#include "zstr.hpp"
#include <cstddef>
#include <stdexcept>
#include <zlib.h>

class ZlibCodec: public IObjectCodec {
public:
    std::string compress(std::string_view s) override {
        
        const char* data = s.data(); 
        std::size_t len = s.size();
        int level = Z_DEFAULT_COMPRESSION;

        auto cap = compressBound(len);        // upper bound for compressed size
        std::string out;
        out.resize(cap);

        int ret = compress2(reinterpret_cast<Bytef*>(&out[0]), &cap,
                            reinterpret_cast<const Bytef*>(data), len, level);
        if (ret != Z_OK) throw std::runtime_error("zlib compress2 failed");
        out.resize(cap);
        return out;
    }

    std::string decompress(std::string_view s) override {
        std::stringbuf buf;
        buf.sputn(s.data(), s.size());
        zstr::istream in(&buf);
        return {std::istreambuf_iterator<char>(in), {}};
    }
}; 

std::unique_ptr<IObjectCodec> make_zlib_codec() {
    return std::make_unique<ZlibCodec>();
}
