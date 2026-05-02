#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace deusridet::telemetry {

class TegraStatsSource {
public:
    TegraStatsSource() = default;
    ~TegraStatsSource() = default;

    bool is_available() const;

    struct ParseResult {
        std::optional<unsigned int> gpu_gpc_freqs[3] = {};
        std::optional<unsigned int> nvenc_freq[2] = {};
        std::optional<unsigned int> nvdec_freq[2] = {};
        std::optional<unsigned int> nvjpg_freq;
        std::optional<unsigned int> vic_freq;
        bool vic_off = true;
        std::optional<unsigned int> ofa_freq;
        std::optional<unsigned int> pva_freq;
        bool pva_off = true;
        std::optional<unsigned int> ape_freq;
        std::optional<unsigned int> emc_freq;
        std::optional<unsigned int> emc_bw_pct;
        std::vector<unsigned int> cpu_freqs;
        std::vector<unsigned int> cpu_utils;
        std::vector<std::pair<std::string, unsigned int>> temps;
        std::vector<std::pair<std::string, std::pair<unsigned int, unsigned int>>> power_rails;
        std::optional<unsigned int> ram_used_mb;
        std::optional<unsigned int> ram_total_mb;
        std::string raw_line;
    };

    // Single snapshot: popen tegrastats, read one line, timeout 5s
    ParseResult query_once(unsigned int timeout_ms = 5000) const;

private:
    ParseResult parse_line(const std::string& line) const;
    bool is_thor_format(const std::string& line) const;

    static std::optional<unsigned int> extract_freq(const std::string& line, const std::string& token);
    static std::optional<unsigned int> extract_freq_array(const std::string& line, const std::string& token, int idx, int max = 3);
};

} // namespace deusridet::telemetry
