#include "unique_name.hh"

#include <rich-log/log.hh>

#include <clean-core/to_string.hh>

#include <cppast/cpp_array_type.hpp>
#include <cppast/cpp_class.hpp>
#include <cppast/cpp_class_template.hpp>
#include <cppast/cpp_decltype_type.hpp>
#include <cppast/cpp_entity_kind.hpp>
#include <cppast/cpp_function.hpp>
#include <cppast/cpp_member_function.hpp>
#include <cppast/cpp_member_variable.hpp>
#include <cppast/cpp_namespace.hpp>
#include <cppast/cpp_template.hpp>
#include <cppast/cpp_template_parameter.hpp>
#include <cppast/cpp_type_alias.hpp>

namespace ca = cppast;

cc::string ld::unique_name_of(ca::cpp_entity const& ee, cppast::cpp_entity_index const& idx)
{
    cc::string name;

    switch (ee.kind())
    {
    case ca::cpp_entity_kind::namespace_t:
    {
        auto const& e = static_cast<ca::cpp_namespace const&>(ee);

        if (e.parent().has_value() && e.parent().value().kind() == ca::cpp_entity_kind::namespace_t)
        {
            name = unique_name_of(e.parent().value(), idx);
            name += "::";
        }

        if (e.is_anonymous())
            return name + "<anon>";

        return name + e.name().c_str();
    }

    case ca::cpp_entity_kind::class_t:
    {
        auto const& e = static_cast<ca::cpp_class const&>(ee);

        auto p = e.parent();
        if (p.has_value() && p.value().kind() == ca::cpp_entity_kind::class_template_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }
        if (p.has_value() && p.value().kind() == ca::cpp_entity_kind::class_template_specialization_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }

        if (p.has_value())
        {
            name += unique_name_of(p.value(), idx);
            name += "::";
        }

        return name + e.name().c_str();
    }

    case ca::cpp_entity_kind::class_template_t:
    {
        auto const& e = static_cast<ca::cpp_class_template const&>(ee);

        if (auto p = e.parent())
        {
            name += unique_name_of(p.value(), idx);
            name += "::";
        }

        return name + e.name().c_str();
    }

    case ca::cpp_entity_kind::function_t:
    case ca::cpp_entity_kind::constructor_t:
    case ca::cpp_entity_kind::conversion_op_t:
    case ca::cpp_entity_kind::member_function_t:
    {
        auto const& e = static_cast<ca::cpp_function_base const&>(ee);

        auto p = e.parent();
        if (p.has_value() && p.value().kind() == ca::cpp_entity_kind::function_template_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }
        if (p.has_value() && p.value().kind() == ca::cpp_entity_kind::function_template_specialization_t)
        {
            // TODO: handle me
            p = p.value().parent();
        }

        if (p.has_value())
        {
            name += unique_name_of(p.value(), idx);
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

            name += unique_name_of(arg.type(), idx);
        }
        name += ')';

        return name;
    }

    case ca::cpp_entity_kind::member_variable_t:
        return ee.name().c_str();

    case ca::cpp_entity_kind::type_alias_t:
        return ee.name().c_str();

    default:
        LOG_WARN("unsupported unique_name_of kind: {}", ee.kind());
        return to_string(ee.kind());
    }
}

// TODO: move me in header if required
cc::string ld::unique_name_of(ca::cpp_expression const& e, cppast::cpp_entity_index const&)
{
    switch (e.kind())
    {
    case ca::cpp_expression_kind::literal_t:
        return static_cast<ca::cpp_literal_expression const&>(e).value().c_str();

    case ca::cpp_expression_kind::unexposed_t:
        return static_cast<ca::cpp_unexposed_expression const&>(e).expression().as_string().c_str();

    default:
        LOG_WARN("unknown expression kind: {}", int(e.kind()));
        return "FIXME";
    }
}

