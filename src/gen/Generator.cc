#include "Generator.hh"

#include <rich-log/log.hh>

#include <filesystem>

#include <clean-core/assertf.hh>

#include <babel-serializer/file.hh>

namespace fs = std::filesystem;

// see https://zzo-docs.vercel.app/zdoc/pages/blog/

static constexpr char const* sys_include_dirs[] = {
    "/usr/include/c++/8/",
    "/usr/include/c++/9/",
};

namespace
{
void write_if_changed(fs::path path, cc::range_ref<cc::string_view> lines)
{
    cc::string content;
    lines.for_each([&](cc::string_view line) {
        content += line;
        content += '\n';
    });
    auto old_content = babel::file::exists(path.c_str()) ? babel::file::read_all_text(path.c_str()) : "";
    if (content != old_content)
    {
        LOG("writing changes to '{}'", path.c_str());
        babel::file::write(path.c_str(), content);
    }
}
}

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

struct ld::Generator::hugo_gen
{
    Generator& gen;
    fs::path template_dir;
    fs::path target_dir;

    hugo_gen(Generator& gen, cc::string const& template_dir_s, cc::string const& target_path_s) : gen(gen)
    {
        template_dir = fs::path(template_dir_s.c_str());
        target_dir = fs::path(target_path_s.c_str());

        CC_ASSERT(template_dir != target_dir);
        CC_ASSERT(fs::is_directory(template_dir) && "must be a directory");
        CC_ASSERT(fs::is_directory(target_dir) && "must be a directory");
        // CC_ASSERT(fs::exists(template_dir / "assets") && "needs an assets folder");

        fs::copy(template_dir, target_dir, fs::copy_options::recursive | fs::copy_options::update_existing);
        // fs::copy(template_dir / "LICENSE", target_dir / "LICENSE", fs::copy_options::overwrite_existing);
        // fs::copy(template_dir / "favicon.png", target_dir / "favicon.png", fs::copy_options::overwrite_existing);
        // fs::copy(template_dir / "gulpfile.js", target_dir / "gulpfile.js", fs::copy_options::overwrite_existing);
        // fs::copy(template_dir / "package.json", target_dir / "package.json", fs::copy_options::overwrite_existing);
        // fs::copy(template_dir / ".gitignore", target_dir / ".gitignore", fs::copy_options::overwrite_existing);
    }

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

    // e.g. array.hh ->
    //      https://github.com/project-arcana/clean-core/blob/develop/src/clean-core/array.hh
    cc::string repo_url_for_file(lib_version const& v, file_repo const& file)
    {
        // simple for now
        return cc::format("{}/{}", v.cfg.url.base, file.filename_without_path());
    }

    cc::string make_internal_ref_url(cc::string url)
    {
        // DEBUG! enable me once we have complete docs
        // return "{{< ref \"" + url + "\" >}}";
        return url;
    }

    // NOTE: returns empty string for "hidden" links
    cc::string make_markdown_inc_link(lib_version const& v, cc::string inc_dir)
    {
        if (v.is_ignored_include(inc_dir))
            return "";

        auto inc_name = v.get_include_name_for(inc_dir);

        if (v.is_system_include(inc_dir))
            return cc::format("[{}](https://en.cppreference.com/w/cpp/header/{})", inc_name, inc_name);

        auto url = url_for_inc_dir(inc_name);
        return cc::format("[{}]({})", inc_name, make_internal_ref_url(url));
    }

    struct line_writer
    {
        cc::vector<cc::string> lines;

        template <class... Args>
        void operator()(char const* fmt, Args&&... args)
        {
            if constexpr (sizeof...(args) == 0)
                lines.push_back(fmt);
            else
                lines.push_back(cc::format(fmt, args...));
        };

        void write_if_changed(fs::path path) { ::write_if_changed(path, lines); }
    };

    //
    // gen pages
    //

