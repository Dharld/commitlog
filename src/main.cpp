#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <openssl/sha.h>
#include "lib/command.h"
#include "lib/zstr.hpp"

using std::cout;
namespace fs = std::filesystem;

std::string get_sha1_str(unsigned char* digest) {
    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
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
            std::string path = ".git/objects/" + dir_name + "/" + sha1;

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
        std::string header = "blob " + std::to_string(payload.size()) + "\0";

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

            fs::create_directories(obj_dir);

            if(!fs::exists(obj_path)) {
                fs::path tmp = obj_dir / (file_name + ".tmp");

                {
                    zstr::ofstream out(tmp.string(), std::ios::binary);
                    if(!out) throw std::runtime_error("cannot open tmp object for write");

                    out.write(reinterpret_cast<char*>(blob_object.data()), blob_object.size());
                    if(!out) throw std::runtime_error("cannot write blob object to destination");
                }

                fs::rename(tmp, obj_path);
            }
        };
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
