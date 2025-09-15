#include <string>
struct CatFileCommand {
    std::string object_id;
    bool print_payload;
    bool print_type;
};

struct HashObjectCommand {
    std::string file_name;
    bool write_file;
};
