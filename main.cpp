#include <iostream>
#include <dirent.h>
#include<vector>
#include <set>

int main() {


    std::cout << "Hello, World!" << std::endl;
    return 0;
}

std::set<std::string> seenPaths;

void walkDir(const std::string& dir) {
    if(seenPaths.find(dir) != seenPaths.end()) return; //dir was already walked
    else seenPaths.insert(dir);

    DIR *directory = opendir("/");

    if (directory) {
        struct dirent *entry;

        // Durchlaufe das Verzeichnis und liste die Dateien auf
        while ((entry = readdir(directory)) != nullptr) {
            if(entry->d_type == DT_DIR) {
                walkDir(dir + "/" + entry->d_name);
            } else if(entry->d_type == DT_LNK) {
                //TODO: Handle symlinks
            }

            printf("%s -> %c\n", entry->d_name, entry->d_type);
        }

        // Schließe das Verzeichnis
        closedir(directory);
    } else {
        perror("Fehler beim Öffnen des Verzeichnisses");
    }
}

const char * getNodeType(unsigned char d_type) {
    switch (d_type) {
        case DT_BLK:
            return "BLOCK";
        case DT_CHR:
            return "CHAR";
        case DT_DIR:
            return "DIR";
        case DT_FIFO:
            return "FIFO";
        case DT_LNK:
            return "SYMLINK";
        case DT_REG:
            return "FILE";
        case DT_SOCK:
            return "SOCKET";
        default:
            return "UNKNOWN";
    }
}
