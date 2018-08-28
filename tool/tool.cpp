﻿#include <cppast/cpp_class.hpp>
#include <cppast/cpp_enum.hpp>
#include <cppast/cpp_member_variable.hpp>
#include <cppast/cpp_member_function.hpp>
#include <cppast/cpp_type.hpp>
#include <cppast/libclang_parser.hpp>
#include <cppast/parser.hpp>
#include <cppast/visitor.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fstream>
#include <sstream>
#include <cassert>
#include <iostream>
#include <string>
#include <functional>
#include <regex>
#include <unordered_set>
#include <filesystem>
#pragma comment(lib,"cppast.lib")
#pragma comment(lib,"_cppast_tiny_process.lib")

bool flag_ismsvc = false;
#if 1
#include <llvm/Support/CommandLine.h>

namespace cl = llvm::cl;
constexpr const char* externname = ".tinyrefl";
bool is_outdated_file(const std::string& file)
{
	using namespace std;
	try
	{
		filesystem::path input_file = file;
		filesystem::path output_file = file + externname;
		assert(filesystem::exists(input_file));

		return !filesystem::exists(output_file) ||
			filesystem::last_write_time(input_file) > filesystem::last_write_time(output_file);
	}
	catch (const std::filesystem::filesystem_error&)
	{

	}
	return false;
}

static const std::string ATTRIBUTES_IGNORE = "tinyrefl::ignore";
static const std::string ATTRIBUTES_REFL = "tinyrefl::meta";

template<typename Seq>
std::string sequence(const Seq& elems, const std::string& separator = ", ",
        const std::string& prefix = "", const std::string& suffix = "")
{
    std::ostringstream os;

    for(std::size_t i = 0; i < elems.size(); ++i)
    {
        os << prefix << elems[i] << suffix;

        if(i < elems.size() - 1)
        {
            os << separator;
        }
    }

    return os.str();
}

struct function_signature_t
{
    std::string return_type;
    std::string class_type;
    std::vector<std::string> arguments;
    std::string qualifiers;

    bool is_member_function() const
    {
        return !class_type.empty();
    }
};

std::ostream& operator<<(std::ostream& os, const function_signature_t& function_signature)
{
    if(function_signature.is_member_function())
    {
        return os << function_signature.return_type << "(" << function_signature.class_type
                  << "::*)" << sequence(function_signature.arguments) << function_signature.qualifiers;
    }
    else
    {
        return os << function_signature.return_type
                  << "(" << sequence(function_signature.arguments) << ")" << function_signature.qualifiers;
    }
}

namespace cppast
{

std::ostream& operator<<(std::ostream& os, const cppast::cpp_attribute& attribute)
{
    if(attribute.scope().has_value())
    {
        os << attribute.scope().value() << "::";
    }

    if(attribute.arguments().has_value())
    {
        return os << attribute.name() << "(" << attribute.arguments().value().as_string() << ")";
    }
    else
    {
        return os << attribute.name();
    }
}

}

bool is_reflectable(const cppast::cpp_entity& e)
{
    return cppast::has_attribute(e, "tinyrefl::on").has_value();
}

void generate_symbol_declaration(std::ostream& os, const std::string& symbol)
{
    fmt::print(os,
"#ifndef TINYREFL_SYMBOL_{0}_DEFINED\n"
"#define TINYREFL_SYMBOL_{0}_DEFINED\n"
"namespace symbols {{\n"
    "CTTI_DEFINE_SYMBOL({0});\n"
"}} // symbols\n"
"#endif // TINYREFL_SYMBOL_{0}_DEFINED\n", symbol);
}

