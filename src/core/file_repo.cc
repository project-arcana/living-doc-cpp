#include "file_repo.hh"

#include <filesystem>

#include <babel-serializer/data/json.hh>
#include <babel-serializer/file.hh>

cc::string ld::file_repo::filename_without_path() const { return std::filesystem::path(filename.c_str()).filename().c_str(); }

cc::string ld::file_repo::filename_without_path_and_ext() const { return std::filesystem::path(filename.c_str()).stem().c_str(); }

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
