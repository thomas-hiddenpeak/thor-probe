/**
 * @file config.cpp
 * @philosophical_role Single source of truth for runtime configuration. Config is not logic \u2014 it is the declared contract between the operator and the entity before awakening.
 * @serves All subsystems that read CLI args, environment, or config files.
 */
// config.cpp \u2014 Configuration parser implementation (DeusRidet-Thor / SM110a)

#include "config.h"
#include "log.h"
#include <fstream>
#include <algorithm>

namespace deusridet {

bool Config::load(const std::string& path) {
    path_ = path;
    std::ifstream ifs(path);
    if (!ifs) {
        LOG_ERROR("Config", "Cannot open config file: %s", path.c_str());
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(ifs, line)) {
        line_num++;
        // Strip comments
        auto hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);

        // Trim whitespace
        auto ltrim = line.find_first_not_of(" \t");
        if (ltrim == std::string::npos) continue;
        auto rtrim = line.find_last_not_of(" \t\r\n");
        line = line.substr(ltrim, rtrim - ltrim + 1);
        if (line.empty()) continue;

        // Split on first '='
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            LOG_WARN("Config", "%s:%d: no '=' found, skipping", path.c_str(), line_num);
            continue;
        }

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        auto kt = key.find_last_not_of(" \t");
        if (kt != std::string::npos) key.resize(kt + 1);
        auto ktl = key.find_first_not_of(" \t");
        if (ktl != std::string::npos) key = key.substr(ktl);
        auto vt = val.find_first_not_of(" \t");
        if (vt != std::string::npos) val = val.substr(vt);
        else val.clear();
        if (!val.empty()) {
            auto vtr = val.find_last_not_of(" \t");
            if (vtr != std::string::npos) val.resize(vtr + 1);
        }

        entries_[key] = val;
    }

    LOG_INFO("Config", "Loaded %zu entries from %s", entries_.size(), path.c_str());
    return true;
}

std::string Config::get_string(const std::string& key, const std::string& def) const {
    auto it = entries_.find(key);
    return (it != entries_.end()) ? it->second : def;
}

int Config::get_int(const std::string& key, int def) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return def;
    try { return std::stoi(it->second); }
    catch (const std::exception&) {
      LOG_WARN("Config", "Invalid int for key '%s', using default %d", key.c_str(), def);
      return def;
    }
}

double Config::get_double(const std::string& key, double def) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return def;
    try { return std::stod(it->second); }
    catch (const std::exception&) {
      LOG_WARN("Config", "Invalid double for key '%s', using default %.1f", key.c_str(), def);
      return def;
    }
}

bool Config::get_bool(const std::string& key, bool def) const {
    auto it = entries_.find(key);
    if (it == entries_.end()) return def;
    return (it->second == "true" || it->second == "1" || it->second == "yes");
}

bool Config::has(const std::string& key) const {
    return entries_.find(key) != entries_.end();
}

void Config::set(const std::string& key, const std::string& value) {
    entries_[key] = value;
}

void Config::print() const {
    fprintf(stderr, "[Config] %s (%zu entries):\n", path_.c_str(), entries_.size());
    for (const auto& [k, v] : entries_) {
        fprintf(stderr, "  %s = %s\n", k.c_str(), v.c_str());
    }
}

// ============================================================================
// MachinaConfig
// ============================================================================

MachinaConfig MachinaConfig::from_config(const Config& cfg) {
    MachinaConfig mc;
    mc.llm_model_dir    = cfg.get_string("llm_model_dir");
    mc.asr_model_dir    = cfg.get_string("asr_model_dir");
    mc.tts_model_dir    = cfg.get_string("tts_model_dir");

    mc.kv_cache_gb      = cfg.get_double("kv_cache_gb",      50.0);
    mc.ssm_conv_gb      = cfg.get_double("ssm_conv_gb",      4.0);
    mc.scratch_gb       = cfg.get_double("scratch_gb",        8.0);

    mc.max_chunk_size   = cfg.get_int("max_chunk_size",       2048);
    mc.max_ssm_slots    = cfg.get_int("max_ssm_slots",        64);

    mc.mtp_enabled      = cfg.get_bool("mtp_enabled",         true);
    mc.mtp_num_drafts   = cfg.get_int("mtp_num_drafts",       1);

    mc.cache_enabled    = cfg.get_bool("cache_enabled",        true);
    mc.cache_dir        = cfg.get_string("cache_dir",          "/tmp/deusridet-thor_cache");
    mc.cache_max_gb     = cfg.get_double("cache_max_gb",       50.0);
    mc.cache_chunk_size = cfg.get_int("cache_chunk_size",      256);

    mc.nvfp4_enabled    = cfg.get_bool("nvfp4_enabled",        true);
    mc.nvfp4_block_size = cfg.get_int("nvfp4_block_size",      16);

    return mc;
}

