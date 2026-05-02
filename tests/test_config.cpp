// test_config.cpp — Unit tests for deusridet::Config
// Tests: file parsing, typed getters, edge cases, MachinaConfig from_config

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include "communis/config.h"

using namespace deusridet;
using Catch::Approx;

TEST_CASE("Config — parse key=value", "[config]") {
    auto tmp = std::filesystem::temp_directory_path() / "test_config_%.conf";
    std::string path = (tmp.generic_string() + std::to_string(getpid())).substr(0, 80);
    
    std::ofstream ofs(path);
    ofs << "# comment\n";
    ofs << "model = T5000\n";
    ofs << "cores=2560\n";
    ofs << "enabled = true\n";
    ofs << "freq = 1.575\n";
    ofs << "  indented_key = value  \n";
    ofs.close();

    Config cfg;
    REQUIRE(cfg.load(path));
    
    CHECK(cfg.get_string("model") == "T5000");
    CHECK(cfg.get_string("cores") == "2560");
    CHECK(cfg.get_bool("enabled") == true);
    CHECK(cfg.get_double("freq", 0.0) == Approx(1.575));
    CHECK(cfg.get_string("indented_key") == "value");
    CHECK(cfg.has("model"));
    CHECK(!cfg.has("nonexistent"));

    std::filesystem::remove(path);
}

TEST_CASE("Config — typed getters with defaults", "[config]") {
    Config cfg;
    CHECK(cfg.get_string("missing", "default") == "default");
    CHECK(cfg.get_int("missing", 42) == 42);
    CHECK(cfg.get_double("missing", 3.14) == Approx(3.14));
    CHECK(cfg.get_bool("missing", true) == true);
}

TEST_CASE("Config — invalid int/double falls back to default", "[config]") {
    auto path = std::filesystem::temp_directory_path() / "test_config_invalid.conf";
    
    std::ofstream ofs(path);
    ofs << "bad_int = not_a_number\n";
    ofs << "bad_double = also_bad\n";
    ofs.close();

    Config cfg;
    cfg.load(path.string());
    
    CHECK(cfg.get_int("bad_int", -1) == -1);
    CHECK(cfg.get_double("bad_double", 0.0) == Approx(0.0));

    std::filesystem::remove(path);
}

TEST_CASE("Config — set() overrides values", "[config]") {
    Config cfg;
    cfg.set("key", "value");
    CHECK(cfg.get_string("key") == "value");
    CHECK(cfg.has("key"));
}

TEST_CASE("Config — bool parsing", "[config]") {
    auto path = std::filesystem::temp_directory_path() / "test_config_bool.conf";
    
    std::ofstream ofs(path);
    ofs << "a = true\n";
    ofs << "b = 1\n";
    ofs << "c = yes\n";
    ofs << "d = false\n";
    ofs << "e = 0\n";
    ofs.close();

    Config cfg;
    cfg.load(path.string());
    
    CHECK(cfg.get_bool("a") == true);
    CHECK(cfg.get_bool("b") == true);
    CHECK(cfg.get_bool("c") == true);
    CHECK(cfg.get_bool("d") == false);
    CHECK(cfg.get_bool("e") == false);

    std::filesystem::remove(path);
}

TEST_CASE("Config — non-existent file returns false", "[config]") {
    Config cfg;
    CHECK(cfg.load("/nonexistent/path/that/does/not/exist.conf") == false);
}

TEST_CASE("Config — MachinaConfig from_config", "[config]") {
    auto path = std::filesystem::temp_directory_path() / "test_machina.conf";
    
    std::ofstream ofs(path);
    ofs << "kv_cache_gb = 60.0\n";
    ofs << "max_chunk_size = 4096\n";
    ofs << "mtp_enabled = false\n";
    ofs << "cache_dir = /custom/cache\n";
    ofs.close();

    Config cfg;
    cfg.load(path.string());
    
    MachinaConfig mc = MachinaConfig::from_config(cfg);
    
    CHECK(mc.kv_cache_gb == Approx(60.0));
    CHECK(mc.max_chunk_size == 4096);
    CHECK(mc.mtp_enabled == false);
    CHECK(mc.cache_dir == "/custom/cache");
    // Defaults preserved
    CHECK(mc.ssm_conv_gb == Approx(4.0));
    CHECK(mc.nvfp4_enabled == true);

    std::filesystem::remove(path);
}

TEST_CASE("Config — lines without = are skipped", "[config]") {
    auto path = std::filesystem::temp_directory_path() / "test_skip.conf";
    
    std::ofstream ofs(path);
    ofs << "good_key = good_value\n";
    ofs << "this line has no equals sign\n";
    ofs << "\n";
    ofs << "# comment line\n";
    ofs.close();

    Config cfg;
    cfg.load(path.string());
    
    CHECK(cfg.get_string("good_key") == "good_value");
    CHECK(!cfg.has("this line has no equals sign"));

    std::filesystem::remove(path);
}

TEST_CASE("Config — empty file", "[config]") {
    auto path = std::filesystem::temp_directory_path() / "test_empty.conf";
    
    std::ofstream ofs(path);
    ofs.close();

    Config cfg;
    REQUIRE(cfg.load(path.string()));
    CHECK(!cfg.has("anything"));

    std::filesystem::remove(path);
}

TEST_CASE("Config — value with = sign preserved", "[config]") {
    auto path = std::filesystem::temp_directory_path() / "test_equals.conf";
    
    std::ofstream ofs(path);
    ofs << "formula = a=b+c\n";
    ofs.close();

    Config cfg;
    cfg.load(path.string());
    
    CHECK(cfg.get_string("formula") == "a=b+c");

    std::filesystem::remove(path);
}
