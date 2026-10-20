#ifndef HASHCLASH_CLI11_OPTIONS_HPP
#define HASHCLASH_CLI11_OPTIONS_HPP

#include <CLI/CLI.hpp>

#include <functional>
#include <cstddef>
#include <istream>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>
#include <utility>

namespace cli11_options {

class options_description;

template <typename T> struct is_std_vector : std::false_type {};
template <typename T, typename Allocator>
struct is_std_vector<std::vector<T, Allocator>> : std::true_type {};
template <typename T> inline constexpr bool is_std_vector_v = is_std_vector<T>::value;

template <typename T>
concept stream_insertable =
    requires(std::ostringstream &stream, const T &value) { stream << value; };

template <typename T> class value_spec {
  public:
    value_spec(T *target) : target_(target) {}
    value_spec *operator->() { return this; }
    const value_spec *operator->() const { return this; }
    value_spec *default_value(const T &value) {
        default_value_ = value;
        has_default_ = true;
        return this;
    }
    value_spec *multitoken() {
        multitoken_ = true;
        return this;
    }
    T *target_;
    T default_value_{};
    bool has_default_ = false;
    bool multitoken_ = false;
};

template <typename T> value_spec<T> value(T *target) { return value_spec<T>(target); }

inline value_spec<bool> bool_switch(bool *target) {
    value_spec<bool> option(target);
    option.default_value(false);
    return option;
}

struct variables_map {
    std::unordered_set<std::string> present;
    std::size_t count(const std::string &name) const { return present.count(name); }
};

struct parsed_options {
    int argc = 0;
    char **argv = nullptr;
    options_description *options = nullptr;
    std::vector<std::string> positional_names;
    std::vector<std::string> owned_arguments;
    std::vector<char *> owned_argv;
    std::unordered_set<std::string> present;
};

class options_description {
    struct definition {
        std::string spec;
        std::string description;
        std::function<void(CLI::App &, bool)> add;
        bool takes_value = false;
        bool multitoken = false;
        bool vector_value = false;
    };
    std::string caption_;
    std::vector<definition> definitions_;
    std::unique_ptr<CLI::App> app_storage_;
    std::unordered_set<std::string> present_;
    std::unordered_set<std::string> command_line_present_;
    bool defaults_applied_ = false;

    static std::string_view long_name(std::string_view spec) {
        auto name = spec.substr(0, spec.find(','));
        if (name.starts_with("--"))
            name.remove_prefix(2);
        return name;
    }
    static std::string_view short_name(std::string_view spec) {
        auto comma = spec.find(',');
        return comma == std::string_view::npos ? std::string_view{} : spec.substr(comma + 1);
    }
    static std::string cli_spec(std::string_view spec) {
        const std::string name = long_name_with_dashes(spec);
        const auto shorthand = short_name(spec);
        if (shorthand.empty())
            return name;
        return "-" + std::string(shorthand) + "," + name;
    }
    static bool is_option(std::string_view argument) {
        return argument.size() > 1 && argument.front() == '-';
    }
    static bool matches_option(std::string_view argument, std::string_view spec) {
        if (argument == long_name_with_dashes(spec))
            return true;
        auto shorthand = short_name(spec);
        return !shorthand.empty() && argument == "-" + std::string(shorthand);
    }
    static std::string long_name_with_dashes(std::string_view spec) {
        auto name = spec.substr(0, spec.find(','));
        if (name.starts_with("--"))
            return std::string(name);
        return "--" + std::string(name);
    }
    const definition *find_definition(std::string_view argument) const {
        for (const auto &definition : definitions_)
            if (matches_option(argument, definition.spec))
                return &definition;
        return nullptr;
    }
    const definition *find_definition_by_name(std::string_view name) const {
        return find_definition("--" + std::string(name));
    }
    static bool consumes_values(const definition &definition) {
        return definition.takes_value && (definition.multitoken || definition.vector_value);
    }

    void build(CLI::App &app) {
        app.set_help_flag("");
        for (const auto &definition : definitions_)
            definition.add(app, !defaults_applied_);
        defaults_applied_ = true;
    }

  public:
    explicit options_description(std::string caption = "Allowed options")
        : caption_(std::move(caption)) {}

    class option_adder {
        options_description &owner_;