std::string unqualified_name(const cppast::cpp_entity& entity)
{
    auto rename_command = cppast::has_attribute(entity, "tinyrefl::rename");

    if(rename_command)
    {
        const auto arguments = rename_command.value().arguments();

        if(!arguments || arguments.value().empty())
        {
            throw std::runtime_error{fmt::format("[error] expected argument for [[tinyrefl::rename(name)]] attribute")};
        }
        else if(arguments.value().end() - arguments.value().begin() != 1)
        {
            throw std::runtime_error{fmt::format("[error] expected one argument for [[tinyrefl::rename(name)]] attribute")};
        }
        else if(arguments.value().front().kind != cppast::cpp_token_kind::string_literal)
        {
            throw std::runtime_error{fmt::format("[error] expected string literal as argument for [[tinyrefl::rename(name)]] attribute")};
        }
        else
        {
            return arguments.value().front().spelling;
        }
    }
    else
    {
        return entity.name();
    }
}

std::string full_qualified_name(const cppast::cpp_entity& entity)
{
    std::string name = entity.name();

    if(entity.kind() == cppast::cpp_entity_kind::base_class_t)
    {
        return cppast::to_string(static_cast<const cppast::cpp_base_class&>(entity).type());
    }
    else if(entity.parent().has_value() && entity.parent().value().kind() != cppast::cpp_entity_kind::file_t)
    {
        return fmt::format("{}::{}", full_qualified_name(entity.parent().value()), name);
    }
    else
    {
        return fmt::format("{}", name);
    }
}

std::string typelist(const std::vector<std::string>& args)
{
    return fmt::format("TINYREFL_SEQUENCE(({}))", sequence(args, ", "));

}

static std::unordered_set<std::string> string_registry;

void generate_string_definition(std::ostream& os, const std::string& str)
{
    const auto hash = std::hash<std::string>()(str);
    const auto guard = fmt::format("TINYREFL_DEFINE_STRING_{}", hash);

    os << "#if defined(TINYREFL_DEFINE_STRINGS) && !defined(" << guard << ")\n"
       << "#define " << guard << "\n"
       << "TINYREFL_DEFINE_STRING(" << str << ")\n"
       << "#endif //" << guard << "\n\n";
}

void generate_string_definitions(std::ostream& os)
{
    for(const std::string& str : string_registry)
    {
        generate_string_definition(os, str);
    }
}

const std::string& string(const std::string& str)
{
    string_registry.insert(str);
    return str;
}

std::string string_constant(const std::string& str)
{
    return fmt::format("TINYREFL_STRING({})", string(str));
}

namespace cppast
{

std::ostream& operator<<(std::ostream& os, const cpp_type& type)
{
    return os << cppast::to_string(type);
}

}

std::string type(const cppast::cpp_member_function& function)
{
    return string(fmt::format("{}({}::*){}", function.return_type(), full_qualified_name(function.parent().value()), function.signature()));
}

std::string type(const cppast::cpp_member_variable& variable)
{
    return string(fmt::format("{} {}::*", variable.type(), full_qualified_name(variable.parent().value())));
}

std::string type(const cppast::cpp_entity& entity)
{
    return string(entity.name());
}

std::string type(const cppast::cpp_type& type)
{
    return string(cppast::to_string(type));
}

std::string type(const cppast::cpp_enum_value& value)
{
    return type(value.parent().value());
}

std::string full_qualified_type(const cppast::cpp_member_function& function)
{
    return type(function);
}

std::string full_qualified_type(const cppast::cpp_member_variable& variable)
{
    return type(variable);
}

std::string full_qualified_type(const cppast::cpp_entity& entity)
{
    return string(full_qualified_name(entity));
}

std::string full_qualified_type(const cppast::cpp_type& type)
{
    return ::type(type);
}

std::string full_qualified_type(const cppast::cpp_enum_value& value)
{
    return full_qualified_type(value.parent().value());
}

template<typename Entity>
std::string type_reference(const Entity& entity)
{
    return fmt::format("TINYREFL_TYPE({}, {})", type(entity), full_qualified_type(entity));
}

template<typename Entity, typename = std::enable_if_t<std::is_base_of<cppast::cpp_entity, Entity>::value>>
std::string value(const Entity& entity)
{
    return fmt::format("TINYREFL_VALUE({}, {})", type_reference(entity), full_qualified_name(entity));
}

