#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

#include <hashclash/cli11_options.hpp>

namespace po = cli11_options;

namespace {

class args {
  public:
    args(std::initializer_list<std::string> values) : storage_(values) {
        for (auto &value : storage_)
            argv_.push_back(value.data());
    }
    int argc() const { return static_cast<int>(argv_.size()); }
    char **argv() { return argv_.data(); }

  private:
    std::vector<std::string> storage_;
    std::vector<char *> argv_;
};

} // namespace

TEST_CASE("config file parses scalar, bool and string options") {
    bool flag = false;
    unsigned num = 7;
    std::string input;

    po::options_description desc("Allowed options");
    desc.add_options()("flag", po::bool_switch(&flag),
                       "a flag")("num,n", po::value<unsigned>(&num)->default_value(7),
                                 "a number")("input,i", po::value<std::string>(&input), "an input");

    std::istringstream config("flag = true\nnum = 5\ninput = hello world\n");
    po::variables_map vm;
    REQUIRE_NOTHROW(po::store(po::parse_config_file(config, desc), vm));
    po::notify(vm);

    CHECK(flag);
    CHECK(num == 5);
    CHECK(input == "hello world");
}

TEST_CASE("boolean switches apply their default once without erasing explicit values") {
    bool flag = true;
    po::options_description desc;
    desc.add_options()("flag", po::bool_switch(&flag), "a flag");
    args empty{"hashclash"};

    SECTION("an absent switch defaults to false") {
        auto parsed = po::parse_command_line(empty.argc(), empty.argv(), desc);
        CHECK_FALSE(flag);
        CHECK(parsed.present.count("flag") == 0);
    }
    SECTION("an empty config file also initializes the switch") {
        std::istringstream config;
        po::parse_config_file(config, desc);
        CHECK_FALSE(flag);
    }
    SECTION("printing help does not initialize the target prematurely") {
        std::ostringstream help;
        help << desc;
        CHECK(flag);
        po::parse_command_line(empty.argc(), empty.argv(), desc);
        CHECK_FALSE(flag);
    }
    SECTION("an explicit switch survives a subsequent empty config and help") {
        args argv{"hashclash", "--flag"};
        po::parse_command_line(argv.argc(), argv.argv(), desc);
        std::istringstream config;
        po::parse_config_file(config, desc);
        std::ostringstream help;
        help << desc;
        CHECK(flag);
    }
    SECTION("an explicit default overrides the normal false default") {
        bool enabled = false;
        desc.add_options()("enabled", po::bool_switch(&enabled)->default_value(true), "enabled");
        po::parse_command_line(empty.argc(), empty.argv(), desc);
        CHECK(enabled);
        CHECK_FALSE(flag);
    }
}

TEST_CASE("config file parses multi-token option values") {
    std::vector<int> values;

    po::options_description desc("Allowed options");
    desc.add_options()("diffm0", po::value<std::vector<int>>(&values)->multitoken(), "a vector");

    std::istringstream config("diffm0 = 1 2 3\n");
    po::variables_map vm;
    REQUIRE_NOTHROW(po::store(po::parse_config_file(config, desc), vm));
    po::notify(vm);

    CHECK(values == std::vector<int>{1, 2, 3});
}

TEST_CASE("config file expands vector options without explicit multitoken") {
    std::vector<unsigned> values;

    po::options_description desc("Allowed options");
    desc.add_options()("diffm0", po::value<std::vector<unsigned>>(&values),
                       "message bit differences");

    std::istringstream config("diffm0 = 1 2 3\n");
    po::variables_map vm;
    REQUIRE_NOTHROW(po::store(po::parse_config_file(config, desc), vm));
    po::notify(vm);

    CHECK(values == std::vector<unsigned>{1, 2, 3});
}

TEST_CASE("long option names are recognized") {
    bool flag = false;
    std::string input;

    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "Show options.")("inputfile,i", po::value<std::string>(&input),
                                                  "an input")("startnearcollision",
                                                              po::bool_switch(&flag), "a flag");

    args argv{"hashclash", "--help", "--inputfile", "prefix.bin", "--startnearcollision"};
    po::variables_map vm;
    po::store(po::command_line_parser(argv.argc(), argv.argv()).options(desc).run(), vm);
    po::notify(vm);

    CHECK(vm.count("help") == 1);
    CHECK(input == "prefix.bin");
    CHECK(flag);
    CHECK(vm.count("startnearcollision") == 1);
}

