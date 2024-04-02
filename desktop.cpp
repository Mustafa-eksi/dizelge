#include "glibmm/ustring.h"
#include <exception>
#include <ios>
#include <optional>
#include <stdexcept>
#include <string>
#include <fstream>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <unistd.h>

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

typedef struct DesktopEntry {
        ParsedEntry pe;
        enum EntryType type;
        bool isGicon, onMenu, onDesktop, onTaskbar, HiddenFilter, NoDisplayFilter,
             OnlyShowInFilter, TryExecFilter;
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
}

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

enum EntryType parse_type(std::string typestr) {
        if (typestr == "Application") return EntryType::Application;
        if (typestr == "Link") return EntryType::Link;
        if (typestr == "Directory") return EntryType::Directory;
        return EntryType::UndefinedType;
}

bool parse_desktop_filter(ParsedEntry pe, std::string xdg_env) {
        bool res = false;
        if (pe["Desktop Entry"]["OnlyShowIn"].strval != "")
                res = pe["Desktop Entry"]["OnlyShowIn"].strval.find(xdg_env) != Glib::ustring::npos;
        if (pe["Desktop Entry"]["NotShowIn"].strval != "")
                res = pe["Desktop Entry"]["NotShowIn"].strval.find(xdg_env) == Glib::ustring::npos;
        return res;
}

// FIXME: make this compatible with spec.
bool try_exec(ParsedEntry pe) {
        if (pe["Desktop Entry"]["TryExec"].strval.empty())
                return true;
        return access(pe["Desktop Entry"]["TryExec"].strval.c_str(), X_OK);
}

std::optional<DesktopEntry> parse_file(std::string filepath, std::string xdg_env) {
        if (auto unde = read_file(filepath)){
                auto oppe = parse_entry(unde.value());
                if(!oppe.has_value())
                        return {};
                ParsedEntry pe = oppe.value();
                EntryType et = parse_type(pe["Desktop Entry"]["Type"].strval);
                // See specification: table 2. Standard Keys
                if (et == EntryType::UndefinedType)
                        return {};
                DesktopEntry de = (DesktopEntry) {
                        .pe = pe,
                        .type = et,
                        .isGicon = pe["Desktop Entry"]["Icon"].strval.find("/") == Glib::ustring::npos,
                        .HiddenFilter = pe["Desktop Entry"]["Hidden"].strval == "true",
                        .NoDisplayFilter = pe["Desktop Entry"]["NoDisplay"].strval == "true",
                        .OnlyShowInFilter = parse_desktop_filter(pe, xdg_env),
                        .TryExecFilter = try_exec(pe)
                };
                
                return de;
        } else {
                return {};
        }
}

}