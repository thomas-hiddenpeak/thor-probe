#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace deusridet::json {

class Writer {
    std::ostringstream oss_;
    int indent_ = 0;
    bool compact_ = false;
    bool need_comma_ = false;

    void maybe_comma() {
        if (need_comma_) {
            oss_ << ",";
            if (!compact_) oss_ << "\n";
        }
        need_comma_ = true;
    }
    void write_indent() {
        if (!compact_) {
            for (int i = 0; i < indent_; ++i) oss_ << "  ";
        }
    }

    static std::string escape(std::string_view s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    }

public:
    Writer(bool compact = false) : compact_(compact) {}

    void begin_object() {
        maybe_comma();
        write_indent();
        oss_ << "{";
        if (!compact_) oss_ << "\n";
        ++indent_;
        need_comma_ = false;
    }
    void end_object() {
        --indent_;
        if (!compact_) { oss_ << "\n"; write_indent(); }
        oss_ << "}";
        need_comma_ = true;
    }

    void begin_object(std::string_view key) {
        maybe_comma();
        write_indent();
        oss_ << "\"" << escape(key) << ": {";
        if (!compact_) oss_ << "\n";
        ++indent_;
        need_comma_ = false;
    }

    void begin_array() {
        maybe_comma();
        write_indent();
        oss_ << "[";
        if (!compact_) oss_ << "\n";
        ++indent_;
        need_comma_ = false;
    }
    void end_array() {
        --indent_;
        if (!compact_) { oss_ << "\n"; write_indent(); }
        oss_ << "]";
        need_comma_ = true;
    }

    void begin_array(std::string_view key) {
        maybe_comma();
        write_indent();
        oss_ << "\"" << escape(key) << ": [";
        if (!compact_) oss_ << "\n";
        ++indent_;
        need_comma_ = false;
    }

    void field_string(std::string_view key, std::string_view val) {
        maybe_comma();
        write_indent();
        oss_ << "\"" << escape(key) << "\": \"" << escape(val) << "\"";
    }
    void field_string(std::string_view key, const char* val) {
        field_string(key, std::string_view(val ? val : ""));
    }

    template<typename T>
    void field_int(std::string_view key, T val) {
        maybe_comma();
        write_indent();
        oss_ << "\"" << escape(key) << "\": " << val;
    }

    void field_uint(std::string_view key, unsigned int val) { field_int(key, val); }
    void field_uint64(std::string_view key, uint64_t val) { field_int(key, val); }
    void field_size(std::string_view key, size_t val) { field_int(key, val); }

    void field_double(std::string_view key, double val) {
        maybe_comma();
        write_indent();
        oss_ << "\"" << escape(key) << "\": " << val;
    }

    void field_bool(std::string_view key, bool val) {
        maybe_comma();
        write_indent();
        oss_ << "\"" << escape(key) << "\": " << (val ? "true" : "false");
    }

    void field_optional_int(std::string_view key, const std::optional<int>& val) {
        if (val) field_int(key, *val);
    }
    void field_optional_double(std::string_view key, const std::optional<double>& val) {
        if (val) field_double(key, *val);
    }

    template<typename T>
    void field_optional_object(std::string_view key, const std::optional<T>& val,
                               std::function<void(Writer&, const T&)> fn) {
        if (val) {
            begin_object(key);
            fn(*this, *val);
            end_object();
        }
    }

    template<typename T>
    void field_array(std::string_view key, const std::vector<T>& vec,
                     std::function<void(Writer&, const T&)> item_fn) {
        if (!vec.empty()) {
            begin_array(key);
            for (const auto& item : vec) item_fn(*this, item);
            end_array();
        }
    }

    std::string str() const { return oss_.str(); }
    std::string finalize() && {
        if (!compact_) oss_ << "\n";
        return std::move(oss_).str();
    }
};

} // namespace deusridet::json
