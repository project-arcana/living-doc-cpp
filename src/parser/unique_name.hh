#pragma once

#include <clean-core/string.hh>

namespace cppast
{
class cpp_entity;
class cpp_type;
class cpp_expression;
class cpp_entity_index;
}

namespace ld
{
cc::string unique_name_of(cppast::cpp_entity const& e, cppast::cpp_entity_index const& idx);
cc::string unique_name_of(cppast::cpp_expression const& e, cppast::cpp_entity_index const& idx);
cc::string unique_name_of(cppast::cpp_type const& t, cppast::cpp_entity_index const& idx);
}