TEST_CASE("short multi-token option does not swallow positional arguments") {
    std::vector<std::string> outputs;
    std::string prefix;

    po::options_description desc("Allowed options");
    desc.add_options()("out,o", po::value<std::vector<std::string>>(&outputs)->multitoken(),
                       "output files")("prefixfile", po::value<std::string>(&prefix), "prefix");

    po::positional_options_description positional;
    positional.add("prefixfile", 1);

    args argv{"hashclash", "-o", "a.bin", "b.bin"};
    po::variables_map vm;
    po::store(po::command_line_parser(argv.argc(), argv.argv())
                  .options(desc)
                  .positional(positional)
                  .run(),
              vm);
    po::notify(vm);

    CHECK(outputs == std::vector<std::string>{"a.bin", "b.bin"});
    CHECK(vm.count("prefixfile") == 0);
    CHECK(prefix.empty());
}

TEST_CASE("positional argument before a multi-token option is preserved") {
    std::vector<std::string> outputs;
    std::string prefix;

    po::options_description desc("Allowed options");
    desc.add_options()("out,o", po::value<std::vector<std::string>>(&outputs)->multitoken(),
                       "output files")("prefixfile", po::value<std::string>(&prefix), "prefix");

    po::positional_options_description positional;
    positional.add("prefixfile", 1);

    args argv{"hashclash", "prefix.bin", "-o", "a.bin", "b.bin"};
    po::variables_map vm;
    po::store(po::command_line_parser(argv.argc(), argv.argv())
                  .options(desc)
                  .positional(positional)
                  .run(),
              vm);
    po::notify(vm);

    CHECK(prefix == "prefix.bin");
    CHECK(outputs == std::vector<std::string>{"a.bin", "b.bin"});
    CHECK(vm.count("prefixfile") == 1);
}

TEST_CASE("short option with a scalar value is registered") {
    unsigned step = 0;

    po::options_description desc("Allowed options");
    desc.add_options()("tstep,t", po::value<unsigned>(&step)->default_value(0), "a step");

    args argv{"hashclash", "-t", "12"};
    po::variables_map vm;
    po::store(po::command_line_parser(argv.argc(), argv.argv()).options(desc).run(), vm);
    po::notify(vm);

    CHECK(step == 12);
    CHECK(vm.count("tstep") == 1);
}

TEST_CASE("compact short options and combined flags are registered") {
    unsigned step = 0;
    po::options_description desc;
    desc.add_options()("tstep,t", po::value<unsigned>(&step), "step")("help,h", "help")("verbose,v",
                                                                                        "verbose");
    args argv{"hashclash", "-t12", "-hv"};
    po::variables_map vm;
    po::store(po::parse_command_line(argv.argc(), argv.argv(), desc), vm);
    CHECK(step == 12);
    CHECK(vm.count("tstep") == 1);
    CHECK(vm.count("help") == 1);
    CHECK(vm.count("verbose") == 1);
}

TEST_CASE("command line overrides configuration without reapplying defaults") {
    unsigned step = 99, threads = 99, limit = 99;
    bool enabled = false;
    std::string workdir;
    std::vector<int> bits;
    po::options_description desc;
    desc.add_options()("tstep,t", po::value<unsigned>(&step)->default_value(0), "step")(
        "workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
        "directory")("threads", po::value<unsigned>(&threads)->default_value(1),
                     "threads")("limit", po::value<unsigned>(&limit)->default_value(10), "limit")(
        "enabled", po::bool_switch(&enabled), "enabled")("bits", po::value<std::vector<int>>(&bits),
                                                         "bits");
    args argv{"hashclash", "-t12", "-w", "custom", "--enabled", "--bits", "3", "4"};
    po::variables_map vm;
    po::store(po::parse_command_line(argv.argc(), argv.argv(), desc), vm);
    std::istringstream config("tstep = 7\nthreads = 8\nenabled = false\nbits = 1 2\n");
    po::store(po::parse_config_file(config, desc), vm);
    po::notify(vm);
    CHECK(step == 12);
    CHECK(workdir == "custom");
    CHECK(threads == 8);
    CHECK(limit == 10);
    CHECK(enabled);
    CHECK(bits == std::vector<int>{3, 4});
    CHECK(vm.count("tstep") == 1);
    CHECK(vm.count("threads") == 1);
    CHECK(vm.count("limit") == 0);

    std::istringstream empty_config;
    po::store(po::parse_config_file(empty_config, desc), vm);
    CHECK(workdir == "custom");
    CHECK(threads == 8);
}

