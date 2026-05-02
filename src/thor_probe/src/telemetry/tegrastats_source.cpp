#include "telemetry/tegrastats_source.h"

#include "communis/log.h"

#include <cstdio>
#include <filesystem>
#include <regex>
#include <sstream>
#include <sys/select.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {
const std::regex gr3d_regex{R"(GR3D_FREQ\s+\[([^\]]*)\])"};
const std::regex pva_regex{R"(PVA0_FREQ\s+\[[^\]]*@(\d+)(?:\s*\])?)"};
const std::regex emc_regex{R"(EMC_FREQ\s+(\d+)%@(\d+))"};
const std::regex cpu_freq_regex{R"(CPU(\d+)\s+@?(\d+))"};
const std::regex cpu_util_regex{R"(CPU(\d+)\s+(\d+)%[^\d])"};
const std::regex temp_regex{R"((\w+)@([0-9.]+)C)"};
const std::regex power_regex{R"((VDD_\w+)\s+(\d+)mW/(\d+)mW)"};
const std::regex ram_regex{R"(RAM\s+(\d+)/(\d+)MB)"};
} // anonymous namespace

namespace deusridet::telemetry {

bool TegraStatsSource::is_available() const {
    return fs::exists("/usr/bin/tegrastats");
}

TegraStatsSource::ParseResult TegraStatsSource::query_once(unsigned int timeout_ms) const {
    ParseResult result;
    if (!is_available()) {
        LOG_WARN("tegrastats", "tegrastats not found at /usr/bin/tegrastats");
        return result;
    }

    FILE* pipe = popen("tegrastats 2>/dev/null", "r");
    if (!pipe) {
        LOG_ERROR("tegrastats", "failed to launch tegrastats");
        return result;
    }

    int fd = fileno(pipe);
    if (fd < 0) {
        LOG_ERROR("tegrastats", "fileno() failed");
        pclose(pipe);
        return result;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(static_cast<int>(fd) + 1, &read_fds, nullptr, nullptr, &tv);
    if (ret <= 0) {
        LOG_WARN("tegrastats", "tegrastats timed out or no data (%d)", ret);
        pclose(pipe);
        return result;
    }

    char buf[4096];
    if (fgets(buf, sizeof(buf), pipe)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        result = parse_line(line);
        result.raw_line = line;
    }

    pclose(pipe);
    return result;
}

bool TegraStatsSource::is_thor_format(const std::string& line) const {
    return line.find("GR3D_FREQ @") != std::string::npos
        || line.find("NVENC0_FREQ") != std::string::npos
        || line.find("PVA0_FREQ") != std::string::npos;
}

TegraStatsSource::ParseResult TegraStatsSource::parse_line(const std::string& line) const {
    ParseResult result;
    result.raw_line = line;

    // Parse GR3D_FREQ @[freq0,freq1,freq2]
    {
        std::smatch m;
        if (std::regex_search(line, m, gr3d_regex)) {
            std::string array_str = m[1].str();
            std::vector<unsigned int> freqs;
            std::istringstream iss(array_str);
            std::string token;
            while (std::getline(iss, token, ',')) {
                token.erase(0, token.find_first_not_of(" \t"));
                if (!token.empty()) {
                    try { freqs.push_back(std::stoul(token)); } catch (...) {}
                }
            }
            for (int i = 0; i < 3 && i < static_cast<int>(freqs.size()); i++) {
                result.gpu_gpc_freqs[i] = freqs[i];
            }
        }
    }

    // Parse NVENC0_FREQ @<freq>, NVENC1_FREQ @<freq>
    {
        for (int i = 0; i < 2; i++) {
            std::string token = "NVENC" + std::to_string(i) + "_FREQ";
            auto freq = extract_freq(line, token);
            if (freq.has_value()) result.nvenc_freq[i] = freq.value();
        }
    }

    // Parse NVDEC0_FREQ @<freq>, NVDEC1_FREQ @<freq>
    {
        for (int i = 0; i < 2; i++) {
            std::string token = "NVDEC" + std::to_string(i) + "_FREQ";
            auto freq = extract_freq(line, token);
            if (freq.has_value()) result.nvdec_freq[i] = freq.value();
        }
    }

    // Parse NVJPG0_FREQ @<freq>
    {
        auto freq = extract_freq(line, "NVJPG0_FREQ");
        if (freq.has_value()) result.nvjpg_freq = freq.value();
    }

    // Parse VIC: "VIC off" or "VIC 0%@0"
    {
        if (line.find("VIC off") != std::string::npos) {
            result.vic_off = true;
        } else {
            auto freq = extract_freq(line, "VIC");
            if (freq.has_value()) {
                result.vic_freq = freq.value();
                result.vic_off = false;
            }
        }
    }

    // Parse OFA_FREQ @<freq>
    {
        auto freq = extract_freq(line, "OFA_FREQ");
        if (freq.has_value()) result.ofa_freq = freq.value();
    }

    // Parse PVA0_FREQ: "off" or "[0%,0%@1215]"
    {
        if (line.find("PVA0_FREQ off") != std::string::npos) {
            result.pva_off = true;
        } else {
            std::smatch m;
            if (std::regex_search(line, m, pva_regex)) {
                try {
                    result.pva_freq = std::stoul(m[1].str());
                    result.pva_off = false;
                } catch (...) {}
            }
        }
    }

    // Parse APE_FREQ @<freq>
    {
        auto freq = extract_freq(line, "APE");
        if (freq.has_value()) result.ape_freq = freq.value();
    }

    // Parse EMC_FREQ: e.g. "EMC_FREQ 0%@665" -> emc_bw_pct=0, emc_freq=665
    {
        std::smatch m;
        if (std::regex_search(line, m, emc_regex)) {
            try {
                result.emc_bw_pct = std::stoul(m[1].str());
                result.emc_freq = std::stoul(m[2].str());
            } catch (...) {}
        }
    }

    // Parse CPU frequencies: "CPU0 @1920" or "CPU0 1920MHz"
    {
        auto begin = std::sregex_iterator(line.begin(), line.end(), cpu_freq_regex);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            try {
                int cpu_id = std::stoi((*it)[1].str());
                unsigned int freq = std::stoul((*it)[2].str());
                while (static_cast<int>(result.cpu_freqs.size()) <= cpu_id) result.cpu_freqs.push_back(0);
                result.cpu_freqs[cpu_id] = freq;
            } catch (...) {}
        }
    }

    // Parse CPU utilization: "CPU0 5%"
    {
        auto begin = std::sregex_iterator(line.begin(), line.end(), cpu_util_regex);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            try {
                int cpu_id = std::stoi((*it)[1].str());
                unsigned int util = std::stoul((*it)[2].str());
                while (static_cast<int>(result.cpu_utils.size()) <= cpu_id) result.cpu_utils.push_back(0);
                result.cpu_utils[cpu_id] = util;
            } catch (...) {}
        }
    }

