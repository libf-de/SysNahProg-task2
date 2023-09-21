#include <iostream>
#include <dirent.h>
#include<vector>
#include<filesystem>
#include <set>
#include <fnmatch.h>
#include <sys/stat.h>
#include <cstdlib>
#include "argparse/argparse.hpp"

std::string ensureTrailingSlash(const std::string& path);
void walkDir(const std::string& dir);
std::set<std::string> seenPaths;
argparse::ArgumentParser args("find");
unsigned char fileType;


int main(int argc, char *argv[]) {
    args.add_argument("directory_name").required();
    args.add_argument("-name").default_value("");
    args.add_argument("-type"); //TODO: Limit to f/d
    args.add_argument("-follow").default_value(false).implicit_value(true); //TODO: TR
    args.add_argument("-xdev").default_value(false).implicit_value(true);

    try {
        args.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << args;
        std::exit(1);
    }

    if(args.is_used("-type")) {
        auto typeStr = args.get<std::string>("-type");
        if(typeStr == "f") fileType = DT_REG;
        else if(typeStr == "d") fileType = DT_DIR;
        else {
            std::cerr << "find: -type: " << typeStr << ": unknown type" << std::endl;
            std::cerr << args;
            std::exit(1);
        }
    }


    std::string dirIn = "test";

    std::string dir = dirIn;
    //if(!dir.starts_with("/")) dir = std::filesystem::current_path().string() + "/" + dirIn;

    //std::cout << dir << std::endl;

    walkDir(args.get<std::string>("directory_name"));

    //std::cout << "Hello, World!" << std::endl;
    return 0;
}

bool shallWalk(const char * dir) {
    char *resolved_name = nullptr;
    char * c_target = realpath(dir, resolved_name);
    std::string fullDir(c_target);
    if(seenPaths.find(fullDir) == seenPaths.end()) {
        seenPaths.insert(fullDir);
        return true;
    }
    return false;
}

void walkDir(const std::string& dir) {
    if(!shallWalk(dir.c_str())) return; //dir was already walked
    //printf("walkDir(%s)", dir.c_str());

    DIR *directory = opendir(dir.c_str());

    if (directory) {
        struct dirent *entry;

        // Durchlaufe das Verzeichnis und liste die Dateien auf
        while ((entry = readdir(directory)) != nullptr) {
            if(strcmp(entry->d_name, ".") == 0) continue;
            if(strcmp(entry->d_name, "..") == 0) continue;

            if( (!args.is_used("-name") ||
                 fnmatch(args.get<std::string>("-name").c_str(), entry->d_name, 0) == 0)
                &&
                (!args.is_used("-type") ||
                 entry->d_type == fileType))
                printf("%s/%s\n", dir.c_str(), entry->d_name);

            if(entry->d_type == DT_DIR) {
                walkDir(dir + "/" + entry->d_name);
            } else if(entry->d_type == DT_LNK) {
                char *resolved_name = nullptr;
                char * c_target = realpath((dir + "/" + entry->d_name).c_str(), resolved_name);
                std::string target(c_target); //TODO: Maybe use readlink() instead?
                struct stat statOut{};
                if(stat((dir + "/" + entry->d_name).c_str(), &statOut) != 0 &&
                   statOut.st_mode == S_IFDIR &&
                   shallWalk(c_target)) {
                    walkDir(target);
                }
            }
        }

        // Schließe das Verzeichnis
        closedir(directory);
    } else {
        perror("Fehler beim Öffnen des Verzeichnisses");
    }
}