TEST_CASE("formatting option help does not reset parsed values") {
    unsigned step = 99;
    po::options_description desc;
    desc.add_options()("tstep,t", po::value<unsigned>(&step)->default_value(0), "step");
    args argv{"hashclash", "-t12"};
    po::parse_command_line(argv.argc(), argv.argv(), desc);
    std::ostringstream help;
    help << desc;
    CHECK(step == 12);
}

TEST_CASE("a bare -- ends option parsing and leaves the remaining arguments positional") {
    std::string prefix;
    bool verbose = false;
    po::options_description desc;
    desc.add_options()("prefixfile", po::value<std::string>(&prefix),
                       "prefix")("verbose,v", po::bool_switch(&verbose), "verbose");
    po::positional_options_description positional;
    positional.add("prefixfile", 1);

    auto parse = [&](args &argv) {
        po::variables_map vm;
        REQUIRE_NOTHROW(po::store(po::command_line_parser(argv.argc(), argv.argv())
                                      .options(desc)
                                      .positional(positional)
                                      .run(),
                                  vm));
        po::notify(vm);
        return vm;
    };

    SECTION("an option-looking argument without -- is still an option") {
        args argv{"hashclash", "--verbose"};
        auto vm = parse(argv);
        CHECK(verbose);
        CHECK(prefix.empty());
        CHECK(vm.count("prefixfile") == 0);
    }
    SECTION("-- keeps an unknown short option positional") {
        args argv{"hashclash", "--", "-x"};
        auto vm = parse(argv);
        CHECK(prefix == "-x");
        CHECK_FALSE(verbose);
        CHECK(vm.count("prefixfile") == 1);
    }
    SECTION("-- keeps a registered option positional") {
        args argv{"hashclash", "--", "--verbose"};
        auto vm = parse(argv);
        CHECK(prefix == "--verbose");
        CHECK_FALSE(verbose);
        CHECK(vm.count("prefixfile") == 1);
    }
}

TEST_CASE("attached multi-token values do not become positional arguments") {
    std::vector<std::string> outputs;
    std::string prefix;
    po::options_description desc;
    desc.add_options()("out,o", po::value<std::vector<std::string>>(&outputs),
                       "outputs")("prefixfile", po::value<std::string>(&prefix), "prefix");
    po::positional_options_description positional;
    positional.add("prefixfile", 1);

    auto check = [&](args &argv) {
        po::variables_map vm;
        po::store(po::command_line_parser(argv.argc(), argv.argv())
                      .options(desc)
                      .positional(positional)
                      .run(),
                  vm);
        CHECK(outputs == std::vector<std::string>{"a.bin", "b.bin"});
        CHECK(prefix.empty());
        CHECK(vm.count("prefixfile") == 0);
        CHECK(vm.count("out") == 1);
    };
    SECTION("compact short option") {
        args argv{"hashclash", "-oa.bin", "b.bin"};
        check(argv);
    }
    SECTION("long option with equals") {
        args argv{"hashclash", "--out=a.bin", "b.bin"};
        check(argv);
    }
}

TEST_CASE("arguments after -- cannot become options when positional slots are exhausted") {
    bool verbose = false;
    std::string prefix;
    po::options_description desc;
    desc.add_options()("verbose,v", po::bool_switch(&verbose),
                       "verbose")("prefixfile", po::value<std::string>(&prefix), "prefix");
    po::positional_options_description positional;

    SECTION("no positional arguments are supported") {
        args argv{"hashclash", "--", "--verbose"};
        CHECK_THROWS_AS(po::command_line_parser(argv.argc(), argv.argv()).options(desc).run(),
                        CLI::ExtrasError);
    }
    SECTION("all positional arguments were supplied before the separator") {
        positional.add("prefixfile", 1);
        args argv{"hashclash", "prefix.bin", "--", "--verbose"};
        CHECK_THROWS_AS(po::command_line_parser(argv.argc(), argv.argv())
                            .options(desc)
                            .positional(positional)
                            .run(),
                        CLI::ExtrasError);
    }
    SECTION("the separator is followed by more arguments than there are slots") {
        positional.add("prefixfile", 1);
        args argv{"hashclash", "--", "prefix.bin", "--verbose"};
        CHECK_THROWS_AS(po::command_line_parser(argv.argc(), argv.argv())
                            .options(desc)
                            .positional(positional)
                            .run(),
                        CLI::ExtrasError);
    }
    CHECK_FALSE(verbose);
}