    // Parse temperatures: e.g. "GPU@31.0C  CPU_junction@48.0C"
    {
        auto begin = std::sregex_iterator(line.begin(), line.end(), temp_regex);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            try {
                std::string name = (*it)[1].str();
                double temp_d = std::stod((*it)[2].str());
                unsigned int temp = temp_d < 0 ? 0 : static_cast<unsigned int>(temp_d);
                result.temps.push_back({name, temp});
            } catch (...) {}
        }
    }

    // Parse power rails: e.g. "VDD_SOC 200mW/8500mW"
    {
        auto begin = std::sregex_iterator(line.begin(), line.end(), power_regex);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            try {
                std::string name = (*it)[1].str();
                unsigned int current = std::stoul((*it)[2].str());
                unsigned int max_val = std::stoul((*it)[3].str());
                result.power_rails.push_back({name, {current, max_val}});
            } catch (...) {}
        }
    }

    // Parse RAM: "RAM 2028/125772MB"
    {
        std::smatch m;
        if (std::regex_search(line, m, ram_regex)) {
            try {
                result.ram_used_mb = std::stoul(m[1].str());
                result.ram_total_mb = std::stoul(m[2].str());
            } catch (...) {}
        }
    }

    return result;
}

std::optional<unsigned int> TegraStatsSource::extract_freq(const std::string& line, const std::string& token) {
    size_t pos = line.find(token);
    while (pos != std::string::npos) {
        if (pos == 0 || line[pos - 1] == ' ') {
            break;
        }
        pos = line.find(token, pos + 1);
    }
    if (pos == std::string::npos) return std::nullopt;
    std::string remainder = line.substr(pos + token.size());
    auto at_pos = remainder.find('@');
    if (at_pos == std::string::npos) return std::nullopt;
    std::string after_at = remainder.substr(at_pos + 1);
    auto num_end = after_at.find_first_not_of("0123456789");
    if (num_end == 0) return std::nullopt;
    std::string num_str = (num_end == std::string::npos) ? after_at : after_at.substr(0, num_end);
    try {
        return static_cast<unsigned int>(std::stoul(num_str));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<unsigned int> TegraStatsSource::extract_freq_array(const std::string& line, const std::string& token, int idx, int max) {
    (void)max;
    auto pos = line.find(token);
    if (pos == std::string::npos) return std::nullopt;
    std::string remainder = line.substr(pos + token.size());
    auto bracket_pos = remainder.find('[');
    if (bracket_pos == std::string::npos) return std::nullopt;
    auto close_bracket = remainder.find(']', bracket_pos);
    if (close_bracket == std::string::npos) return std::nullopt;
    std::string array_str = remainder.substr(bracket_pos + 1, close_bracket - bracket_pos - 1);
    std::vector<unsigned int> values;
    std::istringstream iss(array_str);
    std::string token_str;
    while (std::getline(iss, token_str, ',')) {
        token_str.erase(0, token_str.find_first_not_of(" \t"));
        if (!token_str.empty()) {
            try { values.push_back(std::stoul(token_str)); } catch (...) {}
        }
    }
    if (idx < static_cast<int>(values.size())) return values[idx];
    return std::nullopt;
}

} // namespace deusridet::telemetry
