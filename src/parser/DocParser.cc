#include "DocParser.hh"

#include <filesystem>

#include <rich-log/log.hh>

#include <clean-core/format.hh>
#include <clean-core/overloaded.hh>
#include <clean-core/to_string.hh>
#include <clean-core/unique_function.hh>

#include <parser/unique_name.hh>

#include <gen/Generator.hh>

#include <cppast/cpp_entity_kind.hpp>
#include <cppast/cpp_function.hpp>
#include <cppast/cpp_member_function.hpp>
#include <cppast/cpp_member_variable.hpp>
#include <cppast/cpp_namespace.hpp>
#include <cppast/cpp_type_alias.hpp>
#include <cppast/libclang_parser.hpp>
#include <cppast/visitor.hpp>

/*
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
}*/

void ld::DocParser::add_lib(lib_config cfg)
{
    CC_ASSERT(!_libs.contains_key(cfg.name) && "lib already added");
    _libs[cfg.name].cfg = cc::move(cfg);
}

void ld::DocParser::add_file(cc::string_view lib, cc::string filename)
{
    CC_ASSERT(_libs.contains_key(lib) && "lib must be added first");
    _libs.get(lib).files_to_parse.push_back(filename);
}

void ld::DocParser::parse_all(Generator& gen)
{
    // global parser state
    cppast::stderr_diagnostic_logger logger;
    // logger.set_verbose(true);
    cppast::libclang_parser parser(type_safe::ref(logger));
    cppast::cpp_entity_index idx;

    cc::vector<cc::unique_function<void()>> deferred_parses;

    for (auto const& [libname, lib] : _libs)
    {
        // init lib in gen
        deferred_parses.push_back([this, &gen, libname = libname] {
            auto const& cfg = _libs.get(libname).cfg;
            gen.set_lib_config(libname, cfg.version, cfg);
        });

        // setup parser cfg
        auto cfg = cppast::libclang_compile_config();
        cfg.set_flags(cppast::cpp_standard::cpp_17);

        for (auto const& inc : lib.cfg.compile_config.include_dirs)
            cfg.add_include_dir(inc.c_str());
        for (auto const& def : lib.cfg.compile_config.defines)
            cfg.define_macro(def.c_str(), "");

        // parse files
        for (auto const& filename : lib.files_to_parse)
        {
            auto parsed_file = parser.parse(idx, filename.c_str(), cfg);

            deferred_parses.push_back([this, &idx, libname = libname, filename = filename, &gen, parsed_file = cc::move(parsed_file)] {
                auto const& cfg = _libs.get(libname).cfg;

                ld::file_repo file;

                file.filename = std::filesystem::absolute(cc::string(filename).c_str()).c_str();
                file.is_header = filename.ends_with(".hh") || filename.ends_with(".h") || filename.ends_with(".hpp");

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
                            def.unique_name = unique_name_of(d, idx);

                            if (d.is_variadic())
                                def.is_variadic = true;

                            if (d.is_constexpr())
                                def.is_constexpr = true;

                            if (d.noexcept_condition().has_value())
                            {
                                def.is_noexcept = true;
                                def.noexcept_condition = unique_name_of(d.noexcept_condition().value(), idx);
                            }

                            if (d.body_kind() == cppast::cpp_function_body_kind::cpp_function_defaulted)
                                def.is_defaulted = true;
                            if (d.body_kind() == cppast::cpp_function_body_kind::cpp_function_deleted)
                                def.is_deleted = true;

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
                                        def.class_name = unique_name_of(static_cast<cppast::cpp_class const&>(p.parent().value()), idx);
                                    }
                                    else
                                    {
                                        CC_ASSERT(p.kind() == cppast::cpp_entity_kind::class_t);
                                        def.class_name = unique_name_of(static_cast<cppast::cpp_class const&>(p), idx);
                                    }
                                }
                            }

                            // TODO: parameters
                            return def;
                        };
                        auto const process_member_fun = [&](function_info& def, cppast::cpp_member_function_base const& e) {
                            if (is_const(e.cv_qualifier()))
                                def.is_const = true;
                            if (is_volatile(e.cv_qualifier()))
                                def.is_volatile = true;

                            if (e.ref_qualifier() == cppast::cpp_reference::cpp_ref_lvalue)
                                def.is_lvalue_ref = true;
                            if (e.ref_qualifier() == cppast::cpp_reference::cpp_ref_rvalue)
                                def.is_rvalue_ref = true;

                            if (e.is_virtual())
                                def.is_virtual = true;
                            if (is_pure(e.virtual_info()))
                                def.is_pure = true;
                            if (is_overriding(e.virtual_info()))
                                def.is_override = true;
                            if (is_final(e.virtual_info()))
                                def.is_final = true;

                            // TODO: return type
                        };

                        switch (e.kind())
                        {
                        case cppast::cpp_entity_kind::namespace_t:
                        {
                            auto const& d = static_cast<cppast::cpp_namespace const&>(e);
                            auto& def = file.namespaces.emplace_back();
                            def.comment = make_comment(e);
                            def.name = d.name().c_str();
                            def.unique_name = unique_name_of(d, idx);
                        }
                        break;
                        case cppast::cpp_entity_kind::class_t:
                        {
                            auto const& d = static_cast<cppast::cpp_class const&>(e);
                            auto& def = file.classes.emplace_back();
                            def.comment = make_comment(e);
                            def.name = d.name().c_str();
                            def.unique_name = unique_name_of(d, idx);

                            // TODO: class vs struct
                        }
                        break;
                        case cppast::cpp_entity_kind::constructor_t:
                        {
                            auto const& d = static_cast<cppast::cpp_constructor const&>(e);
                            auto& def = add_function_def(d);
                            def.is_ctor = true;
                            if (d.is_explicit())
                                def.is_explicit = true;
                        }
                        break;
                        case cppast::cpp_entity_kind::function_t:
                        {
                            auto const& d = static_cast<cppast::cpp_function const&>(e);
                            auto& def = add_function_def(d);
                        }
                        break;
                        case cppast::cpp_entity_kind::member_function_t:
                        {
                            auto const& d = static_cast<cppast::cpp_member_function const&>(e);
                            auto& def = add_function_def(d);
                            process_member_fun(def, d);
                        }
                        break;
                        case cppast::cpp_entity_kind::conversion_op_t:
                        {
                            auto const& d = static_cast<cppast::cpp_conversion_op const&>(e);
                            auto& def = add_function_def(d);
                            def.is_conversion = true;
                            process_member_fun(def, d);
                        }
                        break;
                        case cppast::cpp_entity_kind::type_alias_t:
                        {
                            auto const& d = static_cast<cppast::cpp_type_alias const&>(e);
                            auto& def = file.typedefs.emplace_back();
                            def.comment = make_comment(e);
                            def.name = d.name().c_str();
                            def.unique_name = unique_name_of(d, idx);
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

                gen.add_file(libname, cfg.version, cc::move(file));
            });
        }
    }

    for (auto const& parse : deferred_parses)
        parse();
}