std::string enum_declaration(const std::string& name, const std::vector<std::string>& values)
{
    return fmt::format("enum class {} {{{}}};", name, sequence(values, ", "));
}

std::string attribute(const cppast::cpp_attribute& attribute)
{
    std::vector<std::string> arguments;
    std::ostringstream full_attribute;
    std::string namespace_;

    if(attribute.scope().has_value())
    {
        namespace_ = attribute.scope().value();
        full_attribute << namespace_ << "::" << attribute.name();
    }
    else
    {
        full_attribute << attribute.name();
    }

    if(attribute.arguments().has_value())
    {
        const auto& attribute_arguments = attribute.arguments().value();
        full_attribute << "(" << attribute_arguments.as_string() << ")";
		bool isConnect = false;
		for (auto iter{ attribute_arguments.begin() }, end{ attribute_arguments.end() }; iter != end; ++iter)
		{
			std::string spelling = iter->spelling;
			if (iter->kind == cppast::cpp_token_kind::punctuation)
			{
				isConnect = false;
				if (spelling == ",")
					continue;
				if (spelling == "::")
				{
					arguments.back() += spelling;
					isConnect = true;
					continue;
				}
			}
			if (isConnect)
			{
				arguments.back() += spelling;
			}
			else 
			{
				arguments.push_back(spelling);
			}
		}
		for (auto& val : arguments)
		{
			val = string_constant(val);
		}
    }

    return fmt::format("TINYREFL_ATTRIBUTE({}, {}, {}, {})",
        string_constant(attribute.name()),
        string_constant(namespace_),
        string_constant(full_attribute.str()),
        typelist(arguments)
    );
}

std::string attributes(const cppast::cpp_entity& entity)
{
    std::vector<std::string> attributes;

    for(const auto& attribute : entity.attributes())
    {
        attributes.push_back(::attribute(attribute));
    }

    return typelist(attributes);
}

std::string enum_value(const cppast::cpp_enum_value& enum_value)
{
    return fmt::format("TINYREFL_ENUM_VALUE({}, {}, {}, {}, {})",
        string_constant(enum_value.name()),
        string_constant(full_qualified_name(enum_value)),
        type_reference(enum_value.parent().value()),
        value(enum_value),
        attributes(enum_value)
    );
}

template<typename Member>
std::string member_pointer(const Member& member)
{
    return fmt::format("TINYREFL_VALUE({}, &{})", type_reference(member), full_qualified_name(member));
}

std::string function_signature(const cppast::cpp_member_function& function)
{
    std::vector<std::string> params;

    for(const auto& param : function.parameters())
    {
        params.push_back(cppast::to_string(param.type()));
    }

    return typelist(params);
}

std::string member(const cppast::cpp_member_function& member)
{
    // TINYREFL_MEMBER_FUNCTION(name, fullname, parent_class_type, return_type, signature, pointer, attributes)
    return fmt::format("TINYREFL_MEMBER_FUNCTION({}, {}, {}, {}, {}, {}, /* <attributes> */ {} /* </attributes> */)",
        string_constant(member.name()),
        string_constant(full_qualified_name(member)),
        type_reference(member.parent().value()),
        type_reference(member.return_type()),
        function_signature(member),
        member_pointer(member),
        attributes(member)
    );
}

std::string member(const cppast::cpp_member_variable& member)
{
    // TINYREFL_MEMBER_VARIABLE(name, fullname, parent_class_type, value_type, pointer, attributes)
    return fmt::format("TINYREFL_MEMBER_VARIABLE({}, {}, {}, {}, {}, {})",
        string_constant(member.name()),
        string_constant(full_qualified_name(member)),
        type_reference(member.parent().value()),
        type_reference(member.type()),
        member_pointer(member),
        attributes(member)
    );
}

