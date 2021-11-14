#include "unique_name.hh"

#include <rich-log/log.hh>

#include <cppast/cpp_class.hpp>
#include <cppast/cpp_entity_kind.hpp>
#include <cppast/cpp_function.hpp>
#include <cppast/cpp_member_function.hpp>
#include <cppast/cpp_member_variable.hpp>
#include <cppast/cpp_namespace.hpp>
#include <cppast/cpp_type_alias.hpp>


cc::string ld::unique_name_of(cppast::cpp_entity const& ee)
{
    cc::string name;

    switch (ee.kind())
    {
    case cppast::cpp_entity_kind::namespace_t:
    {
        auto const& e = static_cast<cppast::cpp_namespace const&>(ee);

        if (e.parent().has_value() && e.parent().value().kind() == cppast::cpp_entity_kind::namespace_t)
        {
            name = unique_name_of(e.parent().value());
            name += "::";
        }

        if (e.is_anonymous())
            return name + "<anon>";

        return name + e.name().c_str();
    }

    case cppast::cpp_entity_kind::class_t:
    {
        auto const& e = static_cast<cppast::cpp_class const&>(ee);

        auto p = e.parent();
        if (p.has_value() && p.value().kind() == cppast::cpp_entity_kind::class_template_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }
        if (p.has_value() && p.value().kind() == cppast::cpp_entity_kind::class_template_specialization_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }

        if (p.has_value())
        {
            name += unique_name_of(p.value());
            name += "::";
        }

        return name + e.name().c_str();
    }

    case cppast::cpp_entity_kind::function_t:
    case cppast::cpp_entity_kind::constructor_t:
    case cppast::cpp_entity_kind::conversion_op_t:
    case cppast::cpp_entity_kind::member_function_t:
    {
        auto const& e = static_cast<cppast::cpp_function_base const&>(ee);

        auto p = e.parent();
        if (p.has_value() && p.value().kind() == cppast::cpp_entity_kind::function_template_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }
        if (p.has_value() && p.value().kind() == cppast::cpp_entity_kind::function_template_specialization_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }

        if (p.has_value())
        {
            name += unique_name_of(p.value());
            name += "::";
        }

        name += e.name().c_str();

        // TODO: arg signature
        name += '(';
        auto first = true;
        for (auto const& arg : e.parameters())
        {
            if (first)
                first = false;
            else
                name += ", ";

            name += arg.name();
        }
        name += ')';

        return name;
    }

    case cppast::cpp_entity_kind::member_variable_t:
        return ee.name().c_str();

    case cppast::cpp_entity_kind::type_alias_t:
        return ee.name().c_str();

    default:
        LOG_WARN("unsupported unique_name_of kind: {}", ee.kind());
        return to_string(ee.kind());
    }
}

cc::string ld::unique_name_of(cppast::cpp_type const& t)
{
    //
    return "TODO";
}
