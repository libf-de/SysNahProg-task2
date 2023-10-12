#include<string>
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


void walkDirRec(const std::string& dir, const std::string& dirPrefix);
char * rootCwd;
argparse::ArgumentParser args("find");
unsigned char fileType;
dev_t initialDevice = 0;
std::string callPath;
void printFinding(struct dirent *entry, const std::string& dir, const std::string& dirPrefix);

bool isGerman = false;

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    isGerman = std::string(setlocale(LC_ALL, nullptr)).starts_with("de_DE");

    //if(argc < 1) callPath = "find";
    //else callPath = std::string(argv[0]);
    callPath = "find"; // Use find as "executable path" to match GNU find output

    // Parse arguments
    args.add_argument("directory_name").default_value(".").required();
    args.add_argument("-name").default_value("");
    args.add_argument("-type"); //TODO: Limit to f/d
    args.add_argument("-follow").default_value(false).implicit_value(true);
    args.add_argument("-xdev").default_value(false).implicit_value(true);

    try {
        args.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << args;
        std::exit(1);
    }

    // Print help if the specified type is invalid
    if(args.is_used("-type")) {
        auto typeStr = args.get<std::string>("-type");
        if(typeStr == "f") fileType = DT_REG;
        else if(typeStr == "d") fileType = DT_DIR;
        else {
            if(isGerman)
                std::cerr << callPath << ": Unbekanntes Argument für -type: " << typeStr << std::endl;
            else
                std::cerr << callPath << ": Unknown argument to -type: " << typeStr << std::endl;
            std::exit(1);
        }
    }

    // Print warning if -name is used before -xdev
    if(args.is_used("-xdev") && args.is_used("-name")) {
        int xdev_pos;
        int name_pos;
        for(int i = 0; i<argc; ++i) {
            std::string val(argv[i]);
            if(val == "-xdev") xdev_pos = i;
            if(val == "-name") name_pos = i;
        }

        if(xdev_pos > name_pos) {
            if(isGerman)
                std::cerr << callPath << ": Warnung: Sie haben die globale Option -xdev nach dem Argument -name angegeben, aber globale Optionen sind nicht positional, d.h. -xdev wirkt sich sowohl auf die davor als auch auf die danach angegebenen Tests aus. Bitte geben Sie globale Optionen vor anderen Argumenten an." << std::endl;
            else
                std::cerr << callPath << ": warning: you have specified the global option -xdev after the argument -name, but global options are not positional, i.e., -xdev affects tests specified before it as well as those specified after it.  Please specify global options before other arguments." << std::endl;
        }
    }

    auto dirName = args.get<std::string>("directory_name");
    if(dirName.ends_with("/"))
        dirName = dirName.substr(0, dirName.length() - 1);


    if(!dirName.starts_with("/")) {
        rootCwd = getcwd(nullptr, 0);
    }

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
            std::cerr << args.get<std::string>("directory_name") << std::endl;
    } else {
        //Exit with error if we can't open the search root
        if(isGerman)
            std::cerr << "find: ‘" << args.get<std::string>("directory_name") << "’: Datei oder Verzeichnis nicht gefunden" << std::endl;
        else
            std::cerr << "find: '" << args.get<std::string>("directory_name") << "': No such file or directory" << std::endl;
        exit(1);
    }

    walkDirRec(dirName, "");
    return 0;
}

std::string realpathOfSymlink(const std::string& inputPath) {
    size_t lastSlash = inputPath.find_last_of('/');
    std::string parentDir;
    std::string symlinkName;
    if(lastSlash == std::string::npos) { //path does not contain /, so symlink is in current directory
        parentDir = std::string(getcwd(nullptr, 0));
        symlinkName = inputPath;
    } else {
        parentDir = inputPath.substr(0, lastSlash);
        symlinkName = inputPath.substr(lastSlash + 1);
    }


    char * fullParentPath = realpath(parentDir.c_str(), nullptr);
    return (std::string(fullParentPath) + "/" + symlinkName);
}