std::string string_literal(const std::string& str)
{
    return fmt::format("\"{}\"", str);
}

std::string typelist_string(const std::string& str)
{
    return fmt::format("tinyrefl::meta::string<{}>", sequence(str, ", ", "'", "'"));
}

void generate_member(std::ostream& os, const std::string& member)
{
    fmt::print(os, "TINYREFL_REFLECT_MEMBER({})\n", member);
}

bool is_unknown_entity(const cppast::cpp_entity& entity)
{
    auto parent = entity.parent();

    if(entity.name().empty())
    {
        std::cerr << "[warning] Found " << cppast::to_string(entity.kind()) << " with empty name";

        if(parent.has_value())
        {
            std::cerr << " at " << full_qualified_name(parent.value());
        }

        std::cerr << "\n";

        return true;
    }

    if(parent.has_value() && is_unknown_entity(parent.value()))
    {
        return true;
    }

    return false;
}

std::string constructor(const cppast::cpp_constructor& ctor)
{
    return fmt::format("TINYREFL_CONSTRUCTOR({}, {}, {}, TINYREFL_SEQUENCE({}), {})",
        string_constant(ctor.parent().value().name() + ctor.signature()),
        string_constant(full_qualified_name(ctor.parent().value()) + ctor.signature()),
        type_reference(ctor.parent().value()),
        ctor.signature(),
        attributes(ctor)
    );
}

void generate_class(std::ostream& os, const cppast::cpp_class& class_)
{
    std::vector<std::string> member_variables;
    std::vector<std::string> member_functions;
    std::vector<std::string> base_classes;
    std::vector<std::string> enums;
    std::vector<std::string> classes;
    std::vector<std::string> constructors;

    std::cout << " # " << full_qualified_name(class_) << " [attributes: "
                << sequence(class_.attributes(), ", ", "\"", "\"") << "]\n";


    cppast::visit(class_, [&](const cppast::cpp_entity& child, const cppast::visitor_info& info)
    {
        if(cppast::has_attribute(child, ATTRIBUTES_IGNORE) || info.is_old_entity() ||
           info.access != cppast::cpp_access_specifier_kind::cpp_public ||
           cppast::is_templated(child) ||
           child.parent() != class_ ||
           is_unknown_entity(child))
        {
            return;
        }

        switch(child.kind())
        {
            case cppast::cpp_entity_kind::member_function_t:
            {
                std::cout << "    - (member function) " << child.name()
                          << " (signature: " << static_cast<const cppast::cpp_member_function&>(child).signature() << ")" << " [attributes: "
                          << sequence(child.attributes(), ", ", "\"", "\"") << "]\n";

                auto member = ::member(static_cast<const cppast::cpp_member_function&>(child));
                member_functions.push_back(member);
                generate_member(os, member);
                break;
            }
            case cppast::cpp_entity_kind::member_variable_t:
            {
                std::cout << "    - (member variable) " << child.name() << " [attributes: "
                          << sequence(child.attributes(), ", ", "\"", "\"") << "]\n";

                auto member = ::member(static_cast<const cppast::cpp_member_variable&>(child));
                member_variables.push_back(member);
                generate_member(os, member);
                break;
            }
            case cppast::cpp_entity_kind::class_t:
            {
                if(child.name() != class_.name())
                {
                    std::cout << "    - (class) " << child.name() << " ("
                              << (static_cast<const cppast::cpp_class&>(child).is_declaration() ? "declaration" :
                                  static_cast<const cppast::cpp_class&>(child).is_definition() ? "definition" : "") << ") [attributes: "
                              << sequence(child.attributes(), ", ", "\"", "\"") << "]\n";
                    classes.push_back(full_qualified_name(child));
                }
                break;
            }
            case cppast::cpp_entity_kind::enum_t:
            {
                std::cout << "    - (enum) " << child.name() << " [attributes: "
                          << sequence(child.attributes(), ", ", "\"", "\"") << "]\n";
                enums.push_back(full_qualified_name(child));
                break;
            }
            case cppast::cpp_entity_kind::constructor_t:
            {
                const auto& ctor = static_cast<const cppast::cpp_constructor&>(child);

                std::cout << "    - (constructor) "
                          << " (signature: " << ctor.signature() << ")" << " [attributes: "
                          << sequence(child.attributes(), ", ", "\"", "\"") << "]\n";

                constructors.push_back(constructor(ctor));
                break;
            }
            default:
                break;
        }
    });

    for(const auto& base_class : class_.bases())
    {
        if(!cppast::has_attribute(base_class, ATTRIBUTES_IGNORE))
        {
            std::cout << "    - (base) " << full_qualified_name(base_class) << "\n";
            base_classes.push_back(full_qualified_name(base_class));
        }
    }

    fmt::print(os, "TINYREFL_REFLECT_CLASS({}, {}, {}, {}, {}, {}, {}, {}, {})\n",
        string_constant(full_qualified_name(class_)),
        type_reference(class_),
        typelist(base_classes),
        typelist(constructors),
        typelist(member_functions),
        typelist(member_variables),
        typelist(classes),
        typelist(enums),
        attributes(class_)
    );
}

