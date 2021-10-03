#include <nexus/app.hh>

#include <rich-log/log.hh>

#include <parser/DocParser.hh>

#include <gen/Generator.hh>

#include <babel-serializer/data/json.hh>

#define BASE_PATH "/home/ptrettner/projects/living-doc-cpp"

APP("lib gen test")
{
    ld::Generator gen;

    {
        auto cfg = ld::lib_config::from_file(BASE_PATH "/src/apps/cfg-clean-core.json");
        auto parser = ld::DocParser(cfg);
        gen.set_current_version("clean-core", "develop");
        gen.set_lib_config("clean-core", cfg.version, cfg);
        gen.add_file("clean-core", cfg.version, parser.parse_file(BASE_PATH "/extern/clean-core/src/clean-core/array.hh"));
        gen.add_file("clean-core", cfg.version, parser.parse_file(BASE_PATH "/extern/clean-core/src/clean-core/set.hh"));
    }

    {
        auto cfg = ld::lib_config::from_file(BASE_PATH "/src/apps/cfg-typed-geometry.json");
        auto parser = ld::DocParser(cfg);
        gen.set_current_version("typed-geometry", "develop");
        gen.set_lib_config("typed-geometry", cfg.version, cfg);
        gen.add_file("typed-geometry", cfg.version, parser.parse_file(BASE_PATH "/extern/typed-geometry/src/typed-geometry/types/vec.hh"));
    }

    {
        auto cfg = ld::lib_config::from_file(BASE_PATH "/src/apps/cfg-reflector.json");
        auto parser = ld::DocParser(cfg);
        gen.set_current_version("reflector", "develop");
        gen.set_lib_config("reflector", cfg.version, cfg);
        gen.add_file("reflector", cfg.version, parser.parse_file(BASE_PATH "/extern/reflector/src/reflector/introspect.hh"));
        gen.add_file("reflector", cfg.version, parser.parse_file(BASE_PATH "/extern/reflector/src/reflector/to_string.hh"));
    }

    CC_ASSERT(false && "not supported anymore");
    // gen.generate_html(BASE_PATH "/template", "/home/ptrettner/tmp/arcana-docs");
}
