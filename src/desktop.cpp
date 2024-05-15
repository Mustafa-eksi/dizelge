#pragma once
#include <cstdlib>
#include <ios>
#include <optional>
#include <string>
#include <fstream>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <unistd.h>

/*
 * This is the library I use to parse and use desktop entry files.
 * This file doesn't have any ui stuff and can be used seperately.
 */

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

typedef enum EntryType {
        Application = 0,
        Link = 1,
        Directory = 2,
        UndefinedType
} EntryType;

typedef std::vector<std::string> CatList;

typedef struct DesktopEntry {
        UnparsedEntry pe;
        enum EntryType type;
        bool isGicon, onMenu, onDesktop, onTaskbar, HiddenFilter, NoDisplayFilter,
             OnlyShowInFilter, TryExecFilter;
        CatList Categories;
        std::string path;
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
        return "UndefinedType";
}

void print_pe(ParsedEntry pe) {
        for (auto const& [group, pg] : pe)
        {
                printf("Group: %s\n", group.c_str());
                for (auto const& [pkey, pval] : pg) {
                        printf("\t'%s' = '%s' %s =\"%s\"\n", pkey.c_str(), type_to_str(pval.type).c_str(), pval.isArray ? "(array)" : "", pval.strval.c_str());
                }
        }
}

// Disabled due to performance reasons.
/*std::optional<ParsedEntry> parse_entry(UnparsedEntry unde) {
        if (!unde.contains("Desktop Entry"))
                return {};
        ParsedEntry pe;

        for (auto const& [group, keyval] : unde) {
                pe[group] = {};
                for (auto const& [key, val] : keyval) {
                        pe[group][key].strval = val;

                        // keys like Comment[tr]
                        if (key.ends_with("]")) {
                                pe[group][key].isLocal = true;
                                pe[group][key].locale = key.substr(key.find("["));
                                pe[group][key].type = ValueType::LocaleString;
                        }
                        if (val == "true" || val == "false") {
                                pe[group][key].type = ValueType::Boolean;
                                continue;
                        }
                        // Version key falls into float category in most applications
                        // but it is defined as string in the spec.
                        if(key != "Version") {
                                try {
                                        // This line will throw an exception if val isn't a float.
                                        std::stof(val); // Can create problems with higher rates of optimisation.
                                        pe[group][key].type = ValueType::Numeric;
                                        continue;
                                } catch (std::exception& _) {}
                        }
                        // Specification doesn't specify what is an array well, so this can sometimes fail.
                        if (val.find(";") != std::string::npos) {
                                pe[group][key].isArray = true;
                        }
                        if (pe[group][key].type != ValueType::LocaleString) {
                                pe[group][key].type = ValueType::String;
                                // Icon's value is a string but it is iconstring (idk why but can be related to paths)
                                if (key == "Icon")
                                        pe[group][key].type = ValueType::IconString;
                        }
                }
        }
        return pe;
}*/

std::optional<UnparsedEntry> read_file(std::string filepath) {
        std::string buffer;
        UnparsedEntry unde;

        // Read whole file into buffer.
        std::ifstream de_file(filepath);
        de_file.seekg(0, std::ios::end);
        buffer.resize(de_file.tellg());
        de_file.seekg(0);
        de_file.read(buffer.data(), buffer.size());

        std::string current_group;
        for (size_t i = 0; i < buffer.size(); i++) {
                size_t eq_pos;
                size_t end_of_line = buffer.find("\n", i);

                std::string current_line = buffer.substr(i, end_of_line-i);
                if (current_line.empty())
                        continue;
                // Shebang or comment
                if (current_line.at(0) == '#')
                        goto skip;
                //printf("%s: '%s'\n", filepath.c_str(), current_line.c_str());
                // Expected group header
                if (current_line.at(0) == '[') {
                        // Syntax check
                        size_t end_pos = current_line.find("]");
                        if (end_pos == std::string::npos)
                                return {};
                        if (end_pos-1 > 0)
                                current_group = current_line.substr(1, end_pos-1);
                        //unde[current_group] = {}; // Could remove
                }
                // Expected key-value
                eq_pos = current_line.find("=");
                if (eq_pos != std::string::npos && eq_pos != current_line.length()-1) {
                        if (current_group.empty())
                                goto skip;
                        std::string key = current_line.substr(0, eq_pos);
                        if (key.empty())
                                continue;
                        while (key.length() > 1 && key.at(key.length()-1) == ' ') {
                                key.pop_back();
                        }
                        std::string value = current_line.substr(eq_pos+1);
                        if (value.empty())
                                continue;
                        while (value.length() > 1 && value.at(0) == ' ') {
                                value = value.substr(1);
                        }
                        unde[current_group][key] = value;
                }
skip:
                if (end_of_line < buffer.size())
                        i = end_of_line;
        }
        return unde;
}