void generate_enum_value(std::ostream& os, const cppast::cpp_enum_value& value)
{
    fmt::print(os, "TINYREFL_REFLECT_ENUM_VALUE({})\n", enum_value(value));
}

void generate_enum(std::ostream& os, const cppast::cpp_enum& enum_)
{
    std::cout << " # " << full_qualified_name(enum_) << " [attributes: "
                << sequence(enum_.attributes(), ", ", "\"", "\"") << "]\n";

    if(!cppast::has_attribute(enum_, ATTRIBUTES_IGNORE))
    {
        std::vector<std::string> values;

        cppast::visit(enum_, [&values, &os](const cppast::cpp_entity& entity, const cppast::visitor_info&)
        {
            if(entity.kind() == cppast::cpp_enum_value::kind() &&
               !is_unknown_entity(entity))
            {
                const auto& value = static_cast<const cppast::cpp_enum_value&>(entity);

                std::cout << "    - (enum value) " << full_qualified_name(entity) << " [attributes: "
                << sequence(value.attributes(), ", ", "\"", "\"") << "]\n";


                generate_enum_value(os, value);
                values.push_back(enum_value(value));
            }
        });

        fmt::print(os, "TINYREFL_REFLECT_ENUM({}, {}, {}, {})\n",
            string_constant(full_qualified_name(enum_)),
            type_reference(enum_),
            typelist(values),
            attributes(enum_)
        );
    }
}

void visit_ast_and_generate(const cppast::cpp_file& ast_root, const std::string& filepath)
{
    std::ofstream os{filepath + externname };

    const auto include_guard = fmt::format("TINYREFL_GENERATED_FILE_{}_INCLUDED", std::hash<std::string>()(filepath));

	os << "#ifndef " << include_guard << "\n"
		<< "#define " << include_guard << "\n\n";
	if (flag_ismsvc)
	{
		os << "#pragma warning(push)\n"
			<< "#pragma warning(disable:4307)\n";
	}
    os  <<
#include "metadata_header.hpp"
       << std::endl;

    std::ostringstream body;

    cppast::visit(ast_root,
        [](const cppast::cpp_entity& e) {
            return !cppast::is_templated(e) &&
                   cppast::is_definition(e) &&
                   !cppast::has_attribute(e, ATTRIBUTES_IGNORE);
        },
        [&body](const cppast::cpp_entity& e, const cppast::visitor_info& info) {
            if(info.is_new_entity() && info.access == cppast::cpp_public && !is_unknown_entity(e))
            {
                switch(e.kind())
                {
                case cppast::cpp_entity_kind::class_t:
                    generate_class(body, static_cast<const cppast::cpp_class&>(e)); break;
                case cppast::cpp_entity_kind::enum_t:
                    generate_enum(body, static_cast<const cppast::cpp_enum&>(e)); break;
                default:
                    break;
                }
            }
        });

    generate_string_definitions(os);
    os << body.str();
	os << "#pragma warning(pop)\n";
    os << "\n#endif // " << include_guard << "\n";

    std::cout << "Done. Metadata saved in " << filepath << ".tinyrefl\n";
}