void MachinaConfig::print() const {
    fprintf(stderr, "[MachinaConfig]\n");
    fprintf(stderr, "  llm_model_dir  = %s\n", llm_model_dir.c_str());
    fprintf(stderr, "  asr_model_dir  = %s\n", asr_model_dir.c_str());
    fprintf(stderr, "  tts_model_dir  = %s\n", tts_model_dir.c_str());
    fprintf(stderr, "  kv_cache_gb    = %.1f\n", kv_cache_gb);
    fprintf(stderr, "  ssm_conv_gb    = %.1f\n", ssm_conv_gb);
    fprintf(stderr, "  scratch_gb     = %.1f\n", scratch_gb);
    fprintf(stderr, "  max_chunk_size = %d\n", max_chunk_size);
    fprintf(stderr, "  mtp_enabled    = %s\n", mtp_enabled ? "true" : "false");
    fprintf(stderr, "  mtp_num_drafts = %d\n", mtp_num_drafts);
    fprintf(stderr, "  cache_enabled  = %s\n", cache_enabled ? "true" : "false");
    fprintf(stderr, "  cache_dir      = %s\n", cache_dir.c_str());
    fprintf(stderr, "  cache_max_gb   = %.1f\n", cache_max_gb);
    fprintf(stderr, "  cache_chunk_size = %d\n", cache_chunk_size);
    fprintf(stderr, "  nvfp4_enabled  = %s\n", nvfp4_enabled ? "true" : "false");
    fprintf(stderr, "  nvfp4_block_size = %d\n", nvfp4_block_size);
}

// ============================================================================
// PersonaConfig
// ============================================================================

// Split comma-separated string into trimmed tokens
[[maybe_unused]]
static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < s.size()) {
        auto comma = s.find(',', start);
        if (comma == std::string::npos) comma = s.size();
        std::string tok = s.substr(start, comma - start);
        // Trim
        auto lt = tok.find_first_not_of(" \t");
        auto rt = tok.find_last_not_of(" \t");
        if (lt != std::string::npos)
            result.push_back(tok.substr(lt, rt - lt + 1));
        start = comma + 1;
    }
    return result;
}

PersonaConfig PersonaConfig::from_config(const Config& cfg) {
    PersonaConfig pc;
    pc.name = cfg.get_string("name", "Entity");
    pc.aliases = split_csv(cfg.get_string("aliases", pc.name));

    pc.speech_max_tokens   = cfg.get_int("speech_max_tokens",    80);
    pc.thinking_max_tokens = cfg.get_int("thinking_max_tokens",  256);
    pc.decode_interleave_tokens = cfg.get_int("decode_interleave_tokens", 4);

    return pc;
}

void PersonaConfig::print() const {
    fprintf(stderr, "[PersonaConfig]\n");
    fprintf(stderr, "  name              = %s\n", name.c_str());
    fprintf(stderr, "  aliases           =");
    for (const auto& a : aliases) fprintf(stderr, " [%s]", a.c_str());
    fprintf(stderr, "\n");
    fprintf(stderr, "  speech_max_tokens = %d\n", speech_max_tokens);
    fprintf(stderr, "  think_max_tokens  = %d\n", thinking_max_tokens);
    fprintf(stderr, "  interleave_tokens = %d\n", decode_interleave_tokens);
}

// ============================================================================
// SensusConfig
// ============================================================================

SensusConfig SensusConfig::from_config(const Config& cfg) {
    SensusConfig sc;
    sc.sample_rate         = cfg.get_int("sample_rate",          16000);
    sc.chunk_duration_ms   = cfg.get_int("chunk_duration_ms",    30);
    sc.asr_model_dir       = cfg.get_string("asr_model_dir");
    sc.frcrn_batch_size    = cfg.get_int("frcrn_batch_size",     1);
    sc.mossformer2_batch_size = cfg.get_int("mossformer2_batch_size", 1);

    return sc;
}

void SensusConfig::print() const {
    fprintf(stderr, "[SensusConfig]\n");
    fprintf(stderr, "  sample_rate      = %d\n", sample_rate);
    fprintf(stderr, "  chunk_duration_ms= %d\n", chunk_duration_ms);
    fprintf(stderr, "  asr_model_dir    = %s\n", asr_model_dir.c_str());
    fprintf(stderr, "  frcrn_batch_size = %d\n", frcrn_batch_size);
    fprintf(stderr, "  mossformer2_bs   = %d\n", mossformer2_batch_size);
}

// ============================================================================
// ThorConfig
// ============================================================================

ThorConfig ThorConfig::from_config(const Config& cfg) {
    ThorConfig tc;
    tc.platform        = cfg.get_string("platform",      "Thor");
    tc.cuda_arch       = cfg.get_int("cuda_arch",        1100);
    tc.cuda_arch_flag  = cfg.get_string("cuda_arch_flag", "sm_110a");
    tc.tmem_enabled    = cfg.get_bool("tmem_enabled",    true);

    return tc;
}

void ThorConfig::print() const {
    fprintf(stderr, "[ThorConfig]\n");
    fprintf(stderr, "  platform       = %s\n", platform.c_str());
    fprintf(stderr, "  cuda_arch      = %d\n", cuda_arch);
    fprintf(stderr, "  cuda_arch_flag = %s\n", cuda_arch_flag.c_str());
    fprintf(stderr, "  tmem_enabled   = %s\n", tmem_enabled ? "true" : "false");
}

} // namespace deusridet