bool isSymlinkInLoop(const std::string& symlinkPathRelative, std::set<std::string>& visitedPaths) {
    auto symlinkPath = realpathOfSymlink(symlinkPathRelative);

    // Check if path was already visited -> This is a filesystem loop!
    if (visitedPaths.find(symlinkPath) != visitedPaths.end()) return true;

    // Insert the current path into visited paths
    visitedPaths.insert(symlinkPath);

    // Get information about the current path
    struct stat info{};
    if (lstat(symlinkPath.c_str(), &info) != 0) return false;

    if (S_ISLNK(info.st_mode)) {
        // It's a symlink -> check the target
        char * targetPath = realpath(symlinkPath.c_str(), nullptr);
        return isSymlinkInLoop(targetPath, visitedPaths);
    } else if(S_ISDIR(info.st_mode)) {
        // It's a directory -> check if it contains the/a symlink to itself.
        DIR *directory = opendir(symlinkPath.c_str());

        if (directory) {
            struct dirent *entry;

            // Loop over files in directory, skipping current (.) and parent directory (..)
            while ((entry = readdir(directory)) != nullptr) {
                if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
                if (isSymlinkInLoop(symlinkPath + "/" + std::string(entry->d_name), visitedPaths)) return true;
            }
        }
    }

    return false; // No loops found.
}

