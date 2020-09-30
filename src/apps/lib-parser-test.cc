#include <nexus/app.hh>

#include <rich-log/log.hh>

#include <parser/DocParser.hh>

#include <babel-serializer/data/json.hh>

APP("lib parser test")
{
    auto cfg = ld::lib_config::from_file("/home/ptrettner/projects/living-doc-cpp/src/apps/test-config.json");

    LOG("config for lib '{}'", cfg.name);

    auto parser = ld::DocParser(cfg);

    // for (auto const& h : parser.get_all_header_files())
    //     LOG("  - {}", h);

    auto repo = parser.parse_file("/home/ptrettner/projects/living-doc-cpp/extern/clean-core/src/clean-core/array.hh");

    LOG("{}", babel::json::to_string(repo, {2}));
}
