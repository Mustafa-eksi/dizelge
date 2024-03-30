#include <exception>
#include <ios>
#include <optional>
#include <stdexcept>
#include <string>
#include <fstream>
#include <stdio.h>
#include <unordered_map>
#include <vector>

namespace deskentry {

// map of groups and their key-values.
typedef std::unordered_map<std::string, std::unordered_map<std::string, std::string>> UnparsedEntry;

enum ValueType {
        String,
        LocaleString,
        IconString,
        Boolean,
        Numeric
};

struct ParsedValue {
        ValueType type;
        std::string strval, locale;
        bool isArray, isLocal;
};

typedef std::unordered_map<std::string, ParsedValue> ParsedGroup;
typedef std::unordered_map<std::string, ParsedGroup> ParsedEntry;

typedef struct DesktopEntry {
        ParsedEntry pe;
        bool onMenu, onDesktop, onTaskbar;
} DesktopEntry;

void print_unde(UnparsedEntry unde) {
        for (auto const& [group, keyval] : unde)
        {
                printf("Group: %s\n", group.c_str());
                for (auto const& [key, value] : keyval) {
                        printf("\t%s = %s\n", key.c_str(), value.c_str());
                }
        }
}

std::string type_to_str(ValueType t) {
        switch(t) {
                case IconString: return "IconString";
                case LocaleString: return "LocaleString";
                case String: return "String";
                case Boolean: return "Boolean";
                case Numeric: return "Numeric";
        }
}

void print_pe(ParsedEntry pe) {
        for (auto const& [group, pg] : pe)
        {
                printf("Group: %s\n", group.c_str());
                for (auto const& [pkey, pval] : pg) {
                        printf("\t'%s' = '%s' %s =\"%s\"\n", pkey.c_str(), type_to_str(pval.type).c_str(), pval.isArray ? "(array)" : "",
                                pval.type == ValueType::String || pval.type == ValueType::LocaleString ? pval.strval.c_str() : "");
                }
        }
}

std::optional<ParsedEntry> parse_entry(UnparsedEntry unde) {
        if (!unde.contains("Desktop Entry"))
                return {};
        ParsedEntry pe;

        for (auto const& [group, keyval] : unde) {
                pe[group] = {};
                for (auto const& [key, val] : keyval) {
                        if (key.ends_with("]")) {
                                pe[group][key].isLocal = true;
                                pe[group][key].locale = key.substr(key.find("["));
                                pe[group][key].type = ValueType::LocaleString;
                        }
                        pe[group][key].strval = val;

                        if (val == "true" || val == "false") {
                                pe[group][key].type = ValueType::Boolean;
                                pe[group][key].strval = val == "true";
                                continue;
                        }
                        if(key != "Version") {
                                try {
                                        float f = std::stof(val);
                                        pe[group][key].type = ValueType::Numeric;
                                        continue;
                                } catch (std::exception e) {}
                        }

                        if (val.find(";") != std::string::npos) {
                                pe[group][key].isArray = true;
                        }
                        if (pe[group][key].type != ValueType::LocaleString) {
                                // IconString is only for Icon
                                pe[group][key].type = ValueType::String;
                                if (key == "Icon")
                                        pe[group][key].type = ValueType::IconString;
                        }
                }
        }
        return pe;
}

std::optional<UnparsedEntry> read_file(std::string filepath) {
        std::string buffer;
        UnparsedEntry unde;

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
                }
skip:
                if (end_of_line < buffer.size())
                        i = end_of_line;
        }
        //print_unde(unde);

        return unde;
}

std::optional<DesktopEntry> parse_file(std::string filepath) {
        if (auto unde = read_file(filepath)){
                auto pe = parse_entry(unde.value());
                if(!pe.has_value())
                        return {};
                return (DesktopEntry) {
                        .pe = pe.value(),
                };
        } else {
                return {};
        }
}

}