std::string calculateRelativePath(const std::string& absolutePath, const std::string& basePath) {
    std::vector<std::string> absoluteComponents;
    std::vector<std::string> baseComponents;

    // Split the absolute and base paths into individual components
    size_t pos;
    std::string remainingAbsolute = absolutePath;
    while ((pos = remainingAbsolute.find('/')) != std::string::npos) {
        absoluteComponents.push_back(remainingAbsolute.substr(0, pos));
        remainingAbsolute = remainingAbsolute.substr(pos + 1);
    }
    absoluteComponents.push_back(remainingAbsolute);

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

// Prints a given file or directory if it matches the search criteria = maybe-print
void printFinding(struct dirent *entry, const std::string& dir, const std::string& dirPrefix) {
    // Print if:
    // -name is specified -> filename matches pattern
    // ~AND~
    // -type is specified -> type matches
    if( (!args.is_used("-name") || /* name is specified -> matches pattern */
         fnmatch(args.get<std::string>("-name").c_str(), entry->d_name, 0) == 0)
        &&
        (!args.is_used("-type") || /* type is specified -> matches type */
         entry->d_type == fileType)) {

        if (dirPrefix.length() > 0) { // Print with dirPrefix if there is one
            std::cout << dirPrefix << dir << '/' << entry->d_name << std::endl;
        } else {
            std::cout << dir << '/' << entry->d_name << std::endl;
        }
    }
}

void walkDirRec(const std::string& dir, const std::string& dirPrefix) {
    if(args.is_used("-xdev")) { //If we don't want to cross devices, check the current dir...
        struct stat statOut{};
        if(stat(dir.c_str(), &statOut) == 0) {
            if(statOut.st_dev != initialDevice) return; //If we would cross devices, return
        } else {
            std::cout << "stat() failed: " << dirPrefix << dir << std::endl;
        }
    }

    // Open the directory…
    DIR *directory = opendir(dir.c_str());
    if (directory) {
        struct dirent *entry;

        // Loop over directory contents
        while ((entry = readdir(directory)) != nullptr) {
            std::string curObject(entry->d_name);
            if(curObject == "." || curObject == "..") continue;

            std::string curObjPathWithoutPrefix(dir);
            curObjPathWithoutPrefix += "/";
            curObjPathWithoutPrefix += curObject;
            std::string curObjPathWithPrefix(dirPrefix);
            curObjPathWithPrefix += curObjPathWithoutPrefix;

            // If path is too long AND we should follow symlinks
            // -> maybe-print error message and directory path, then continue?
            // (To match GNU find behavior)
            if(args.is_used("-follow") &&
               dirPrefix.ends_with("/") &&
               (dirPrefix.length() + dir.length() + curObject.length()) > 4096 &&
               entry->d_type == DT_DIR) {
                if(isGerman)
                    std::cerr << callPath << ": ‘" << dirPrefix << dir << "/" << curObject << "’: Der Dateiname ist zu lang" << std::endl;
                else
                    std::cerr << callPath << ": '" << dirPrefix << dir << "/" << curObject << "': File name too long" << std::endl;
                printFinding(entry, dir, dirPrefix);
                continue;
            } else if(args.is_used("-follow") &&
                      !dirPrefix.ends_with("/") &&
                      (dirPrefix.length() + dir.length() + curObject.length() + 1) > 4096 &&
                      entry->d_type == DT_DIR) {
                if(isGerman)
                    std::cerr << callPath << ": ‘" << dirPrefix << "/" << dir << "/" << curObject << "’: Der Dateiname ist zu lang" << std::endl;
                else
                    std::cerr << callPath << ": '" << dirPrefix << "/" << dir << "/" << curObject << "': File name too long" << std::endl;
                printFinding(entry, dir, dirPrefix);
                continue;
            }

            // Maybe-print the item if it's not a symlink that we may or may not follow.
            if(entry->d_type != DT_LNK || !args.is_used("-follow"))
                printFinding(entry, dir, dirPrefix);

            if(entry->d_type == DT_DIR) {
                // chdir (to circumvent path length limit)...
                char * oldPath = getcwd(nullptr, 0);
                chdir(dir.c_str());

                // and recurse into subdirectory
                if(dirPrefix.length() > 0) { //prepend dirPrefix if there is one
                    walkDirRec(curObject, dirPrefix + dir + "/");
                } else {
                    walkDirRec(curObject, dir + "/");
                }

                chdir(oldPath);
            } else if(entry->d_type == DT_LNK && args.is_used("-follow")) {
                // Follow symlink if the target exists, otherwise just maybe-print it.
                char *resolved_name = nullptr;
                char * c_target = realpath(curObjPathWithoutPrefix.c_str(), resolved_name);
                if (access(c_target, F_OK) != 0) {
                    // Print broken links
                    printFinding(entry, dir, dirPrefix);
                    continue;
                }
                std::string target(c_target);

                // Check for filesystem loops.
                std::set<std::string> visitedPaths;
                if(isSymlinkInLoop(curObjPathWithoutPrefix, visitedPaths)) {

                    if(isGerman) {
                        std::cerr << callPath << ": Dateisystemschleife erkannt; ‘";
                        if(rootCwd == nullptr) { // directory-to-search was absolute path, so print absolute paths
                            std::cerr << realpathOfSymlink(curObjPathWithPrefix);
                            std::cerr << "’ ist ein Teil der gleichen Schleife wie ‘";
                            std::cerr << target << "’." << std::endl;
                        } else { // directory-to-search was relative, so print relative paths.
                            std::cerr << dirPrefix << dir << "/" << curObject;
                            std::cerr << "’ ist ein Teil der gleichen Schleife wie ‘";
                            std::cerr << calculateRelativePath(target, rootCwd) << "’." << std::endl;
                        }
                    } else {
                        std::cerr << callPath << ": File system loop detected; '";
                        if(rootCwd == nullptr) { // directory-to-search was absolute path, so print absolute paths
                            std::cerr << realpathOfSymlink(curObjPathWithPrefix);
                            std::cerr << "' is part of the same file system loop as '";
                            std::cerr << target << "'." << std::endl;
                        } else { // directory-to-search was relative, so print relative paths.
                            std::cerr << dirPrefix << dir << "/" << curObject;
                            std::cerr << "' is part of the same file system loop as '";
                            std::cerr << calculateRelativePath(target, rootCwd) << "'." << std::endl;
                        }
                    }
                    continue;
                }

                struct stat statOut{};
                int statRV = stat(curObjPathWithoutPrefix.c_str(), &statOut);
                if(statRV == 0 && S_ISDIR(statOut.st_mode)) {
                    // If the symlink ends in a directory, print it and then handle it like a normal directory
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
                } else {
                    // If not, just print it.
                    printFinding(entry, dir, dirPrefix);
                }
            }
        }
        closedir(directory);
    } else {
        if(isGerman)
            std::cerr << "find: ‘" << dirPrefix << dir << "’: Keine Berechtigung" << std::endl;
        else
            std::cerr << "find: '" << dirPrefix << dir << "': Permission denied" << std::endl;
    }
}