    void gen_header_ref(library const& lib, lib_version const& v, file_repo const& header, fs::path header_ref_dir)
    {
        line_writer writeln;

        writeln("---");
        writeln("title: \"{}\"", header.filename_without_path());
        writeln("description: \"{}\"", "DESCRIBE ME");
        writeln("---");
        writeln("");

        writeln("View [source]({}) in repository.", repo_url_for_file(v, header));
        writeln("");

        if (!header.typedefs.empty())
        {
            writeln("## Type Aliases");
            for (auto const& t : header.typedefs)
                writeln("* {}", t.unique_name);
            writeln("");
        }

        if (!header.classes.empty())
        {
            writeln("## Classes");
            for (auto const& t : header.classes)
                writeln("* {}", t.unique_name);
            writeln("");
        }

        if (!header.enums.empty())
        {
            writeln("## Enums");
            for (auto const& t : header.enums)
                writeln("* {}", t.unique_name);
            writeln("");
        }

        if (!header.functions.empty())
        {
            writeln("## Functions");
            for (auto const& f : header.functions)
                writeln("* {}", f.unique_name);
            writeln("");
        }

        if (!header.includes.empty())
        {
            writeln("## Includes");
            for (auto const& inc : header.includes)
                if (auto mk = make_markdown_inc_link(v, inc); !mk.empty())
                    writeln("* {}", mk);
            writeln("");
        }

        writeln.write_if_changed(header_ref_dir / (header.filename_without_path_and_ext() + ".md").c_str());
    }

    void make_doc_file(fs::path path, cc::string_view title, cc::string_view layout)
    {
        cc::vector<cc::string> lines;

        lines.push_back("---");
        lines.push_back(cc::format("title: \"{}\"", title));
        lines.push_back(cc::format("description: \"{}\"", "DESCRIBE ME"));
        lines.push_back(cc::format("layout: {}", layout));
        lines.push_back("---");
        lines.push_back("TODO");

        write_if_changed(path, lines);
    }

    //
    // generate
    //
    void generate()
    {
        // gen_home();

        // TODO: versions
        for (auto const& lib_name : gen._lib_names)
        {
            auto const& lib = gen._libs.get(lib_name);
            auto data_path = target_dir / "data" / (lib_name + ".json").c_str();
            auto content_dir = target_dir / "content" / lib_name.c_str();
            auto ref_dir = content_dir / "reference";
            auto update_dir = content_dir / "update";
            auto blog_dir = content_dir / "blog";
            auto tutorial_dir = content_dir / "tutorial";
            auto guide_dir = content_dir / "guide";
            auto header_ref_dir = ref_dir / "header";
            auto type_ref_dir = ref_dir / "type";
            auto fun_ref_dir = ref_dir / "function";
            auto macro_ref_dir = ref_dir / "macro";

            auto const& v = lib.versions.get(lib.current_version);

            for (auto const& d : {
                     content_dir,

                     blog_dir,
                     update_dir,
                     tutorial_dir,
                     guide_dir,

                     ref_dir,
                     header_ref_dir,
                     type_ref_dir,
                     fun_ref_dir,
                     macro_ref_dir,
                 })
                if (!fs::exists(d))
                    fs::create_directory(d);

            // docs.project-arcana.net/clean-core
            make_doc_file(content_dir / "_index.md", lib.name, "lib-home");

            {
                // docs.project-arcana.net/clean-core/update
                make_doc_file(update_dir / "_index.md", "updates", "lib-updates-home");
            }

            {
                // docs.project-arcana.net/clean-core/blog
                make_doc_file(blog_dir / "_index.md", "blog", "lib-blog-home");
            }

            {
                // docs.project-arcana.net/clean-core/tutorial
                make_doc_file(tutorial_dir / "_index.md", "tutorials", "lib-tutorials-home");
            }

            {
                // docs.project-arcana.net/clean-core/guide
                make_doc_file(guide_dir / "_index.md", "guides", "lib-guides-home");
            }

            {
                // docs.project-arcana.net/clean-core/reference
                make_doc_file(ref_dir / "_index.md", "reference", "lib-ref-home");

                // docs.project-arcana.net/clean-core/reference/type
                make_doc_file(type_ref_dir / "_index.md", "types", "lib-type-ref-home");

                // docs.project-arcana.net/clean-core/reference/macro
                make_doc_file(macro_ref_dir / "_index.md", "macros", "lib-macro-ref-home");

                // docs.project-arcana.net/clean-core/reference/function
                make_doc_file(fun_ref_dir / "_index.md", "functions", "lib-function-ref-home");

                {
                    // docs.project-arcana.net/clean-core/reference/header
                    make_doc_file(header_ref_dir / "_index.md", "headers", "lib-header-ref-home");

                    for (auto const& f : v.files)
                    {
                        // TODO: headers in subdirs

                        // docs.project-arcana.net/clean-core/reference/header/vector
                        if (f.is_header)
                            gen_header_ref(lib, v, f, header_ref_dir);
                    }
                }
            }
        }
    }
};

void ld::Generator::generate_hugo(cc::string const& template_dir_s, cc::string const& target_path_s)
{
    auto gen = hugo_gen{*this, template_dir_s, target_path_s};
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
