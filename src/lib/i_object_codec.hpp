#pragma once

#include <memory>
#include <string>
#include <string_view>

class IObjectCodec {
public:
    virtual ~IObjectCodec() = default;
    virtual std::string compress(std::string_view uncompressed) = 0;
    virtual std::string decompress(std::string_view compressed) = 0;
};

std::unique_ptr<IObjectCodec> make_zlib_codec();