      public:
        explicit option_adder(options_description &owner) : owner_(owner) {}
        option_adder &operator()(const char *spec, const char *description) {
            owner_.definitions_.push_back({spec, description,
                                           [spec, description](CLI::App &app, bool) {
                                               app.add_flag(options_description::cli_spec(spec),
                                                            description);
                                           },
                                           false, false, false});
            return *this;
        }
        template <typename T>
        option_adder &operator()(const char *spec, value_spec<T> option, const char *description) {
            owner_.definitions_.push_back(
                {spec, description,
                 [spec, description, option](CLI::App &app, bool apply_defaults) {
                     if constexpr (std::is_same_v<T, bool>) {
                         if (apply_defaults && option.has_default_)
                             *option.target_ = option.default_value_;
                         app.add_flag(options_description::cli_spec(spec), *option.target_,
                                      description);
                     } else {
                         auto *cli_option = app.add_option(options_description::cli_spec(spec),
                                                           *option.target_, description);
                         if constexpr (stream_insertable<T>) {
                             if (option.has_default_) {
                                 std::ostringstream value;
                                 value << option.default_value_;
                                 if (apply_defaults)
                                     cli_option->default_val(value.str());
                                 else
                                     cli_option->default_str(value.str());
                             }
                         }
                         if (option.multitoken_ || is_std_vector_v<T>)
                             cli_option->expected(1, -1);
                     }
                 },
                 !std::is_same_v<T, bool>, option.multitoken_, is_std_vector_v<T>});
            return *this;
        }
        template <typename T>
        option_adder &operator()(const char *spec, value_spec<T> *option, const char *description) {
            return (*this)(spec, *option, description);
        }
    };

