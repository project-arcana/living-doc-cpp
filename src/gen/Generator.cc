#include "Generator.hh"

#include <rich-log/log.hh>

#include <filesystem>

#include <clean-core/assertf.hh>

#include <babel-serializer/file.hh>

namespace fs = std::filesystem;

static constexpr char const* sys_include_dirs[] = {
    "/usr/include/c++/8/",
    "/usr/include/c++/9/",
};

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
    void set(cc::string key, char const* value) { set(key, cc::string(value)); }
    void set(cc::string key, fs::path const& v) { set(key, cc::string(v.c_str())); }
    void set(cc::string key, template_file const& tf) { set(key, tf.content); }
};

struct table_cfg
{
    cc::string table_class = "table-responsive-lg";
    cc::string entry_class = "py-1";
};

cc::string make_table(cc::range_ref<cc::range_ref<cc::string_view>> rows, table_cfg const& cfg = {})
{
    cc::string r;
    r += cc::format(R"(<div class="{}"><table class="table">)", cfg.table_class);
    r += "<tbody>";
    rows.for_each([&](cc::range_ref<cc::string_view> cols) {
        r += "<tr>";
        cols.for_each([&](cc::string_view entry) {
            r += cc::format(R"(<td class="{}">)", cfg.entry_class);
            r += entry;
            r += "</td>";
        });
        r += "</tr>";
    });
    r += "</tbody>";
    r += "</table></div>";
    return r;
}
}

struct ld::Generator::html_gen
{
    Generator& gen;
    fs::path template_dir;
    fs::path target_dir;

    struct
    {
        cc::string default_header;

        cc::string lib_home;
        cc::string lib_reference;

        cc::string header_reference;
        cc::string header_reference_includes;
        cc::string header_reference_classes;

        cc::string doc_nav_block;
        cc::string doc_nav_element;

        cc::string doc_content_nav_item;

    } snippet;

    html_gen(Generator& gen, cc::string const& template_dir_s, cc::string const& target_path_s) : gen(gen)
    {
        template_dir = fs::path(template_dir_s.c_str());
        target_dir = fs::path(target_path_s.c_str());

        CC_ASSERT(template_dir != target_dir);
        CC_ASSERT(fs::is_directory(template_dir) && "must be a directory");
        CC_ASSERT(fs::is_directory(target_dir) && "must be a directory");
        CC_ASSERT(fs::exists(template_dir / "assets") && "needs an assets folder");

        fs::copy(template_dir / "assets", target_dir / "assets", fs::copy_options::recursive | fs::copy_options::update_existing);
        fs::copy(template_dir / "LICENSE", target_dir / "LICENSE", fs::copy_options::overwrite_existing);
        fs::copy(template_dir / "favicon.png", target_dir / "favicon.png", fs::copy_options::overwrite_existing);
        fs::copy(template_dir / "gulpfile.js", target_dir / "gulpfile.js", fs::copy_options::overwrite_existing);
        fs::copy(template_dir / "package.json", target_dir / "package.json", fs::copy_options::overwrite_existing);
        fs::copy(template_dir / ".gitignore", target_dir / ".gitignore", fs::copy_options::overwrite_existing);

        snippet.default_header = load_snippet("_header.html");
        snippet.lib_home = load_snippet("libs/main.html");
        snippet.lib_reference = load_snippet("libs/reference.html");
        snippet.header_reference = load_snippet("libs/ref-header.html");
        snippet.header_reference_includes = load_snippet("libs/ref-header-includes.html");
        snippet.header_reference_classes = load_snippet("libs/ref-header-classes.html");
        snippet.doc_nav_block = load_snippet("docs/nav-block.html");
        snippet.doc_nav_element = load_snippet("docs/nav-element.html");
        snippet.doc_content_nav_item = load_snippet("docs/content-nav-item.html");
    }

