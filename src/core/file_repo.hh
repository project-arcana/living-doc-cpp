#pragma once

#include <clean-core/string.hh>
#include <clean-core/string_view.hh>
#include <clean-core/vector.hh>

namespace ld
{
/// a documentation comment
struct doc_comment
{
    cc::string content;
    int line = -1;

    void merge(doc_comment const& c)
    {
        if (content.empty())
            *this = c;
        else
        {
            // TODO: remember lines?
            content += "\n";
            content += c.content;
        }
    }
};

template <class In>
constexpr void introspect(In&& inspect, doc_comment& v)
{
    inspect(v.content, "content");
    inspect(v.line, "line");
}

/// entity information: a C++ namespace
struct namespace_info
{
    /// empty for anon
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// doc comment if any
    doc_comment comment;
};

template <class In>
constexpr void introspect(In&& inspect, namespace_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.comment, "comment");
}

/// entity information: a C++ namespace
struct namespace_alias_info
{
    /// empty for anon
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// unique name of the target namespace
    cc::string target_namespace;

    /// doc comment if any
    doc_comment comment;
};

template <class In>
constexpr void introspect(In&& inspect, namespace_alias_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.target_namespace, "target_namespace");
    inspect(v.comment, "comment");
}

/// entity information: a C++ enum
struct enum_info
{
    /// empty for anon
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// doc comment if any
    doc_comment comment;
};

template <class In>
constexpr void introspect(In&& inspect, enum_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.comment, "comment");
}

/// entity information: a C++ macro
struct macro_info
{
    /// without parameters
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// doc comment if any
    doc_comment comment;
};

template <class In>
constexpr void introspect(In&& inspect, macro_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.comment, "comment");
}

/// entity information: a C++ function
struct function_info
{
    /// without parameters
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// for members: unique name of the class this function belongs to
    cc::string class_name;

    /// doc comment if any
    doc_comment comment;

    /// noexcept(<expr>)
    cc::string noexcept_condition;

    bool is_member_fun() const { return !class_name.empty(); }

    bool is_ctor = false;
    bool is_defaulted = false;
    bool is_deleted = false;
    bool is_constexpr = false;
    bool is_const = false;
    bool is_volatile = false;
    bool is_lvalue_ref = false;
    bool is_rvalue_ref = false;
    bool is_conversion = false;
    bool is_noexcept = false;
    bool is_virtual = false;
    bool is_pure = false;
    bool is_override = false;
    bool is_final = false;
    bool is_variadic = false;
    bool is_explicit = false;
};

template <class In>
constexpr void introspect(In&& inspect, function_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.class_name, "class_name");
    inspect(v.comment, "comment");
    inspect(v.noexcept_condition, "noexcept_condition");

    inspect(v.is_ctor, "is_ctor");
    inspect(v.is_defaulted, "is_defaulted");
    inspect(v.is_deleted, "is_deleted");
    inspect(v.is_constexpr, "is_constexpr");
    inspect(v.is_const, "is_const");
    inspect(v.is_volatile, "is_volatile");
    inspect(v.is_lvalue_ref, "is_lvalue_ref");
    inspect(v.is_rvalue_ref, "is_rvalue_ref");
    inspect(v.is_conversion, "is_conversion");
    inspect(v.is_noexcept, "is_noexcept");
    inspect(v.is_virtual, "is_virtual");
    inspect(v.is_pure, "is_pure");
    inspect(v.is_override, "is_override");
    inspect(v.is_final, "is_final");
    inspect(v.is_variadic, "is_variadic");
    inspect(v.is_explicit, "is_explicit");
}

/// entity information: a C++ class
struct class_info
{
    /// empty for anon
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// doc comment if any
    doc_comment comment;
};

template <class In>
constexpr void introspect(In&& inspect, class_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.comment, "comment");
}

/// entity information: a C++ typedef
struct typedef_info
{
    cc::string name;

    /// unique name for internal addressing
    cc::string unique_name;

    /// doc comment if any
    doc_comment comment;
};

template <class In>
constexpr void introspect(In&& inspect, typedef_info& v)
{
    inspect(v.name, "name");
    inspect(v.unique_name, "unique_name");
    inspect(v.comment, "comment");
}

struct warning
{
    cc::string message;
    int line = 0; // NOTE: currently not really supported in cppast
};

template <class In>
constexpr void introspect(In&& inspect, warning& v)
{
    inspect(v.message, "message");
    inspect(v.line, "line");
}

/**
 * A repository of entities found in a c++ file (header or source)
 *
 * Each header or source file creates a single object of this struct
 * Documentation is generated from the set of all file_repos of all versions
 *
 * Entities only link to their parent, never to their children
 * (because children can be easily added to other files but not the other way around)
 *
 * Entities are not added hierarchically here but as flat lists
 * This allows easy scanning and processing
 *
 * The unique name of an entity contains template arguments and function arg types,
 * e.g. some_ns::nsB::class<class,int,int>::inner_class<class>::member_fun<int,int>(int,ns::some_class<class>&&)const
 * (TODO: how are template args called? T0..TN?)
 *
 * NOTE: entities can occur multiple times in the same file, e.g. multiple namespace decls of the same namespace
 */
struct file_repo
{
    /// slightly redundant but helps diagnose errors: the filename of the file that this repo belongs to
    /// (absolute path)
    cc::string filename;

    /// e.g. vector.hh for src/clean-core/vector.hh
    cc::string filename_without_path() const;
    /// e.g. vector for src/clean-core/vector.hh
    cc::string filename_without_path_and_ext() const;

    /// if true, belongs to a header file
    bool is_header = false;

    /// list of all includes (absolute paths)
    cc::vector<cc::string> includes;

    /// flat list of all (potentially nested) namespaces in this file
    cc::vector<namespace_info> namespaces;

    /// flat list of all (potentially inner) namespace aliases in this file
    cc::vector<namespace_alias_info> namespaces_aliases;

    /// flat list of all (potentially nested) enums in this file
    cc::vector<enum_info> enums;

    /// flat list of all (potentially nested) classes in this file
    cc::vector<class_info> classes;

    /// flat list of all (potentially member) functions in this file
    cc::vector<function_info> functions;

    /// flat list of all (potentially nested) typedefs and type aliases in this file
    cc::vector<typedef_info> typedefs;

    /// warnings during the doc parsing process
    cc::vector<warning> warnings;

    static file_repo from_file(cc::string_view filename);
    void write_to_file(cc::string_view filename, bool compact = true) const;
};

template <class In>
constexpr void introspect(In&& inspect, file_repo& v)
{
    inspect(v.filename, "filename");
    inspect(v.is_header, "is_header");
    inspect(v.includes, "includes");
    inspect(v.namespaces, "namespaces");
    inspect(v.namespaces_aliases, "namespaces_aliases");
    inspect(v.enums, "enums");
    inspect(v.classes, "classes");
    inspect(v.functions, "functions");
    inspect(v.typedefs, "typedefs");
    inspect(v.warnings, "warnings");
}
}