cppast::cpp_standard get_cpp_standard(const std::string& cpp_standard)
{
    static const std::unordered_map<std::string, cppast::cpp_standard> map{
        {"c++98", cppast::cpp_standard::cpp_98},
        {"c++03", cppast::cpp_standard::cpp_03},
        {"c++11", cppast::cpp_standard::cpp_11},
        {"c++14", cppast::cpp_standard::cpp_14},
        {"c++1z", cppast::cpp_standard::cpp_1z},
    };

    auto it = map.find(cpp_standard);

    if(it != map.end())
    {
        return it->second;
    }
    else
    {
        return cppast::cpp_standard::cpp_latest;
    }
}

struct compile_definition
{
    std::string macro;
    std::string value;

    compile_definition() = default;
    compile_definition(std::string macro, std::string value);
    compile_definition(const std::string& input);
};

compile_definition parse_compile_definition(const std::string& input)
{
    std::size_t equal_sign = input.find_first_of('=');

    if(equal_sign != std::string::npos)
    {
        return {input.substr(0, equal_sign), input.substr(equal_sign + 1)};
    }
    else
    {
        return {input, ""};
    }
}

compile_definition::compile_definition(std::string macro, std::string value) :
    macro{std::move(macro)},
    value{std::move(value)}
{}

compile_definition::compile_definition(const std::string& input) :
    compile_definition{parse_compile_definition(input)}
{}

std::istream& operator>>(std::istream& is, compile_definition& compile_definition)
{
    std::string input;
    is >> input;
    compile_definition = parse_compile_definition(input);
    return is;
}
namespace {
	int parse_number(const char*& str)
	{
		auto result = 0;
		for (; *str && *str != '.'; ++str)
		{
			result *= 10;
			result += int(*str - '0');
		}
		return result;
	}
}