    cc::string load_snippet(fs::path p) { return babel::file::read_all_text((template_dir / "snippets" / p).c_str()); };

    void instantiate(fs::path src, fs::path target, cc::function_ref<void(template_file&)> f)
    {
        auto tf = template_file{babel::file::read_all_text(src.c_str())};
        auto& content = tf.content;

        f(tf);

        if (content.contains("{{"))
        {
            auto i = cc::string_view(content).index_of('{');
            LOG_ERROR("missing replacement somewhere around: {}", content.subview(i, 10));
        }

        CC_ASSERT(!content.contains("{{") && "missing replacement");
        babel::file::write(target.c_str(), content);
    };

    //
    // URLs
    //
    cc::string url_for_home() const { return "/"; }
    cc::string url_for_lib_home(library const& lib) const { return cc::format("/{}", lib.name); }
    cc::string url_for_reference(library const& lib) const { return cc::format("/{}/reference", lib.name); }
    cc::string url_for_reference_headers(library const& lib) const { return cc::format("/{}/reference/header", lib.name); }
    cc::string url_for_reference_header(library const& lib, file_repo const& header) const
    {
        return cc::format("/{}/reference/header/{}", lib.name, header.filename_without_path_and_ext());
    }

    // TODO: version?
    cc::string url_for_inc_dir(cc::string_view rel_inc_dir)
    {
        CC_ASSERT(!rel_inc_dir.starts_with('/'));
        for (auto const& lib : gen._libs.values())
            if (rel_inc_dir.starts_with(lib.name))
            {
                auto path = rel_inc_dir.subview(lib.name.size() + 1);
                CC_ASSERT(path.ends_with(".hh") && "others not supported yet");
                path = path.subview(0, path.last_index_of('.'));
                return cc::format("/{}/reference/header/{}", lib.name, path);
            }
        CC_UNREACHABLE("unknown inc dir");
        return rel_inc_dir;
    }
    cc::string url_for_class(class_info const& ci)
    {
        // must parse unique name, get lib, etc.
        return cc::format("/TODO/class/{}", ci.name);
    }

    //
    // home
    //
    void gen_home()
    {
        instantiate(template_dir / "pages" / "default.html", target_dir / "index.html", [&](template_file& f) {
            // snippets
            auto s_lib_card = load_snippet("home/library-card.html");
            auto s_main = load_snippet("home/main.html");

            // meta
            f.set("title", "Project Arcana | Docs");
            f.set("header", snippet.default_header);

            // content
            f.set("content", [&] {
                auto f = template_file{s_main};
                f.set("libraries", [&] {
                    cc::string lib_cards;
                    for (auto const& lib_name : gen._lib_names)
                    {
                        auto const& lib = gen._libs.get(lib_name);
                        CC_ASSERTF(lib.versions.contains_key(lib.current_version), "no default version set for {}", lib.name);
                        auto const& v = lib.versions.get(lib.current_version);
                        CC_ASSERTF(!v.cfg.name.empty(), "no config set for lib {}", lib.name);

                        auto card = template_file{s_lib_card};
                        card.set("icon", v.cfg.icon);
                        card.set("name", v.cfg.name);
                        card.set("url", url_for_lib_home(lib));
                        card.set("summary", v.cfg.description);
                        lib_cards += card.content;
                    }
                    return lib_cards;
                }());
                return f;
            }());
        });
    }

    //
    // lib
    //
    void gen_lib_home(library const& lib, lib_version const& v, fs::path base_dir)
    {
        instantiate(template_dir / "pages" / "default.html", base_dir / "index.html", [&](template_file& f) {
            // snippets

            // meta
            f.set("title", lib.name + " | docs");
            f.set("header", snippet.default_header);

            // content
            f.set("content", [&] {
                auto f = template_file{snippet.lib_home};
                f.set("name", lib.name);
                f.set("description", v.cfg.description);
                f.set("reference_url", url_for_reference(lib));
                return f;
            }());
        });
    }

