#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "lib/command.h"
#include "lib/zstr.hpp"

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

            std::string payload = input_content.substr(input_content.find("\0") + 1);
            std::cout << payload << std::flush;
       }
    }
    else {
        std::cerr << "Unknown command " << command << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
