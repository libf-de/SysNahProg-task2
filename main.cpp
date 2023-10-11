#include<string>
#include<string_view>
#include <iostream>
#include <dirent.h>
#include<filesystem>
#include <set>
#include <fnmatch.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include "argparse/argparse.hpp"
#include <unistd.h>
#include <linux/limits.h>
#include <libexplain/stat.h>

void walkDirRec(const std::string& dir, const std::string& dirPrefix);
void walkDir(const std::string& dir);
std::set<std::string> seenPaths;
std::set<std::string> followedLinks;
std::string absoluteBasePath;
char * rootCwd;
argparse::ArgumentParser args("find");
unsigned char fileType;
dev_t initialDevice = NULL;
std::string callPath;


/*int main(int argc, char *argv[]) {
    std::cout << fnmatch("BB[12]", "BB1", 0) << std::endl;
}*/

int main(int argc, char *argv[]) {
//int kartoffel(int argc, char *argv[]) {
    //if(argc < 1) callPath = "find";
    //else callPath = std::string(argv[0]);
    callPath = "find";

    args.add_argument("directory_name").default_value(".").required();
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

    if(args.is_used("-xdev")) {

    }

    std::string dirName = args.get<std::string>("directory_name");
    if(!dirName.starts_with("/")) {
        rootCwd = getcwd(nullptr, 0);
    }

    char temp[8192];
    std::string abp(realpath(dirName.c_str(), temp));
    absoluteBasePath = abp;

    char rCwd[8192];
    rootCwd = getcwd(rCwd, 8192);

    struct stat statOut{};
    if(stat((dirName).c_str(), &statOut) == 0) {
        //If we don't want to cross devices, store the device of the search root.
        if(args.is_used("-xdev")) initialDevice = statOut.st_dev;

        //Check if search root matches search criteria
        if(
                (!args.is_used("-name") ||
                 fnmatch(args.get<std::string>("-name").c_str(), dirName.c_str(), 0) == 0)
                &&
                (!args.is_used("-type") ||
                    ((S_ISREG(statOut.st_mode) && fileType == DT_REG)
                    || (S_ISDIR(statOut.st_mode) && fileType == DT_DIR))))
            printf("%s\n", dirName.c_str());
    } else {
        //Exit with error if we can't open the search root
        exit(1);
    }


    //if(!args.is_used("-follow")) {
        walkDirRec(dirName, "");
    /*} else {
        walkDir(dirName);
    }*/
    //walkDir(dirName, "");

    //std::cout << "Hello, World!" << std::endl;
    return 0;
}

bool isSymlinkInLoop(const std::string& symlinkPathR, std::set<std::string>& visitedPaths) {
    size_t lastSlash = symlinkPathR.find_last_of("/");
    std::string parentDir;
    std::string symlinkName;
    if(lastSlash == std::string::npos) { //path does not contain /, so symlink is in current directory
        parentDir = std::string(getcwd(nullptr, 0));
        symlinkName = symlinkPathR;
    } else {
        parentDir = symlinkPathR.substr(0, lastSlash);
        symlinkName = symlinkPathR.substr(lastSlash + 1);
    }


    char * fullParentPath = realpath(parentDir.c_str(), nullptr);
    std::string symlinkPath(std::string(fullParentPath) + "/" + symlinkName);

    /*char * fullSymlinkPath = realpath(symlinkPathR.c_str(), nullptr);
    std::string symlinkPath(fullSymlinkPath);*/

    // Überprüfen, ob der Pfad bereits besucht wurde, um eine Endlosschleife zu erkennen.
    if (visitedPaths.find(symlinkPath) != visitedPaths.end()) {
        return true; // Endlosschleife erkannt.
    }

    visitedPaths.insert(symlinkPath);

    struct stat info;
    if (lstat(symlinkPath.c_str(), &info) != 0) {
        // Fehler beim Abrufen der Dateiinformationen.
        return false;
    }

    if (S_ISLNK(info.st_mode)) {
        // Es handelt sich um einen symbolischen Link.

        // Den Ziel-Pfad des Symlinks erhalten.
        char * targetPath = realpath(symlinkPath.c_str(), nullptr);
        return isSymlinkInLoop(targetPath, visitedPaths);
        //char targetPath[PATH_MAX]; // Verwende PATH_MAX, um den Zielpfad zu speichern.
        /*ssize_t bytesRead = readlink(symlinkPath.c_str(), targetPath, sizeof(targetPath));
        if (bytesRead != -1) {
            targetPath[bytesRead] = '\0';

            std::string tgtPth(targetPath, bytesRead);

            //std::string(targetPath);

            // Rekursiv prüfen, ob der Ziel-Pfad in einer Endlosschleife endet.
            return isSymlinkInLoop(tgtPth, visitedPaths);
        }*/
    } else if(S_ISDIR(info.st_mode)) {
        DIR *directory = opendir(symlinkPath.c_str());

        if (directory) {
            struct dirent *entry;

            // Durchlaufe das Verzeichnis und liste die Dateien auf
            while ((entry = readdir(directory)) != nullptr) {
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                if (isSymlinkInLoop(symlinkPath + "/" + std::string(entry->d_name), visitedPaths)) return true;
            }
        }
    }

    return false; // Keine Endlosschleife gefunden.
}

