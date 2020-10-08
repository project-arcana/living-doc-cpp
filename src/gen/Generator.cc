#include "Generator.hh"

#include <filesystem>

#include <clean-core/assertf.hh>

#include <babel-serializer/file.hh>

void ld::Generator::set_current_version(cc::string_view lib_name, cc::string version) { get_lib(lib_name).current_version = version; }

void ld::Generator::add_file(cc::string_view lib_name, cc::string_view version, ld::file_repo file)
{
    get_lib_version(lib_name, version).files.push_back(cc::move(file));
}

void ld::Generator::set_lib_config(cc::string_view lib_name, cc::string_view version, ld::lib_config cfg)
{
    get_lib_version(lib_name, version).cfg = cc::move(cfg);
}

ld::Generator::library& ld::Generator::get_lib(cc::string_view name)
{
    if (!_libs.contains_key(name))
    {
        auto& l = _libs[name];
        l.name = name;
        _lib_names.push_back(name);
    }

    return _libs.get(name);
}

ld::Generator::lib_version& ld::Generator::get_lib_version(cc::string_view name, cc::string_view version)
{
    auto& l = get_lib(name);
    if (!l.versions.contains_key(version))
    {
        auto& v = l.versions[version];
        v.version = version;
    }
    return l.versions.get(version);
}

namespace
{
struct template_file
{
    cc::string content;

    void set(cc::string key, cc::string const& value)
    {
        key = "{{" + key + "}}";
        CC_ASSERTF(content.contains(key), "key not found: {}", key);
        content.replace(key, value);
    }
    void set(cc::string key, template_file const& tf) { set(key, tf.content); }
};
}

void ld::Generator::generate_html(cc::string const& template_dir_s, cc::string const& target_path_s)
{
    namespace fs = std::filesystem;

    auto template_dir = fs::path(template_dir_s.c_str());
    auto target_path = fs::path(target_path_s.c_str());

    CC_ASSERT(template_dir != target_path);
    CC_ASSERT(fs::is_directory(template_dir) && "must be a directory");
    CC_ASSERT(fs::is_directory(target_path) && "must be a directory");
    CC_ASSERT(fs::exists(template_dir / "assets") && "needs an assets folder");

    fs::copy(template_dir / "assets", target_path / "assets", fs::copy_options::recursive | fs::copy_options::update_existing);
    fs::copy(template_dir / "LICENSE", target_path / "LICENSE", fs::copy_options::overwrite_existing);
    fs::copy(template_dir / "favicon.png", target_path / "favicon.png", fs::copy_options::overwrite_existing);
    fs::copy(template_dir / "gulpfile.js", target_path / "gulpfile.js", fs::copy_options::overwrite_existing);
    fs::copy(template_dir / "package.json", target_path / "package.json", fs::copy_options::overwrite_existing);
    fs::copy(template_dir / ".gitignore", target_path / ".gitignore", fs::copy_options::overwrite_existing);

    auto instantiate = [&](fs::path src, fs::path target, cc::function_ref<void(template_file&)> f) {
        auto tf = template_file{babel::file::read_all_text(src.c_str())};
        auto& content = tf.content;

        f(tf);

        CC_ASSERT(!content.contains("{{") && "missing replacement");
        babel::file::write(target.c_str(), content);
    };

    auto load_snippet = [&](fs::path p) { return babel::file::read_all_text((template_dir / "snippets" / p).c_str()); };

    auto default_header = load_snippet("_header.html");

    //
    // home
    //

    instantiate(template_dir / "pages" / "default.html", target_path / "index.html", [&](template_file& f) {
        // snippets
        auto s_lib_card = load_snippet("home/library-card.html");
        auto s_main = load_snippet("home/main.html");

        // meta
        f.set("title", "Project Arcana | Docs");
        f.set("header", default_header);

        // content
        f.set("content", [&] {
            auto f = template_file{s_main};
            f.set("libraries", [&] {
                cc::string lib_cards;
                for (auto const& lib_name : _lib_names)
                {
                    auto const& lib = _libs.get(lib_name);
                    CC_ASSERTF(lib.versions.contains_key(lib.current_version), "no default version set for {}", lib.name);
                    auto const& v = lib.versions.get(lib.current_version);
                    CC_ASSERTF(!v.cfg.name.empty(), "no config set for lib {}", lib.name);

                    auto card = template_file{s_lib_card};
                    card.set("icon", v.cfg.icon);
                    card.set("name", v.cfg.name);
                    card.set("url", lib_name);
                    card.set("summary", v.cfg.description);
                    lib_cards += card.content;
                }
                return lib_cards;
            }());
            return f;
        }());
    });

    //
    // per lib home
    //
    auto s_lib_main = load_snippet("libs/main.html");
    for (auto const& lib_name : _lib_names)
    {
        auto const& lib = _libs.get(lib_name);
        auto base_dir = target_path / lib_name.c_str();
        auto ref_dir = base_dir / "reference";

        auto const& v = lib.versions.get(lib.current_version);
        if (!fs::exists(base_dir))
            fs::create_directory(base_dir);
        if (!fs::exists(ref_dir))
            fs::create_directory(ref_dir);

        instantiate(template_dir / "pages" / "default.html", base_dir / "index.html", [&](template_file& f) {
            // snippets

            // meta
            f.set("title", lib_name + " | docs");
            f.set("header", default_header);

            // content
            f.set("content", [&] {
                auto f = template_file{s_lib_main};
                f.set("name", lib.name);
                f.set("description", v.cfg.description);
                //
                return f;
            }());
        });

        instantiate(template_dir / "pages" / "doc.html", ref_dir / "index.html", [&](template_file& f) {
            // snippets

            // meta
            f.set("title", lib_name + " | reference");
            f.set("header", default_header);

            // content
            // f.set("content", [&] {
            //     auto f = template_file{s_lib_main};
            //     f.set("name", lib.name);
            //     f.set("description", v.cfg.description);
            //     //
            //     return f;
            // }());
        });
    }
}
