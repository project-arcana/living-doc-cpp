#include "DocParser.hh"

#include <filesystem>

#include <rich-log/log.hh>

#include <clean-core/format.hh>
#include <clean-core/overloaded.hh>
#include <clean-core/to_string.hh>

#include <parser/unique_name.hh>

#include <cppast/cpp_entity_kind.hpp>
#include <cppast/cpp_function.hpp>
#include <cppast/cpp_member_function.hpp>
#include <cppast/cpp_member_variable.hpp>
#include <cppast/cpp_namespace.hpp>
#include <cppast/cpp_type_alias.hpp>
#include <cppast/libclang_parser.hpp>
#include <cppast/visitor.hpp>

ld::DocParser::DocParser(ld::lib_config cfg) : _cfg(cc::move(cfg)) {}

cc::vector<cc::string> ld::DocParser::get_all_header_files() const
{
    cc::vector<cc::string> res;

    for (auto const& dir : _cfg.dirs)
        for (auto const& p : std::filesystem::recursive_directory_iterator(dir.c_str()))
        {
            if (!p.is_regular_file())
                continue;

            if (p.path().extension() == ".hh" || p.path().extension() == ".h" || p.path().extension() == ".hpp")
                res.push_back(p.path().c_str());
        }

    return res;
}

cc::vector<cc::string> ld::DocParser::get_all_source_files() const
{
    cc::vector<cc::string> res;

    for (auto const& dir : _cfg.dirs)
        for (auto const& p : std::filesystem::recursive_directory_iterator(dir.c_str()))
        {
            if (!p.is_regular_file())
                continue;

            if (p.path().extension() == ".cc" || p.path().extension() == ".c" || p.path().extension() == ".cpp")
                res.push_back(p.path().c_str());
        }

    return res;
}