bool shallWalkLink(const char * dir) {
    //fprintf(stderr, "shallWalkLink: %s", dir);
    char *resolved_name = nullptr;
    char * c_target = realpath(dir, resolved_name);
    if(c_target == nullptr) return true;

    std::string fullDir(c_target);
    if(followedLinks.find(fullDir) == followedLinks.end()) {
        followedLinks.insert(fullDir);
        return true;
    }
    return false;
}

void removeWalkedLink(const char * dir) {
    char *resolved_name = nullptr;
    char * c_target = realpath(dir, resolved_name);
    if(c_target == nullptr) return;

    std::string fullDir(c_target);
    if(followedLinks.find(fullDir) == followedLinks.end()) {
        followedLinks.erase(fullDir);
    }
}

void walkDir(const std::string& dir) {
    //if(!shallWalk(dir.c_str())) return; //dir was already walked
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
                //TODO: Handle symlinks
                char *resolved_name = nullptr;
                struct stat statOut{};
                int statRV = stat((dir + "/" + entry->d_name).c_str(), &statOut);
                if(statRV == ELOOP) {
                    fprintf(stderr, "fs loop");
                    exit(1);
                }

                char * c_target = realpath((dir + "/" + entry->d_name).c_str(), resolved_name);
                if (access(c_target, F_OK) != 0) continue;
                std::string target(c_target); //TODO: Maybe use readlink() instead?
                if(statRV == 0 && S_ISDIR(statOut.st_mode) /*&& shallWalkLink(c_target)*/) {
                    walkDir(target);
                } else {
                    std::cout << "statRV = " << statRV;
                }

            }
        }

        // Schließe das Verzeichnis
        closedir(directory);
    } else {
        perror("Fehler beim Öffnen des Verzeichnisses");
    }
}

std::string calculateRelativePath(const std::string& absolutePath, const std::string& basePath) {
    std::vector<std::string> absoluteComponents;
    std::vector<std::string> baseComponents;

    // Split the absolute and base paths into individual components
    size_t pos = 0;
    std::string remainingAbsolute = absolutePath;
    while ((pos = remainingAbsolute.find('/')) != std::string::npos) {
        absoluteComponents.push_back(remainingAbsolute.substr(0, pos));
        remainingAbsolute = remainingAbsolute.substr(pos + 1);
    }
    absoluteComponents.push_back(remainingAbsolute);

    pos = 0;
    std::string remainingBase = basePath;
    while ((pos = remainingBase.find('/')) != std::string::npos) {
        baseComponents.push_back(remainingBase.substr(0, pos));
        remainingBase = remainingBase.substr(pos + 1);
    }
    baseComponents.push_back(remainingBase);

    // Find the longest common prefix
    size_t commonCount = 0;
    while (commonCount < absoluteComponents.size() && commonCount < baseComponents.size() &&
           absoluteComponents[commonCount] == baseComponents[commonCount]) {
        commonCount++;
    }

    std::string relativePath;

    // Construct the relative path
    for (size_t i = commonCount; i < baseComponents.size(); i++) {
        relativePath += "../";
    }

    for (size_t i = commonCount; i < absoluteComponents.size(); i++) {
        relativePath += absoluteComponents[i] + "/";
    }

    // Remove the trailing slash if it exists
    if (!relativePath.empty() && relativePath.back() == '/') {
        relativePath.pop_back();
    }

    return relativePath;
}


void printFinding(struct dirent *entry, const std::string& dir, const std::string& dirPrefix) {
    if( (!args.is_used("-name") ||
         fnmatch(args.get<std::string>("-name").c_str(), entry->d_name, 0) == 0)
        &&
        (!args.is_used("-type") ||
         entry->d_type == fileType))
        if(dirPrefix.length() > 0) {
            if(dirPrefix.ends_with("/")) {
                printf("%s%s/%s\n", dirPrefix.c_str(), dir.c_str(), entry->d_name);
            } else {
                printf("%s/%s/%s\n", dirPrefix.c_str(), dir.c_str(), entry->d_name);
            }
        } else {
            printf("%s/%s\n", dir.c_str(), entry->d_name);
        }
}

