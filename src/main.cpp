#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <openssl/sha.h>
#include <zlib.h>
#include "lib/command.hpp"
#include "lib/zstr.hpp"
#include "lib/entry.hpp"

namespace fs = std::filesystem;

std::string get_sha1_str(unsigned char* digest) {
    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
}

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

std::vector<unsigned char> read_file_bytes(fs::path file_path) {
    std::ifstream file(file_path, std::ios::in | std::ios::binary);

    if (!file) {
        std::cerr << "Error: Could not open file " << file_path << std::endl;
        throw EXIT_FAILURE;
    }

    file.seekg(0, std::ios::end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(file_size);

    file.read(reinterpret_cast<char*>(buffer.data()), file_size);

    if (!file) {
        std::cerr << "Error: Could not read entire file. Read only " << file.gcount() << " bytes." << std::endl;
        throw EXIT_FAILURE;
    }

    return buffer;
}

int main(int argc, char *argv[])
{
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    if (argc < 2) {
        std::cerr << "No command provided.\n";
        return EXIT_FAILURE;
    }

    std::string command = argv[1];

    if (command == "init") {
        try {
            std::filesystem::create_directory(".git");
            std::filesystem::create_directory(".git/objects");
            std::filesystem::create_directory(".git/refs");

            std::ofstream headFile(".git/HEAD");
            if (headFile.is_open()) {
                headFile << "ref: refs/heads/main\n";
                headFile.close();
            } else {
                std::cerr << "Failed to create .git/HEAD file.\n";
                return EXIT_FAILURE;
            }

            std::cout << "Initialized git directory\n";
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << e.what() << '\n';
            return EXIT_FAILURE; 
        }
    } 
    else if (command == "cat-file") {
       CatFileCommand cmd{};

       for (int i = 0; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-p") cmd.print_payload = true;
            else if (arg == "-t") cmd.print_type = true;
            else cmd.object_id = arg;
       }

       if (cmd.print_payload) {
            std::string dir_name = cmd.object_id.substr(0, 2);
            std::string sha1 = cmd.object_id.substr(2);
            
            // Decompress the file
            fs::path path = ".git/objects/" + dir_name + "/" + sha1;

            zstr::ifstream input(path);
            if(!input.is_open()) {
                std::cerr << "Impossible to decompress the file" << std::endl;
                return EXIT_FAILURE;
            }

            std::ostringstream ss;
            ss << input.rdbuf();
            std::string input_content = ss.str();
            input.close();

            std::string payload = input_content.substr(input_content.find('\0') + 1);
            std::cout << payload << std::flush;
       }
    } else if (command == "hash-object") {
        HashObjectCommand cmd{};

         for (int i = 0; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-w") cmd.write_file = true;
            else cmd.file_name = arg;
        }


        // Construct the absolute path
        fs::path current_dir = fs::current_path();
        fs::path full_path = current_dir / fs::relative(cmd.file_name);

        std::vector<unsigned char> payload = read_file_bytes(full_path);

        // Prepend the header to our buffer
        std::string header = "blob " + std::to_string(payload.size()) + '\0';

        std::vector<unsigned char> blob_object;
        blob_object.insert(blob_object.end(), header.begin(), header.end());
        blob_object.insert(blob_object.end(), payload.begin(), payload.end());
    
        // Encode using sha1
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1(blob_object.data(), blob_object.size(), digest);

        // Write to file using the sha1 as a key
        std::string sha1_str = get_sha1_str(digest);
        

        std::cout << sha1_str << "\n";

        if (cmd.write_file) {
            std::string dir_name = sha1_str.substr(0, 2);
            std::string file_name = sha1_str.substr(2);

            fs::path obj_dir = ".git/objects" / fs::path(dir_name);
            fs::path obj_path = obj_dir / file_name;
            
            // Create the directory and the file
            fs::create_directories(obj_dir);
            
            std::string compressed = zlib_compress(blob_object.data(), blob_object.size());

            fs::path tmp = obj_dir / (file_name + ".tmp");

            {
                std::ofstream out(tmp.string(), std::ios_base::binary);
                if(!out) throw std::runtime_error("cannot open tmp object for write");

                out.write(compressed.data(), compressed.size());

                if (!out) throw std::runtime_error("write failed");
                out.close();
            }

            fs::rename(tmp, obj_path);

        };
    }
    else if(command == "ls-tree") {
        LSTreeCommand cmd;
        
        for (int i = 0; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-r") cmd.print_recursive = true;
            if (arg == "--name-only") cmd.print_name_only = true;
            else cmd.object_id = arg;
        }


        // read dir/filename
        std::string obj_dir = cmd.object_id.substr(0, 2);
        std::string obj_name = cmd.object_id.substr(2);

        fs::path root_dir = fs::absolute(".git/objects");
        fs::path full_path = root_dir / obj_dir / obj_name;
        
        // Inflate Object
        zstr::ifstream input(full_path, std::ios::binary);

        if (!input) {
            std::cerr << "Impossible to open the file at: " << full_path << std::endl;
            throw EXIT_FAILURE;
        }
        
        // Skip the header;
        char ch;
        std::string header;
        while (input.get(ch)) {
            if (ch == '\0') break;
            header.push_back(ch);
        }
        
        if (header.find("tree") != 0) {
            std::cerr << "The sha1 provided is not associated to a valid tree" << std::endl;
            throw EXIT_FAILURE;
        }

        // Parse the entries
        std::vector<char> buffer{
            std::istreambuf_iterator<char>(input), 
            std::istreambuf_iterator<char>()
        };

        std::string_view payload{buffer.data(), buffer.size()};

        EntryParser parser{payload};
        std::vector<Entry> entries = parser.parse_all();
        
        for(auto e: entries) {
            if (cmd.print_name_only) {
                std::cout << e.name << std::endl; 
            } else {
                std::cout << e.mode << " " << e.get_type() 
                          << " " << e.oid.to_hex() << " " << e.name << std::endl;
            }
        }
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
