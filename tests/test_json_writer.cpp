// test_json_writer.cpp — Unit tests for deusridet::json::Writer
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "communis/json_writer.h"

using namespace deusridet::json;

static bool str_contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

TEST_CASE("JSON Writer — basic object", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_string("name", "thor");
    w.field_int("cores", 2560);
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"name\": \"thor\""));
    CHECK(str_contains(result, "\"cores\": 2560"));
    CHECK(str_contains(result, "{"));
    CHECK(str_contains(result, "}"));
}

TEST_CASE("JSON Writer — string escaping", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_string("quote", "say \"hello\"");
    w.field_string("backslash", "path\\to\\file");
    w.field_string("newline", "line1\nline2");
    w.field_string("tab", "col1\tcol2");
    w.field_string("carriage", "a\rb");
    w.field_string("bell", "\x07");
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\\\"hello\\\""));
    CHECK(str_contains(result, "path\\\\to\\\\file"));
    CHECK(str_contains(result, "\\n"));
    CHECK(str_contains(result, "\\t"));
    CHECK(str_contains(result, "\\r"));
    CHECK(str_contains(result, "\\u0007"));
}

TEST_CASE("JSON Writer — compact mode", "[json_writer]") {
    Writer w(true);
    w.begin_object();
    w.field_string("key", "val");
    w.field_int("num", 42);
    w.end_object();
    std::string result = std::move(w).finalize();
    // compact mode omits newlines/indent but still has spaces after : and ,
    CHECK(result == "{\"key\": \"val\",\"num\": 42}");
}

TEST_CASE("JSON Writer — nested objects", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.begin_object("gpu");
    w.field_int("sm_count", 20);
    w.begin_object("cache");
    w.field_int("l2_kb", 51200);
    w.end_object();
    w.end_object();
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"gpu\": {"));
    CHECK(str_contains(result, "\"sm_count\": 20"));
    CHECK(str_contains(result, "\"l2_kb\": 51200"));
}

TEST_CASE("JSON Writer — arrays", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.begin_array("cores");
    w.field_int("core", 0);
    w.field_int("core", 1);
    w.field_int("core", 2);
    w.end_array();
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"cores\": ["));
    CHECK(str_contains(result, "\"core\": 0"));
}

TEST_CASE("JSON Writer — numbers", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_int("int_val", -42);
    w.field_uint("uint_val", 100U);
    w.field_uint64("u64_val", 9999999999ULL);
    w.field_size("size_val", size_t(12345));
    w.field_double("double_val", 3.14159);
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"int_val\": -42"));
    CHECK(str_contains(result, "\"uint_val\": 100"));
    CHECK(str_contains(result, "\"u64_val\": 9999999999"));
    CHECK(str_contains(result, "\"size_val\": 12345"));
    CHECK(str_contains(result, "\"double_val\": 3.14159"));
}

TEST_CASE("JSON Writer — NaN/Inf to null", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_double("nan", std::nan(""));
    w.field_double("inf", std::numeric_limits<double>::infinity());
    w.field_double("neginf", -std::numeric_limits<double>::infinity());
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"nan\": null"));
    CHECK(str_contains(result, "\"inf\": null"));
    CHECK(str_contains(result, "\"neginf\": null"));
}

TEST_CASE("JSON Writer — bool", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_bool("enabled", true);
    w.field_bool("disabled", false);
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"enabled\": true"));
    CHECK(str_contains(result, "\"disabled\": false"));
}

TEST_CASE("JSON Writer — optional fields", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_optional_int("present", std::optional<int>(42));
    w.field_optional_int("absent", std::nullopt);
    w.field_optional_double("d_present", std::optional<double>(3.14));
    w.field_optional_double("d_absent", std::nullopt);
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"present\": 42"));
    CHECK(!str_contains(result, "\"absent\""));
    CHECK(str_contains(result, "3.14"));
    CHECK(!str_contains(result, "\"d_absent\""));
}

TEST_CASE("JSON Writer — empty array omitted", "[json_writer]") {
    Writer w;
    w.begin_object();
    std::vector<int> empty;
    w.field_array("empty", empty, std::function<void(Writer&, const int&)>(
        [](Writer& out, const int& val) { out.field_int("item", val); }
    ));
    w.end_object();
    std::string result = w.str();
    CHECK(!str_contains(result, "\"empty\""));
}

TEST_CASE("JSON Writer — null string", "[json_writer]") {
    Writer w;
    w.begin_object();
    w.field_string("null_ptr", static_cast<const char*>(nullptr));
    w.end_object();
    std::string result = w.str();
    CHECK(str_contains(result, "\"null_ptr\": \"\""));
}
