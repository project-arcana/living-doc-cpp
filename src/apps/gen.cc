#include <nexus/app.hh>
#include <nexus/args.hh>

#include <clean-core/string.hh>
#include <clean-core/vector.hh>

#include <rich-log/log.hh>

#include <parser/DocParser.hh>

#include <gen/Generator.hh>

#include <babel-serializer/data/json.hh>
#include <babel-serializer/file.hh>

namespace
{
struct gen_config
{
    cc::vector<cc::string> libs;
};

template <class In>
constexpr void introspect(In&& inspect, gen_config& v)
{
    inspect(v.libs, "libs");
}
}

// test setup:
//   - execute in bin
//   ~ gen -c ../src/apps/gen-config.json -o /tmp/arcana-hugo-docs
APP("gen")
{
    // config
    cc::string cfg_path;
    cc::string output_path;
    cc::string template_path;

    // arg parse
    auto args = nx::args("LivingDoc++ Generator") //
                    .add(cfg_path, "c", "configuration path")
                    .add(template_path, "t", "template path")
                    .add(output_path, "o", "output path");

    if (!args.parse())
        return;

    if (template_path.empty())
        template_path = "../template/hugo";

    if (!babel::file::exists(cfg_path))
    {
        LOG("non-existing cfg file '{}'", cfg_path);
        return;
    }

    if (output_path.empty())
    {
        LOG("empty output path");
        return;
    }

    LOG("  config path: {}", cfg_path);
    LOG("  output path: {}", output_path);
    LOG("template path: {}", template_path);

    // exec

    auto cfg = babel::json::read<gen_config>(babel::file::read_all_text(cfg_path));

#define BASE_PATH "/home/ptrettner/projects/living-doc-cpp"

    // DEBUG!!!
    ld::Generator gen;

    ld::DocParser parser;

    {
        auto cfg = ld::lib_config::from_file(BASE_PATH "/src/apps/cfg-clean-core.json");
        gen.set_current_version(cfg.name, "develop");
        parser.add_lib(cfg);
        parser.add_file(cfg.name, BASE_PATH "/extern/clean-core/src/clean-core/array.hh");
        parser.add_file(cfg.name, BASE_PATH "/extern/clean-core/src/clean-core/span.hh");
        parser.add_file(cfg.name, BASE_PATH "/extern/clean-core/src/clean-core/set.hh");
    }

    {
        auto cfg = ld::lib_config::from_file(BASE_PATH "/src/apps/cfg-typed-geometry.json");
        gen.set_current_version(cfg.name, "develop");
        parser.add_lib(cfg);
        parser.add_file(cfg.name, BASE_PATH "/extern/typed-geometry/src/typed-geometry/types/vec.hh");
    }

    {
        auto cfg = ld::lib_config::from_file(BASE_PATH "/src/apps/cfg-reflector.json");
        gen.set_current_version(cfg.name, "develop");
        parser.add_lib(cfg);
        parser.add_file(cfg.name, BASE_PATH "/extern/reflector/src/reflector/introspect.hh");
        parser.add_file(cfg.name, BASE_PATH "/extern/reflector/src/reflector/to_string.hh");
    }

    parser.parse_all(gen);
    gen.generate_hugo(template_path, output_path);
}