inline void format_path(std::string& path)
{
	path = std::filesystem::path(path).generic_string();
}
inline void format_path(cl::opt<std::string>& path)
{
	format_path(path.getValue());
}
template<typename... path>
void format_paths(path&... ps)
{
	(format_path(ps), ...);
}
#include <windows.h>
bool reflect_file(std::string filepath
	, const cppast::cpp_standard cpp_standard
	, cl::list<std::string>& include_dirs
	, cl::list<std::string>& definitions
	, const std::vector<std::string>& custom_flags
	, cl::list<std::string>& warnings
	, std::string clangbin, std::string clangversion,std::string clangsystemfile,bool nodry,bool msvcSupport)
{
	flag_ismsvc = msvcSupport;
	format_paths(filepath, clangbin, clangsystemfile);
	for (auto& p : include_dirs)
		format_path(p);
	try
	{
		if (clangbin.empty())
		{
			std::wstring buffer;
			buffer.resize(4096);
			if (::GetModuleFileNameW(NULL, buffer.data(),4096) > 0)
			{
				std::filesystem::path path = buffer;
				path.remove_filename();
				std::filesystem::directory_iterator iterator(path);
				for (const std::filesystem::directory_entry& directoryEntry : iterator)
				{
					if (directoryEntry.is_regular_file() && directoryEntry.path().filename() == "clang.exe")
					{
						std::cout << "in path:" << std::filesystem::current_path() << "find clang.exe (using version clang 6.0.0)" << std::endl;
						clangbin = directoryEntry.path().generic_string();
						clangversion = "6.0.0";
						clangsystemfile = (path += "/clang/6.0.0/include").generic_string();
						if (!std::filesystem::exists(clangsystemfile))
						{
							std::cout << "but not find llvm lib include dir:" << clangsystemfile << std::endl;
							clangbin.clear();
							clangversion.clear();
							clangsystemfile.clear();
						}
						break;
					}
				}
			}
		}
		if (!clangbin.empty())
		{
			bool ret = std::filesystem::exists(clangbin);
			if (clangversion.empty())
			{
				std::cout << "need " << clangversion << std::endl;
				return false;
			}
			ret |= std::filesystem::exists(clangsystemfile);
			include_dirs.push_back(clangsystemfile);
		}

	}
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cout << e.what() << std::endl;
		return false;
	}
    using parser_t = cppast::simple_file_parser<cppast::libclang_parser>;

    if(!is_outdated_file(filepath) && !nodry)
    {
        std::cout << "file " << filepath << " metadata is up to date, skipping\n";
        return true;
    }
    cppast::cpp_entity_index index;
    parser_t parser{type_safe::ref(index)};
    parser_t::config config;
	cppast::compile_flags flag; 
	if (msvcSupport)
	{
		flag |= cppast::compile_flag::ms_compatibility | cppast::compile_flag::ms_extensions;;
	}
    config.set_flags(cpp_standard, flag);
	if (!clangbin.empty())
	{
		auto ptr = clangversion.c_str();
		auto major = parse_number(ptr);
		auto minor = parse_number(ptr);
		auto patch = parse_number(ptr);
		config.set_clang_binary(clangbin, major, minor, patch);
	}

    std::cout << "parsing file " << filepath << " "
        << cppast::to_string(cpp_standard) << " ";

    // Add definitions to identify that the translation unit
    // is being parsed by tinyrefl-tool
    definitions.addValue("TINYREFL_TOOL_RUNNING");
    definitions.addValue("__tinyrefl__");

    for(const auto& definition : definitions)
    {
        compile_definition def{definition};
        std::cout << "-D" << def.macro << "=" << def.value << " ";

        if(def.macro.empty())
        {
            std::cout << "(empty, ignored) ";
        }
        else
        {
            config.define_macro(def.macro, def.value);
        }
    }

    for(const std::string& include_dir : include_dirs)
    {
        std::cout << "-I" << include_dir << " ";

        if(include_dir.empty())
        {
            std::cout << "(empty, ignored) ";
        }
        else
        {
            config.add_include_dir(include_dir);
        }
    }

    for(const std::string& warning : warnings)
    {
        auto warning_flag = "-W" + warning;
        std::cout << warning_flag << " ";
        config.add_flag(warning_flag);
    }

    for(const auto& flag : custom_flags)
    {
        std::cout << flag << " ";
        config.add_flag(flag);
    }


    // Tell libclang to ignore unknown arguments
    std::cout << " -Qunused-arguments -Wno-unknown-warning-option";
    config.add_flag("-Qunused-arguments");
    config.add_flag("-Wno-unknown-warning-option");
    std::cout << " ...\n";

    try
    {
        auto file = parser.parse(filepath, config);

        if(file.has_value())
        {
            visit_ast_and_generate(file.value(), filepath);
            return true;
        }
        else
        {
            std::cerr << "error parsing input file\n";
        }
    } catch(const cppast::libclang_error& error)
    {
        std::cerr << "[error] " << error.what() << "\n";
    }

    return false;
}
inline std::string WCharToChar(const wchar_t* str)
{
	return std::filesystem::path(str).generic_string();
}