    //
    // docs
    //
    cc::string make_doc_navigation(cc::string_view curr_block, cc::string_view curr_entry, library const& lib, lib_version const& v, fs::path const& ref_dir)
    {
        cc::string res;

        // headers
        {
            // TODO: link in block title
            auto f = template_file{snippet.doc_nav_block};
            f.set("title", "Headers");
            f.set("elements", [&] {
                cc::string res;
                for (auto const& file : v.files)
                {
                    if (!file.is_header)
                        continue;

                    auto f = template_file{snippet.doc_nav_element};
                    f.set("text", file.filename_without_path());
                    f.set("url", url_for_reference_header(lib, file));
                    f.set("class", " code");

                    res += f.content;
                }
                return res;
            }());

            res += f.content;
        }

        return res;
    }

    //
    // reference
    //
    void gen_lib_reference(library const& lib, lib_version const& v, fs::path ref_dir)
    {
        instantiate(template_dir / "pages" / "doc.html", ref_dir / "index.html", [&](template_file& f) {
            // meta
            f.set("title", lib.name + " | reference");
            f.set("header", snippet.default_header);

            // navigation
            f.set("navigation", make_doc_navigation("", "", lib, v, ref_dir));

            // content
            f.set("content", [&] {
                auto f = template_file{snippet.lib_reference};
                f.set("libname", lib.name);
                return f;
            }());
            f.set("content-nav", "TODO");
        });
    }
    void gen_header_reference(library const& lib, lib_version const& v, file_repo const& header, fs::path ref_dir)
    {
        CC_ASSERT(header.is_header);
        auto target_path = ref_dir / cc::format("{}.html", header.filename_without_path_and_ext()).c_str();

        instantiate(template_dir / "pages" / "doc.html", target_path, [&](template_file& f) {
            // meta
            auto header_inc_dir = v.get_include_name_for(header.filename);
            f.set("title", cc::format("{} | {} reference", header_inc_dir, lib.name));
            f.set("header", snippet.default_header);

            // navigation
            // TODO: set curr
            f.set("navigation", make_doc_navigation("", "", lib, v, ref_dir));

            // content
            cc::string content_nav;
            f.set("content", [&] {
                cc::string r;

                // intro
                {
                    auto f = template_file{snippet.header_reference};
                    f.set("filename", cc::format("&lt;{}&gt;", header_inc_dir));
                    // TOOD: fix me https://getbootstrap.com/docs/4.3/components/alerts/#javascript-behavior
                    f.set("filename-copy", cc::format("#include &lt;{}&gt;", header_inc_dir));
                    r += f.content;
                }

                struct cat
                {
                    cc::string name;
                    cc::string link;
                    cc::string content;
                };

                cc::vector<cat> categories;

                // includes
                if (!header.includes.empty())
                {
                    auto f = template_file{snippet.header_reference_includes};
                    // TODO: sort / categorize me?
                    cc::vector<cc::vector<cc::string>> rows;
                    for (auto const& id : header.includes)
                    {
                        if (v.is_ignored_include(id))
                            continue;

                        auto inc_dir = v.get_include_name_for(id);
                        cc::string url;
                        if (v.is_system_include(id))
                            url = cc::format("https://en.cppreference.com/w/cpp/header/{}", inc_dir);
                        else
                            url = url_for_inc_dir(inc_dir);

                        auto& r = rows.emplace_back();
                        r.push_back(cc::format(R"(<a href="{}" class="code">&lt;{}&gt;</a>)", url, inc_dir));
                        r.push_back("TODO: description");
                    }
                    f.set("includes", make_table(rows));

                    categories.push_back({"Includes", "includes", f.content});
                }

                // classes
                if (!header.classes.empty())
                {
                    auto f = template_file{snippet.header_reference_classes};
                    // TODO: sort / categorize me?
                    cc::vector<cc::vector<cc::string>> rows;
                    for (auto const& c : header.classes)
                    {
                        // TODO: ignore detail?

                        auto url = url_for_class(c);

                        auto& r = rows.emplace_back();
                        r.push_back(cc::format(R"(<a href="{}" class="code">{}</a>)", url, c.name));
                        // TODO: only short desc
                        r.push_back(c.comment.content);
                    }
                    f.set("classes", make_table(rows));

                    categories.push_back({"Classes", "classes", f.content});
                }

                // build result
                for (auto const& c : categories)
                {
                    r += c.content;
                    auto f = template_file{snippet.doc_content_nav_item};
                    f.set("link", c.link);
                    f.set("name", c.name);
                    content_nav += f.content;
                }

                return r;
            }());
            f.set("content-nav", content_nav);
        });
    }

