#include <nexus/app.hh>

#include <rich-log/log.hh>

/// stuff

#include <cppast/cpp_entity_kind.hpp>
#include <cppast/libclang_parser.hpp>
#include <cppast/visitor.hpp>

APP("parser test")
{
    // auto db_dir = "/home/ptrettner/builds/living-doc-cpp/ZapCC_64bit/Debug";
    auto parse_file = "/home/ptrettner/projects/living-doc-cpp/extern/clean-core/src/clean-core/array.hh";
    // auto parse_file = __FILE__;

    // auto db = cppast::libclang_compilation_database(db_dir);

    // auto cfg = cppast::libclang_compile_config(db, parse_file);
    auto cfg = cppast::libclang_compile_config();
    cfg.set_flags(cppast::cpp_standard::cpp_1z);

    cfg.add_include_dir("/home/ptrettner/projects/living-doc-cpp/extern/clean-core/src");

    cppast::stderr_diagnostic_logger logger;
    // logger.set_verbose(true);

    cppast::libclang_parser parser(type_safe::ref(logger));
    cppast::cpp_entity_index idx;
    auto file = parser.parse(idx, parse_file, cfg);

    auto indent = 0;
    cppast::visit(*file, [&](cppast::cpp_entity const& e, cppast::visitor_info const& v) {
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

    /*
    for (auto const& e : *file)
        if (e.kind() == cppast::cpp_entity_kind::include_directive_t)
        {
            auto const& id = static_cast<cppast::cpp_include_directive const&>(e);
            LOG("include {}", e.name());
            LOG("  full path: {}", id.full_path());
            for (auto const& e : id.target().get(idx))
                LOG("   .. {}", e->name());
        }
        */

    // LOG("file: {}", file->name());
    // for (auto const& c : file->unmatched_comments())
    //     LOG("  unmatched comment: '{}'", c.content);
    // for (auto const& e : *file)
    // {
    //     auto cmt = e.comment().has_value() ? e.comment().value() : "no doc";
    //     LOG("  entity: {} ({}, {})", e.name(), to_string(e.kind()), cmt);
    // }
}
