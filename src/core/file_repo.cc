#include "file_repo.hh"

#include <babel-serializer/data/json.hh>
#include <babel-serializer/file.hh>

ld::file_repo ld::file_repo::from_file(cc::string_view filename)
{
    auto cfg_string = babel::file::read_all_text(filename);
    return babel::json::read<file_repo>(cfg_string);
}

void ld::file_repo::write_to_file(cc::string_view filename, bool compact) const
{
    babel::json::write_config cfg;
    if (compact)
        cfg.indent = 2;
    babel::json::write(babel::file::file_output_stream(filename), *this, cfg);
}