//extern "C"{
//	__declspec(dllexport) bool reflect_file_execute(const wchar_t* filepath
//		, const cppast::cpp_standard cpp_standard
//		, const wchar_t** include_dirs, size_t include_dirs_size
//		, const wchar_t** definitions, size_t definitions_size
//		, const wchar_t** custom_flags, size_t custom_flags_size
//		, const wchar_t* clangbin, const wchar_t* clangversion, const wchar_t* clangsystemdir, bool nodry, bool msvcSupport)
//	{
//		cl::list<std::string> includedirs(cl::NumOccurrencesFlag::Optional);
//		for (int i = 0; i < include_dirs_size; ++i)
//		{
//			includedirs.push_back(WCharToChar(include_dirs[i]));
//		}
//		cl::list<std::string> definitions1(cl::NumOccurrencesFlag::Optional);
//		for (int i = 0; i < definitions_size; ++i)
//		{
//			includedirs.push_back(WCharToChar(definitions[i]));
//		}
//		std::vector<std::string> customflags;
//		for (int i = 0; i < custom_flags_size; ++i)
//		{
//			includedirs.push_back(WCharToChar(custom_flags[i]));
//		}
//		cl::list<std::string> warnings(cl::NumOccurrencesFlag::Optional);
//		return reflect_file(WCharToChar(filepath), cpp_standard
//			, includedirs, definitions1, customflags, warnings
//			, WCharToChar(clangbin), WCharToChar(clangversion), WCharToChar(clangsystemdir), nodry, msvcSupport);
//	}
//
//	__declspec(dllexport) const wchar_t* reflect_generate_path(const wchar_t* filepath)
//	{
//		std::filesystem::path path(filepath);
//		path += externname;
//		static std::wstring storeBuffer;
//		storeBuffer = path.generic_wstring();
//		return storeBuffer.c_str();
//	}
//}

int main(int argc, char** argv)
{
	cl::opt<std::string>          filename{ cl::Positional, cl::desc("<input header>"), cl::Required };
	cl::list<std::string>         includes{ "I", cl::Prefix, cl::ValueOptional, cl::desc("Include directories") };
	cl::list<std::string>         definitions{ "D", cl::Prefix, cl::ValueOptional, cl::desc("Compile definitions") };
	cl::list<std::string>         warnings{"W", cl::Prefix, cl::ValueOptional, cl::desc("Warnings")};
	cl::opt<bool>				  parseWithMsvc{ "msvc",cl::Prefix,cl::ValueOptional,cl::desc("support msvc parse") };
	cl::opt<std::string>          clangBinary{ "cl", cl::Prefix, cl::ValueOptional, cl::desc("clang binary path, exp:-cl=\"H:\\llvm\\clang.exe\"") };
	cl::opt<std::string>          clangBinaryVersion{ "clv", cl::Prefix, cl::ValueOptional, cl::desc("clang binary exe version,exp:-clv=6.0.0") };
	cl::opt<std::string>          clanggSystemIncludeDir{ "SI", cl::Prefix, cl::ValueOptional, cl::desc("clang system include dir, exp:-SI=\"H:\\llvm\\lib\\include\"") };
	cl::opt<cppast::cpp_standard> stdversion{ "std", cl::desc("C++ standard"), cl::values(
		clEnumValN(cppast::cpp_standard::cpp_98, "c++98", "C++ 1998 standard"),
		clEnumValN(cppast::cpp_standard::cpp_03, "c++03", "C++ 2003 standard"),
		clEnumValN(cppast::cpp_standard::cpp_11, "c++11", "C++ 2011 standard"),
		clEnumValN(cppast::cpp_standard::cpp_14, "c++14", "C++ 2014 standard"),
		clEnumValN(cppast::cpp_standard::cpp_1z, "c++17", "C++ 2017 standard")
	) };
	cl::list<std::string> custom_flags{ cl::Sink, cl::desc("Custom compiler flags") };
	cl::opt<bool>		  nodry{ "nodry",cl::Prefix,cl::ValueOptional,cl::desc("must update file") };
    if(cl::ParseCommandLineOptions(argc, argv, "Tinyrefl codegen tool"))
    {
		if (reflect_file(filename, stdversion, includes, definitions, custom_flags,warnings,
			clangBinary, clangBinaryVersion, clanggSystemIncludeDir,
			nodry, parseWithMsvc))
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 2;
    }
}

#endif