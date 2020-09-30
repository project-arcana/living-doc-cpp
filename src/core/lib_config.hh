#pragma once

#include <clean-core/string.hh>
#include <clean-core/string_view.hh>
#include <clean-core/vector.hh>

namespace ld
{
struct compile_config
{
    cc::vector<cc::string> include_dirs;
    cc::vector<cc::string> defines;
};

template <class In>
constexpr void introspect(In&& inspect, compile_config& v)
{
    inspect(v.include_dirs, "include_dirs");
    inspect(v.defines, "defines");
}

struct lib_config
{
    cc::string name;
    cc::string version;
    cc::vector<cc::string> dirs;
    cc::vector<cc::string> ignored_namespaces;
    ld::compile_config compile_config;

    static lib_config from_file(cc::string_view filename);
};

template <class In>
constexpr void introspect(In&& inspect, lib_config& v)
{
    inspect(v.name, "name");
    inspect(v.version, "version");
    inspect(v.dirs, "dirs");
    inspect(v.ignored_namespaces, "ignored_namespaces");
    inspect(v.compile_config, "compile_config");
}
}
