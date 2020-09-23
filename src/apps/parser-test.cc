#include <nexus/app.hh>

#include <cppast/libclang_parser.hpp>

APP("parser test")
{
    cppast::libclang_compile_config cfg;
}
