#pragma once

#include <clean-core/map.hh>
#include <clean-core/string.hh>

#include <core/file_repo.hh>
#include <core/lib_config.hh>

namespace ld
{
class Generator;

class DocParser
{
public:
    // cc::vector<cc::string> get_all_header_files() const;
    // cc::vector<cc::string> get_all_source_files() const;

    // NOTE: removed because cppast index needs to be populated
    // file_repo parse_file(cc::string_view filename) const;
    void add_lib(lib_config cfg);
    void add_file(cc::string_view lib, cc::string filename);

    void parse_all(Generator& gen);

private:
    struct lib
    {
        lib_config cfg;
        cc::vector<cc::string> files_to_parse;
    };
    cc::map<cc::string, lib> _libs;
};
}