enum EntryType parse_type(std::string typestr) {
        if (typestr == "Application") return EntryType::Application;
        if (typestr == "Link") return EntryType::Link;
        if (typestr == "Directory") return EntryType::Directory;
        return EntryType::UndefinedType;
}

bool parse_desktop_filter(UnparsedEntry pe, std::string xdg_env) {
        bool res = false;
        if (pe["Desktop Entry"]["OnlyShowIn"] != "")
                res = pe["Desktop Entry"]["OnlyShowIn"].find(xdg_env) != std::string::npos;
        if (pe["Desktop Entry"]["NotShowIn"] != "")
                res = pe["Desktop Entry"]["NotShowIn"].find(xdg_env) == std::string::npos;
        return res;
}

// FIXME: make this compatible with spec.
bool try_exec(UnparsedEntry pe) {
        if (pe["Desktop Entry"]["TryExec"].empty())
                return true;
        return access(pe["Desktop Entry"]["TryExec"].c_str(), X_OK);
}

CatList parse_categories(UnparsedEntry pe) {
        CatList res;
        if (pe["Desktop Entry"]["Categories"].find(";") != std::string::npos) {
                std::string it = pe["Desktop Entry"]["Categories"];
                size_t pos = it.find(";");
                while (pos != std::string::npos) {
                        res.push_back(it.substr(0, pos));
                        it = it.substr(pos+1);
                        pos = it.find(";");
                }
                // Add last element if array doesn't end with a semicolon
                if (!it.empty())
                        res.push_back(it);
        } else if (!pe["Desktop Entry"]["Categories"].empty()){
                res.push_back(pe["Desktop Entry"]["Categories"]);
        }
        return res;
}

std::optional<DesktopEntry> parse_file(std::string filepath, std::string xdg_env) {
        if (auto unde = read_file(filepath)){
                if(!unde.has_value())
                        return {};
                UnparsedEntry upe = unde.value();
                EntryType et = parse_type(upe["Desktop Entry"]["Type"]);
                // See specification: table 2. Standard Keys
                if (et == EntryType::UndefinedType)
                        return {};
                DesktopEntry de = (DesktopEntry) {
                        .pe = upe,
                        .type = et,
                        .HiddenFilter = upe["Desktop Entry"]["Hidden"] == "true",
                        .NoDisplayFilter = upe["Desktop Entry"]["NoDisplay"] == "true",
                        .OnlyShowInFilter = parse_desktop_filter(upe, xdg_env),
                        .TryExecFilter = try_exec(upe),
                        .Categories = parse_categories(upe),
                        .path = filepath,
                };
                
                return de;
        } else {
                return {};
        }
}

void write_to_file(UnparsedEntry pe, std::string out_path) {
        std::string output_file = "#!/usr/bin/env xdg-open\n";
        for (auto const& [group, pg] : pe)
        {
                output_file += "["+group+"]\n";
                for (auto const& [pkey, pval] : pg) {
                        if (pval.empty())
                                continue;
                        output_file += pkey+"="+pval+"\n";
                }
                output_file += "\n";
        }
        if (access(out_path.c_str(), W_OK) != 0) {
                system(("pkexec bash -c \"echo \\\""+output_file+"\\\" > "+out_path+"\"").c_str());
                return;
        }
        std::ofstream OutStream(out_path);
        OutStream << output_file;
        OutStream.close();
}


}
