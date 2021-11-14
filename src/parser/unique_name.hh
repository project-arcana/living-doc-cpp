#pragma once

#include <clean-core/string.hh>

namespace cppast
{
class cpp_entity;
class cpp_type;
}

namespace ld
{
cc::string unique_name_of(cppast::cpp_entity const& e);
cc::string unique_name_of(cppast::cpp_type const& t);
}
