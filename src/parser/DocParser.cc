#include "DocParser.hh"

#include <filesystem>

#include <rich-log/log.hh>

#include <cppast/cpp_entity_kind.hpp>
#include <cppast/cpp_namespace.hpp>
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
            switch (e.kind())
            {
            case cppast::cpp_entity_kind::namespace_t:
            {
                auto const& d = static_cast<cppast::cpp_namespace const&>(e);
                auto& def = file.namespaces.emplace_back();
                def.comment = make_comment(e);
                def.name = d.name().c_str();
                // TODO: unique_name
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
