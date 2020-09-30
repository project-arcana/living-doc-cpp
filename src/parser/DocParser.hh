#pragma once

#include <core/file_repo.hh>
#include <core/lib_config.hh>

namespace ld
{
class DocParser
{
public:
    explicit DocParser(lib_config cfg);

    cc::vector<cc::string> get_all_header_files() const;
    cc::vector<cc::string> get_all_source_files() const;

    file_repo parse_file(cc::string_view filename) const;

private:
    lib_config _cfg;
};
}