ld::file_repo ld::DocParser::parse_file(cc::string_view filename) const
{
    ld::file_repo file;

    file.filename = std::filesystem::absolute(cc::string(filename).c_str()).c_str();
    file.is_header = filename.ends_with(".hh") || filename.ends_with(".h") || filename.ends_with(".hpp");

    auto cfg = cppast::libclang_compile_config();
    cfg.set_flags(cppast::cpp_standard::cpp_1z);

    for (auto const& inc : _cfg.compile_config.include_dirs)
        cfg.add_include_dir(inc.c_str());
    for (auto const& def : _cfg.compile_config.defines)
        cfg.define_macro(def.c_str(), "");

    cppast::stderr_diagnostic_logger logger;
    // logger.set_verbose(true);

    cppast::libclang_parser parser(type_safe::ref(logger));
    cppast::cpp_entity_index idx;
    auto parsed_file = parser.parse(idx, cc::string(filename).c_str(), cfg);

    auto const make_comment = [&](cppast::cpp_entity const& e) -> doc_comment {
        doc_comment c;
        if (e.comment().has_value())
        {
            c.content = e.comment().value().c_str();
            // TODO: line not exposed
        }
        return c;
    };

    auto not_impl_cnt = 0;
    auto indent = 0;
    cppast::visit(*parsed_file, [&](cppast::cpp_entity const& e, cppast::visitor_info const& v) {
        if (v.event != cppast::visitor_info::container_entity_exit)
        {
            auto const add_function_def = [&](auto const& d) -> function_info& {
                auto& def = file.functions.emplace_back();
                def.comment = make_comment(e);
                def.name = d.name().c_str();
                def.unique_name = unique_name_of(d);

                CC_ASSERT((d.parent().has_value() || d.semantic_parent().has_value()) && "why?");

                auto const is_in_class = d.kind() == cppast::cpp_entity_kind::member_function_t //
                                         || d.kind() == cppast::cpp_entity_kind::constructor_t  //
                                         || d.kind() == cppast::cpp_entity_kind::conversion_op_t;

                if (is_in_class)
                {
                    if (d.semantic_parent().has_value())
                    {
                        // TODO: resolve parent use unique name
                        def.class_name = d.semantic_parent().value().name().c_str();
                    }
                    else
                    {
                        CC_ASSERT(d.parent().has_value());
                        auto const& p = d.parent().value();
                        // LOG("{}", d.parent().value().kind());
                        if (p.kind() == cppast::cpp_entity_kind::function_template_t)
                        {
                            CC_ASSERT(p.parent().has_value());
                            CC_ASSERT(p.parent().value().kind() == cppast::cpp_entity_kind::class_t);
                            def.class_name = unique_name_of(static_cast<cppast::cpp_class const&>(p.parent().value()));
                        }
                        else
                        {
                            CC_ASSERT(p.kind() == cppast::cpp_entity_kind::class_t);
                            def.class_name = unique_name_of(static_cast<cppast::cpp_class const&>(p));
                        }
                    }
                }

                // TODO: parameters
                return def;
            };

            switch (e.kind())
            {
            case cppast::cpp_entity_kind::namespace_t:
            {
                auto const& d = static_cast<cppast::cpp_namespace const&>(e);
                auto& def = file.namespaces.emplace_back();
                def.comment = make_comment(e);
                def.name = d.name().c_str();
                def.unique_name = unique_name_of(d);
            }
            break;
            case cppast::cpp_entity_kind::class_t:
            {
                auto const& d = static_cast<cppast::cpp_class const&>(e);
                auto& def = file.classes.emplace_back();
                def.comment = make_comment(e);
                def.name = d.name().c_str();
                def.unique_name = unique_name_of(d);

                // TODO: class vs struct
            }
            break;
            case cppast::cpp_entity_kind::constructor_t:
            {
                auto const& d = static_cast<cppast::cpp_constructor const&>(e);
                auto& def = add_function_def(d);
                def.is_ctor = true;
            }
            break;
            case cppast::cpp_entity_kind::member_function_t:
            {
                auto const& d = static_cast<cppast::cpp_member_function const&>(e);
                auto& def = add_function_def(d);
            }
            break;
            case cppast::cpp_entity_kind::function_t:
            {
                auto const& d = static_cast<cppast::cpp_function const&>(e);
                auto& def = add_function_def(d);
            }
            break;
            case cppast::cpp_entity_kind::type_alias_t:
            {
                auto const& d = static_cast<cppast::cpp_type_alias const&>(e);
                auto& def = file.typedefs.emplace_back();
                def.comment = make_comment(e);
                def.name = d.name().c_str();
                def.unique_name = unique_name_of(d);
            }
            break;
            case cppast::cpp_entity_kind::include_directive_t:
            {
                auto const& d = static_cast<cppast::cpp_include_directive const&>(e);
                if (indent > 1)
                    file.warnings.push_back({"nested include directives are not supported", -1});
                file.includes.push_back(std::filesystem::canonical(d.full_path()).c_str());
            }
            break;
            default:
                not_impl_cnt++;
                LOG("{}[not implemented]", cc::string::filled(indent * 2 + 2, ' '));
            }
        }

        switch (v.event)
        {
        case cppast::visitor_info::container_entity_enter:
            LOG("{}{}: {}", cc::string::filled(indent * 2, ' '), to_string(e.kind()), e.name());
            ++indent;
            break;
        case cppast::visitor_info::container_entity_exit:
            --indent;
            break;
        case cppast::visitor_info::leaf_entity:
            LOG("{}{}: {}", cc::string::filled(indent * 2, ' '), to_string(e.kind()), e.name());
            if (e.comment().has_value())
                LOG("{}comment: '{}'", cc::string::filled(indent * 2 + 2, ' '), e.comment().value());
            break;
        }

        return true;
    });

    if (not_impl_cnt > 0)
        LOG_WARN("DocParser found {} entities with a type that is not implemented", not_impl_cnt);

    return file;
}
