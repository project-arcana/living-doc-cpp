#include "lib_config.hh"

#include <babel-serializer/data/json.hh>
#include <babel-serializer/file.hh>

ld::lib_config ld::lib_config::from_file(cc::string_view filename)
{
    auto cfg_string = babel::file::read_all_text(filename);
    return babel::json::read<lib_config>(cfg_string);
}
