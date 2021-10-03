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

    /// generates the actual documentation files to generate with hugo
    /// - template_dir contains assets and snippets for the HTML docs (e.g. a /assets dir)
    /// - target_path is a directory that will contain the config.toml of the final hugo project
    void generate_hugo(cc::string const& template_dir, cc::string const& target_path);

    // private data structures
private:
    struct lib_version
    {
        cc::string version;
        lib_config cfg;

        cc::vector<file_repo> files;

        // e.g. /some/path/to/cc/array.hh -> clean-core/array.hh
        // e.g. /some/path/to/cc/subdir/bla.hh -> clean-core/subdir/bla.hh
        cc::string get_include_name_for(cc::string filename) const;
        // e.g. /some/path/to/cc/array.hh -> array.hh
        // e.g. /some/path/to/cc/subdir/bla.hh -> subdir/bla.hh
        cc::string get_relative_name_for(cc::string filename) const;

        // e.g. /path/to/clean-core/detail/stuff.hh
        bool is_ignored_include(cc::string_view filename) const;
        // e.g. /usr/include/c++/8/vector
        bool is_system_include(cc::string_view filename) const;
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

    // generator impl
private:
    struct hugo_gen;
};
}
