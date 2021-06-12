#include <sstream>
#include <iterator>
#include <cstddef>
#include <vector>
#include <cstring>
#include <string>
#include "linenoise.h"
#include <iostream>

std::vector<std::string> split(std::string &str, const std::string &delim = " ") {
    std::vector<std::string> words{};

    size_t pos = 0;
    while ((pos = str.find(delim)) != std::string::npos) {
        words.push_back(str.substr(0, pos));
        str.erase(0, pos + delim.length());
    }
    if (words.size() == 0) {
        words.push_back(str);
    }
    return words;
}

void handle_line_parts(const std::vector<std::string> &parts) {
    std::cout << parts.size() << "\n";
    std::string cmd = parts[0];
    if (cmd == "f") {

    } else {
        std::cout << "Invalid cmd\n";
    }    

}

int main(int argc, char* argv[]) {
    // get the file from argv
    if (argc != 2) {
        std::cerr << "Bad args\n";
        return 1;
    }
    std::string path = argv[1];
    std::cout << "Path to file is " << path <<"\n";
    char *line;
    while((line = linenoise("pbf-inspector> ")) != nullptr) {
        // Split string into parts
        std::string line_str = line;
        auto line_parts = split(line_str);
        handle_line_parts(line_parts);

        // if (line[0] != '\0' && line[0] != '/') {
        //     printf("echo: '%s'\n", line);
        //     linenoiseHistoryAdd(line); /* Add to the history. */
        //     linenoiseHistorySave("history.txt"); /* Save the history on disk. */
        // } else if (!strncmp(line,"/historylen",11)) {
        //     /* The "/historylen" command will change the history len. */
        //     int len = atoi(line+11);
        //     linenoiseHistorySetMaxLen(len);
        // } else if (!strncmp(line, "/mask", 5)) {
        //     linenoiseMaskModeEnable();
        // } else if (!strncmp(line, "/unmask", 7)) {
        //     linenoiseMaskModeDisable();
        // } else if (line[0] == '/') {
        //     printf("Unreconized command: %s\n", line);
        // }
        free(line);
    }
    return 0;
}