    option_adder add_options() { return option_adder(*this); }
    options_description &add(const options_description &other) {
        definitions_.insert(definitions_.end(), other.definitions_.begin(),
                            other.definitions_.end());
        return *this;
    }
    CLI::App &parse(int argc, char **argv, parsed_options &result,
                    const std::vector<std::string> &positional = {}, bool from_config = false) {
        app_storage_ = std::make_unique<CLI::App>(caption_);
        build(*app_storage_);
        std::vector<std::string> arguments;
        arguments.reserve(static_cast<std::size_t>(argc) + positional.size());
        arguments.emplace_back(argv[0]);
        std::size_t positional_index = 0;
        bool positional_only = false;
        for (int i = 1; i < argc; ++i) {
            std::string argument(argv[i]);
            if (!positional_only && argument == "--") {
                // Boost.Program_options treats everything after a bare '--' as
                // positional; CLI11 would report the separator itself as an
                // unexpected argument, so drop it and stop option parsing here.
                positional_only = true;
                continue;
            }
            if ((positional_only || !is_option(argument)) && positional_index < positional.size()) {
                arguments.emplace_back("--" + positional[positional_index++]);
                arguments.push_back(std::move(argument));
            } else if (positional_only) {
                // No positional slot remains. Preserve the separator so CLI11
                // rejects excess arguments instead of interpreting them as options.
                arguments.emplace_back("--");
                for (; i < argc; ++i)
                    arguments.emplace_back(argv[i]);
                break;
            } else {
                arguments.push_back(std::move(argument));
                if (!positional_only && is_option(arguments.back())) {
                    std::string_view name(arguments.back());
                    const auto equal = name.find('=');
                    bool attached_value = equal != std::string_view::npos;
                    name = name.substr(0, equal);
                    if (!name.starts_with("--") && name.size() > 2) {
                        name = name.substr(0, 2);
                        attached_value = true;
                    }
                    const auto *definition = find_definition(name);
                    if (definition != nullptr && definition->takes_value) {
                        if (!attached_value && i + 1 < argc)
                            arguments.emplace_back(argv[++i]);
                        if (consumes_values(*definition)) {
                            while (i + 1 < argc && !is_option(argv[i + 1]))
                                arguments.emplace_back(argv[++i]);
                        }
                    }
                }
            }
        }
        std::vector<char *> rewritten_argv;
        rewritten_argv.reserve(arguments.size());
        for (auto &argument : arguments)
            rewritten_argv.push_back(argument.data());
        result.argc = argc;
        result.argv = argv;
        result.options = this;
        result.positional_names = positional;
        app_storage_->parse(static_cast<int>(rewritten_argv.size()), rewritten_argv.data());
        result.present.clear();
        for (const auto &definition : definitions_) {
            const auto name = std::string(long_name(definition.spec));
            if (app_storage_->count(long_name_with_dashes(definition.spec)) != 0) {
                result.present.insert(name);
                present_.insert(name);
                if (!from_config)
                    command_line_present_.insert(name);
            }
        }
        return *app_storage_;
    }
    bool count(const std::string &name) const { return present_.contains(name); }
    bool is_multitoken(const std::string &name) const {
        const auto *definition = find_definition_by_name(name);
        return definition != nullptr && (definition->multitoken || definition->vector_value);
    }
    friend struct parser;
    friend void store(parsed_options, variables_map &);
    friend parsed_options parse_config_file(std::istream &, options_description &);
    friend std::ostream &operator<<(std::ostream &, const options_description &);
};

struct parser {
    options_description &options;
    parser &options_(options_description &) { return *this; }
    parsed_options run() { return {}; }
};

inline parsed_options parse_command_line(int argc, char **argv, options_description &options) {
    parsed_options result;
    options.parse(argc, argv, result);
    return result;
}

inline void store(parsed_options parsed, variables_map &vm) {
    vm.present.insert(parsed.present.begin(), parsed.present.end());
}
inline void notify(variables_map &) {}
inline void store(parsed_options, options_description &) {}
inline void notify(options_description &) {}
inline parsed_options parse_config_file(std::istream &input, options_description &options) {
    parsed_options result;
    result.owned_arguments.emplace_back("config");
    std::string line;
    while (std::getline(input, line)) {
        auto comment = line.find_first_of("#;");
        if (comment != std::string::npos)
            line.erase(comment);
        auto begin = line.find_first_not_of(" \t\r\n");
        auto end = line.find_last_not_of(" \t\r\n");
        if (begin == std::string::npos)
            continue;
        line = line.substr(begin, end - begin + 1);
        auto separator = line.find('=');
        if (separator == std::string::npos)
            continue;
        auto key = line.substr(0, separator);
        auto value = line.substr(separator + 1);
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        // Explicit command-line values take precedence over configuration,
        // including bool and vector options.
        if (options.command_line_present_.contains(key))
            continue;
        // Boost.Program_options splits multi-token values on whitespace; CLI11
        // expects one token per value, so expand them here.
        if (options.is_multitoken(key)) {
            result.owned_arguments.emplace_back("--" + key);
            std::istringstream values(value);
            std::string token;
            while (values >> token)
                result.owned_arguments.push_back(std::move(token));
        } else {
            result.owned_arguments.emplace_back("--" + key + "=" + value);
        }
    }
    for (auto &argument : result.owned_arguments)
        result.owned_argv.push_back(argument.data());
    result.argc = static_cast<int>(result.owned_argv.size());
    result.argv = result.owned_argv.data();
    options.parse(result.argc, result.argv, result, {}, true);
    return result;
}

class command_line_parser {
    int argc_;
    char **argv_;
    options_description *options_ = nullptr;
    std::vector<std::string> positional_;

  public:
    command_line_parser(int argc, char **argv) : argc_(argc), argv_(argv) {}
    command_line_parser &options(options_description &options) {
        options_ = &options;
        return *this;
    }
    command_line_parser &positional(const class positional_options_description &positional);
    parsed_options run() {
        parsed_options result;
        options_->parse(argc_, argv_, result, positional_);
        return result;
    }
};

class positional_options_description {
    struct entry {
        std::string name;
        int count;
    };
    std::vector<entry> entries_;

  public:
    void add(const char *name, int count) { entries_.push_back({name, count}); }
    friend class command_line_parser;
};

inline command_line_parser &
command_line_parser::positional(const positional_options_description &positional) {
    for (const auto &entry : positional.entries_)
        for (int i = 0; i < entry.count; ++i)
            positional_.push_back(entry.name);
    return *this;
}

inline std::ostream &operator<<(std::ostream &out, const variables_map &) { return out; }
inline std::ostream &operator<<(std::ostream &out, const options_description &options) {
    CLI::App app(options.caption_);
    app.set_help_flag("");
    for (const auto &definition : options.definitions_)
        definition.add(app, false);
    return out << app.help();
}

} // namespace cli11_options

#endif
