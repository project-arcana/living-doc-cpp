#pragma once

#include <clean-core/map.hh>
#include <clean-core/string.hh>
#include <clean-core/string_view.hh>
#include <clean-core/vector.hh>

#include <core/file_repo.hh>
#include <core/lib_config.hh>

namespace ld
{
/**
 * Generates the documentation for a whole set of libraries
 *
 * Usage:
 *   - call add_file for all files that should get included in the documentation
 */
class Generator
{
public:
    void set_current_version(cc::string_view lib_name, cc::string version);

    void add_file(cc::string_view lib_name, cc::string_view version, file_repo file);
    void set_lib_config(cc::string_view lib_name, cc::string_view version, lib_config cfg);

    /// generates the actual documentation files
    /// - template_dir contains assets and snippets for the HTML docs (e.g. a /assets dir)
    /// - target_path is a directory that will contain the index.html of the final documentation
    void generate_html(cc::string const& template_dir, cc::string const& target_path);

    // private data structures
private:
    struct lib_version
    {
        cc::string version;
        lib_config cfg;

        cc::vector<file_repo> files;
    };

    struct library
    {
        cc::string name;
        cc::string current_version;

        cc::map<cc::string, lib_version> versions;
    };

    // members
private:
    /// set of all libs
    cc::map<cc::string, library> _libs;
    /// ordered list of libs
    cc::vector<cc::string> _lib_names;

    // helper
private:
    library& get_lib(cc::string_view name);
    lib_version& get_lib_version(cc::string_view name, cc::string_view version);
};
}