cc::string ld::unique_name_of(ca::cpp_type const& t, cppast::cpp_entity_index const& idx)
{
    switch (t.kind())
    {
    case ca::cpp_type_kind::builtin_t:
        return to_string(static_cast<ca::cpp_builtin_type const&>(t).builtin_type_kind());

    case ca::cpp_type_kind::user_defined_t:
        // FIXME
        for (auto const& e : static_cast<ca::cpp_user_defined_type const&>(t).entity().get(idx))
            return unique_name_of(*e, idx);
        return "__unknown_user_defined_type";

    case ca::cpp_type_kind::auto_t:
        return "auto";
    case ca::cpp_type_kind::decltype_t:
        return "decltype(" + unique_name_of(static_cast<ca::cpp_decltype_type const&>(t).expression(), idx) + ")";
    case ca::cpp_type_kind::decltype_auto_t:
        return "decltype(auto)";

    case ca::cpp_type_kind::cv_qualified_t:
    {
        auto& tt = static_cast<ca::cpp_cv_qualified_type const&>(t);
        switch (tt.cv_qualifier())
        {
        case ca::cpp_cv::cpp_cv_const:
            return unique_name_of(tt.type(), idx) + " const";
        case ca::cpp_cv::cpp_cv_const_volatile:
            return "volatile " + unique_name_of(tt.type(), idx) + " const";
        case ca::cpp_cv::cpp_cv_volatile:
            return "volatile " + unique_name_of(tt.type(), idx);

        default:
        case ca::cpp_cv::cpp_cv_none:
            return unique_name_of(tt.type(), idx);
        }
    }
    case ca::cpp_type_kind::pointer_t:
        return unique_name_of(static_cast<ca::cpp_pointer_type const&>(t).pointee(), idx) + "*";
    case ca::cpp_type_kind::reference_t:
    {
        auto& tt = static_cast<ca::cpp_reference_type const&>(t);
        switch (tt.reference_kind())
        {
        case ca::cpp_reference::cpp_ref_lvalue:
            return unique_name_of(tt.referee(), idx) + "&";
        case ca::cpp_reference::cpp_ref_rvalue:
            return unique_name_of(tt.referee(), idx) + "&&";

        default:
        case ca::cpp_reference::cpp_ref_none:
            return unique_name_of(tt.referee(), idx);
        }
    }

    case ca::cpp_type_kind::array_t:
    {
        auto& tt = static_cast<ca::cpp_array_type const&>(t);
        auto name = unique_name_of(tt.value_type(), idx);
        if (tt.size().has_value())
            return name + "[" + unique_name_of(tt.size().value(), idx) + "]";
        else
            return name + "[]";
    }
    case ca::cpp_type_kind::function_t:
        LOG_WARN("TODO: implement unique_name_of function_t");
        return "FUN";
    case ca::cpp_type_kind::member_function_t:
        LOG_WARN("TODO: implement unique_name_of member_function_t");
        return "MFUN";
    case ca::cpp_type_kind::member_object_t:
        LOG_WARN("TODO: implement unique_name_of member_object_t");
        return "MOBJ";
    case ca::cpp_type_kind::template_parameter_t:
        return static_cast<ca::cpp_template_parameter_type const&>(t).entity().name().c_str();
    case ca::cpp_type_kind::template_instantiation_t:
    {
        auto const& tt = static_cast<ca::cpp_template_instantiation_type const&>(t);
        // LOG("looking up {}", tt.primary_template().name());
        // LOG("got {} ids", tt.primary_template().id().size().get());
        // for (auto id : tt.primary_template().id())
        // {
        //     auto e = idx.lookup(id);
        //     LOG("  id: {}", (uint64_t)id);
        //     LOG("    lookup: {}", e.has_value());
        // }
        // std::exit(0);
        for (auto const& e : tt.primary_template().get(idx))
        {
            auto name = unique_name_of(*e, idx);
            if (tt.arguments_exposed())
            {
                name += " __exposed_args";
            }
            else
            {
                name += "<";
                name += tt.unexposed_arguments().c_str();
                name += ">";
            }
            return name;
        }

        return "__could_not_lookup_templ_" + cc::to_string(tt.primary_template().no_overloaded().get());
    }

    case ca::cpp_type_kind::dependent_t:
        // TODO: has a dependee
        return static_cast<ca::cpp_dependent_type const&>(t).name().c_str();

    case ca::cpp_type_kind::unexposed_t:
        return static_cast<ca::cpp_unexposed_type const&>(t).name().c_str();

    default:
        LOG_WARN("unsupported unique_name_of type kind: {}", int(t.kind()));
        return "FIXME";
    }
}