void walkDirRec(const std::string& dir, const std::string& dirPrefix) {
    //if(!shallWalk(dir.c_str())) return; //dir was already walked
    if(args.is_used("-xdev")) { //If we don't want to cross devices, check the current dir..
        struct stat statOut{};
        if(stat(dir.c_str(), &statOut) == 0) {
            if(statOut.st_dev != initialDevice) return; //If we would cross devices, return
        } else {
            std::cout << "stat() failed: " << dirPrefix << "/" << dir << std::endl;
        }
    }



    DIR *directory = opendir(dir.c_str());

    if (directory) {
        struct dirent *entry;

        // Durchlaufe das Verzeichnis und liste die Dateien auf
        while ((entry = readdir(directory)) != nullptr) {
            std::string curObject(entry->d_name);
            //const char * curObject = entry->d_name;
            //if(strcmp(curObject, ".") == 0) continue;
            //if(strcmp(curObject, "..") == 0) continue;
            if(curObject == "." || curObject == "..") continue;

            if(args.is_used("-follow") &&
               dirPrefix.ends_with("/") &&
               (dirPrefix.length() + dir.length() + curObject.length()) > 4096 &&
               entry->d_type == DT_DIR) {
                std::cerr << callPath << ": ‘" << dirPrefix << dir << "/" << curObject << "’: Der Dateiname ist zu lang" << std::endl;
                printFinding(entry, dir, dirPrefix);
                return;
            } else if(args.is_used("-follow") &&
                      dirPrefix.ends_with("/") &&
                      (dirPrefix.length() + dir.length() + curObject.length() + 1) > 4096 &&
                      entry->d_type == DT_DIR) {
                std::cerr << callPath << ": ‘" << dirPrefix << "/" << dir << "/" << curObject << "’: Der Dateiname ist zu lang" << std::endl;
                printFinding(entry, dir, dirPrefix);
                return;
            }

            if(entry->d_type != DT_LNK || !args.is_used("-follow"))
                printFinding(entry, dir, dirPrefix); //Print all things that aren't symlinks-to-follow

            if(entry->d_type == DT_DIR) {
                char oldPath[8192];
                getcwd(oldPath, 8192);
                chdir(dir.c_str());
                if(dirPrefix.length() > 0) {
                    walkDirRec(curObject, dirPrefix + dir + "/");
                } else {
                    walkDirRec(curObject, dir + "/");
                }

                chdir(oldPath);
            } else if(entry->d_type == DT_LNK && args.is_used("-follow")) {
                char *resolved_name = nullptr;
                char * c_target = realpath((dir + "/" + curObject).c_str(), resolved_name);
                if (access(c_target, F_OK) != 0) {
                    // Print broken links, but access them anyways.
                    printFinding(entry, dir, dirPrefix);
                    continue;
                }
                std::string target(c_target); //TODO: Maybe use readlink() instead?

                std::set<std::string> visitedPaths;
                if(isSymlinkInLoop((dir + "/" + curObject), visitedPaths)) {
                    std::cerr << callPath << ": Dateisystemschleife erkannt; ‘";
                    if(rootCwd == nullptr) {
                        std::cerr << realpath((dir + "/" + curObject).c_str(), nullptr);
                        std::cerr << "’ ist ein Teil der gleichen Schleife wie ‘";
                        std::cerr << target << "’." << std::endl;
                    } else {
                        std::cerr << dirPrefix << dir << "/" << curObject;
                        std::cerr << "’ ist ein Teil der gleichen Schleife wie ‘";
                        std::cerr << calculateRelativePath(target, rootCwd) << "’." << std::endl;
                    }
                    continue;
                }

                struct stat statOut{};
                int statRV = stat((dir + "/" + curObject).c_str(), &statOut);
                if(statRV == 0 &&
                   S_ISDIR(statOut.st_mode) &&
                   shallWalkLink(c_target)) {
                    printFinding(entry, dir, dirPrefix);

                    char oldPath[8192];
                    getcwd(oldPath, 8192);
                    chdir(dir.c_str());
                    if(dirPrefix.length() > 0) {
                        walkDirRec(curObject, dirPrefix + dir + "/");
                    } else {
                        walkDirRec(curObject, dir + "/");
                    }

                    chdir(oldPath);
                    removeWalkedLink(c_target); //TODO: Remove conversion from char->str->char->str
                } else {
                    printFinding(entry, dir, dirPrefix);
                    //fprintf(stderr, "%s\n", explain_errno_stat(statRV, (dir + curObject).c_str(), &statOut));
                    //fprintf(stderr, "error on %s, %d", (dir + curObject).c_str(), statRV);
                    //exit(1);
                }
            }
        }

        // Schließe das Verzeichnis
        closedir(directory);
    } else {
        perror("Fehler beim Öffnen des Verzeichnisses");
    }
}