    //
    // generate
    //
    void generate()
    {
        gen_home();

        for (auto const& lib_name : gen._lib_names)
        {
            auto const& lib = gen._libs.get(lib_name);
            auto base_dir = target_dir / lib_name.c_str();
            auto ref_dir = base_dir / "reference";
            auto header_ref_dir = ref_dir / "header";

            auto const& v = lib.versions.get(lib.current_version);

            for (auto const& d : {base_dir, ref_dir, header_ref_dir})
                if (!fs::exists(d))
                    fs::create_directory(d);

            gen_lib_home(lib, v, base_dir);
            gen_lib_reference(lib, v, ref_dir);

            for (auto const& f : v.files)
            {
                if (f.is_header)
                    gen_header_reference(lib, v, f, ref_dir / "header");
            }
        }
    }
};

void ld::Generator::generate_html(cc::string const& template_dir_s, cc::string const& target_path_s)
{
    auto gen = html_gen{*this, template_dir_s, target_path_s};
    gen.generate();
}

cc::string ld::Generator::lib_version::get_include_name_for(cc::string filename) const
{
    CC_ASSERT(filename.starts_with('/') && "must be absolute unix path");

    for (cc::string_view sysdir : sys_include_dirs)
        if (filename.starts_with(sysdir))
            return filename.subview(sysdir.size());

    auto fp = absolute(fs::path(filename.c_str()));
    for (auto const& ds : cfg.compile_config.include_dirs)
    {
        auto d = fs::absolute(fs::path(ds.c_str()));
        if (cc::string_view(fp.c_str()).starts_with(d.c_str()))
            return cc::string_view(fp.c_str()).subview(d.string().size());
    }

    CC_ASSERTF(false, "could not find include name for {}", filename);
    return filename;
}

cc::string ld::Generator::lib_version::get_relative_name_for(cc::string filename) const
{
    CC_ASSERT(filename.starts_with('/') && "must be absolute unix path");

    auto fp = absolute(fs::path(filename.c_str()));
    for (auto const& ds : cfg.compile_config.include_dirs)
    {
        auto d = fs::absolute(fs::path(ds.c_str()));
        if (cc::string_view(fp.c_str()).starts_with(d.c_str()))
            return cc::string_view(fp.c_str()).subview(d.string().size());
    }
    CC_ASSERTF(false, "could not find include name for {}", filename);
    return filename;
}

bool ld::Generator::lib_version::is_ignored_include(cc::string_view filename) const
{
    CC_ASSERT(filename.starts_with('/') && "must be absolute unix path");
    CC_ASSERT(!filename.contains('\\') && "should have been normalized");

    for (auto const& d : cfg.ignored_folders)
        if (filename.contains("/" + d + "/"))
            return true;
    return false;
}

bool ld::Generator::lib_version::is_system_include(cc::string_view filename) const
{
    CC_ASSERT(filename.starts_with('/') && "must be absolute unix path");

    // TODO: more
    for (cc::string_view sysdir : sys_include_dirs)
        if (filename.starts_with(sysdir))
            return true;

    return false;
}
