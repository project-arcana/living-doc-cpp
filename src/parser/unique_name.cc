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

template <class Params>
static cc::string unique_name_of_template_def(Params const& params, cppast::cpp_entity_index const& idx)
{
    cc::string args;
    args += "<";
    for (auto const& arg : params)
    {
        if (args.size() > 1)
            args += ", ";

        if (arg.kind() == ca::cpp_entity_kind::template_type_parameter_t)
            args += "class";
        else if (arg.kind() == ca::cpp_entity_kind::non_type_template_parameter_t)
            args += ld::unique_name_of(static_cast<ca::cpp_non_type_template_parameter const&>(arg).type(), idx);
        else if (arg.kind() == ca::cpp_entity_kind::template_template_parameter_t)
        {
            args += "template";
            args += unique_name_of_template_def(static_cast<ca::cpp_template_template_parameter const&>(arg).parameters(), idx);
            args += " class";
        }
        else
        {
            LOG_WARN("unknown template arg type: {}", arg.kind());
            args += "UNKNOWN";
        }

        if (arg.is_variadic())
            args += "...";
    }
    args += ">";
    return args;
}
static cc::string unique_name_of_template_inst(ca::cpp_template_specialization const& e, cppast::cpp_entity_index const& idx)
{
    cc::string args;
    args += "<";
    if (e.arguments_exposed())
    {
        for (auto const& arg : e.arguments())
        {
            if (args.size() > 1)
                args += ", ";

            if (auto e = arg.expression())
                args += ld::unique_name_of(e.value(), idx);
            else if (auto t = arg.type())
                args += ld::unique_name_of(t.value(), idx);
            else if (auto t = arg.template_ref())
                args += t.value().name(); // TODO: more?
            else
            {
                LOG_WARN("unknown template instantiation type");
                args += "UNKNOWN";
            }
        }
    }
    else
    {
        // TODO: can be fixed in cppast parser
        args += e.unexposed_arguments().as_string().c_str();
    }
    args += ">";
    return args;
}

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

        cc::string args;

        auto p = e.parent();
        if (p.has_value() && p.value().kind() == ca::cpp_entity_kind::class_template_t)
        {
            args = unique_name_of_template_def(static_cast<ca::cpp_class_template const&>(p.value()).parameters(), idx);
            p = p.value().parent();
        }
        if (p.has_value() && p.value().kind() == ca::cpp_entity_kind::class_template_specialization_t)
        {
            args = unique_name_of_template_inst(static_cast<ca::cpp_class_template_specialization const&>(p.value()), idx);
            p = p.value().parent();
        }

        if (p.has_value())
        {
            name += unique_name_of(p.value(), idx);
            name += "::";
        }

        name += e.name().c_str();

        name += args;

        return name;
    }

        // called for template instantiation, so templ args do NOT belong in the name
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

        // arg signature
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
        if (e.is_variadic())
        {
            if (!first)
                name += ", ";
            name += "...";
        }
        name += ')';

        // suffix
        if (e.kind() == ca::cpp_entity_kind::member_function_t || //
            e.kind() == ca::cpp_entity_kind::conversion_op_t)
        {
            cc::string suffix;
            auto const& ee = static_cast<ca::cpp_member_function_base const&>(e);

            if (is_const(ee.cv_qualifier()))
                suffix = "const";

            if (ee.ref_qualifier() == ca::cpp_reference::cpp_ref_lvalue)
                suffix += "&";
            else if (ee.ref_qualifier() == ca::cpp_reference::cpp_ref_rvalue)
                suffix += "&&";

            if (is_volatile(ee.cv_qualifier()))
            {
                if (!suffix.empty())
                    suffix += " ";
                suffix += "volatile";
            }

            if (!suffix.empty())
            {
                name += " ";
                name += suffix;
            }
        }

        if (e.noexcept_condition().has_value())
        {
            name += " ";
            auto ne_str = unique_name_of(e.noexcept_condition().value(), idx);
            if (ne_str == "true")
                name += "noexcept";
            else
            {
                name += "noexcept(";
                name += ne_str;
                name += ")";
            }
        }

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
    {
        auto const& tt = static_cast<ca::cpp_user_defined_type const&>(t);
        for (auto const& e : tt.entity().get(idx))
            return unique_name_of(*e, idx);
        // if no lookup was successful, this means the type is not on the index
        // this means that the type was not declared in any parsed file
        // this _might_ be fine for system types like size_t
        return tt.entity().name().c_str();
    }

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

        cc::string args;
        if (tt.arguments_exposed())
        {
            args += " __exposed_args";
        }
        else
        {
            args += "<";
            // TODO: look up current instantiation
            if (cc::string_view(tt.unexposed_arguments()).starts_with("type-parameter-"))
            {
                args += "__current_instantiation";
            }
            else
            {
                // TODO: this can be fixed in type_parser.cpp
                //       currently does not really expose type, only stringified type (with wrong const order)
                args += tt.unexposed_arguments().c_str();
            }

            args += ">";
        }

        for (auto const& e : tt.primary_template().get(idx))
            return unique_name_of(*e, idx) + args;

        // type not on index
        // see ca::cpp_type_kind::user_defined_t for longer explanation
        return tt.primary_template().name().c_str() + args;
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
