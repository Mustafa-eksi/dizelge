#include <ios>
#include <optional>
#include <string>
#include <fstream>
#include <stdio.h>
#include <unordered_map>

namespace deskentry {

typedef struct DesktopEntry {
        std::string filename, app_name, exec_command;
        bool onMenu, onDesktop, onTaskbar;
} DesktopEntry;

// map of groups and their key-values.
typedef std::unordered_map<std::string, std::unordered_map<std::string, std::string>> UnparsedDe;

void print_unde(UnparsedDe unde) {
        for (auto const& [group, keyval] : unde)
        {
                printf("Group: %s\n", group.c_str());
                for (auto const& [key, value] : keyval) {
                        printf("\t%s = %s\n", key.c_str(), value.c_str());
                }
        }
}

std::optional<DesktopEntry> parse_file(std::string filepath) {
        std::string buffer;
        UnparsedDe unde;

        std::ifstream de_file(filepath);
        de_file.seekg(0, std::ios::end);
        buffer.resize(de_file.tellg());
        de_file.seekg(0);
        de_file.read(buffer.data(), buffer.size());

        std::string current_group;
        for (int i = 0; i < buffer.size(); i++) {
                int eq_pos;
                int end_of_line = buffer.find("\n", i);

                std::string current_line = buffer.substr(i, end_of_line-i);
                //printf("current line: '%s'\n", current_line.c_str());
                // Shebang or comment
                if (current_line.starts_with("#"))
                        goto skip;
                // Expected group header
                if (current_line.starts_with("[")) {
                        // Syntax check
                        int end_pos = current_line.find("]");
                        std::string grp = current_line.substr(1, end_pos-1);
                        if (grp.find("[") != std::string::npos)
                                return {};
                        current_group = grp;
                        unde[current_group] = {}; // Could remove
                }
                // Expected key-value
                eq_pos = current_line.find("=");
                if (eq_pos != std::string::npos) {
                        if (current_group.empty())
                                goto skip;
                        std::string key = current_line.substr(0, eq_pos);
                        while (key.ends_with(" ")) {
                                key.pop_back();
                        }
                        std::string value = current_line.substr(eq_pos+1);
                        while (value.starts_with(" ")) {
                                value = value.substr(1);
                        }
                        unde[current_group][key] = value;
                        //printf("Key: '%s' = Value: '%s'\n", key.c_str(), value.c_str());
                }
skip:
                if (end_of_line < buffer.size())
                        i = end_of_line;
        }
        print_unde(unde);
        printf("Ended\n");

        return {};
}

}