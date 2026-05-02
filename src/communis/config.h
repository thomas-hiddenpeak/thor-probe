/**
 * @file config.h
 * @philosophical_role Declarative shape of every runtime knob the operator may turn. A header that forbids implicit defaults from spreading into subsystem code.
 * @serves All subsystems that include runtime configuration.
 */
// config.h \u2014 Unified configuration parser (DeusRidet-Thor / SM110a)
//
// Parses key=value .conf files. Supports comments (#), whitespace trimming.
// Each DeusRidet-Thor config file (machina.conf, conscientia.conf, persona.conf,
// thor.conf, sensus.conf) is loaded into a flat string map; typed accessors
// provide defaults.

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace deusridet {

class Config {
public:
    Config() = default;

    // Load from .conf file (key=value format, # comments)
    bool load(const std::string& path);

    // Typed accessors with defaults
    std::string get_string(const std::string& key, const std::string& def = "") const;
    int         get_int(const std::string& key, int def = 0) const;
    double      get_double(const std::string& key, double def = 0.0) const;
    bool        get_bool(const std::string& key, bool def = false) const;

    // Check existence
    bool has(const std::string& key) const;

    // Set a value (for CLI override)
    void set(const std::string& key, const std::string& value);

    // Print all entries (for debug)
    void print() const;

private:
    std::unordered_map<std::string, std::string> entries_;
    std::string path_;
};

// Structured config for the machina inference engine (Thor-tuned defaults)
struct MachinaConfig {
    std::string llm_model_dir;
    std::string asr_model_dir;
    std::string tts_model_dir;

    // Memory allocation defaults for Thor (SM110a, 122GB system RAM)
    double kv_cache_gb      = 50.0;  // increased from 14.0 for Thor's large memory
    double ssm_conv_gb      = 4.0;   // increased from 2.0 for wider convolution state
    double scratch_gb       = 8.0;   // increased from 4.0 for larger intermediate buffers

    int    max_chunk_size    = 2048;
    int    max_ssm_slots     = 64;

    bool   mtp_enabled       = true;
    int    mtp_num_drafts    = 1;

    bool   cache_enabled     = true;
    std::string cache_dir    = "/tmp/deusridet-thor_cache";
    double cache_max_gb      = 50.0;  // increased from 20.0 for Thor capacity
    int    cache_chunk_size   = 256;

    // nvFP4 (NVIDIA FP4 quantization) support
    bool   nvfp4_enabled     = true;
    int    nvfp4_block_size  = 16;    // per-block scaling granularity

    // Construct from Config
    static MachinaConfig from_config(const Config& cfg);

    void print() const;
};

// Structured config for persona and response behavior
struct PersonaConfig {
    std::string name;                    // entity name (e.g. "\u9ed1\u746a")
    std::vector<std::string> aliases;    // name variants / wake words

    int  speech_max_tokens   = 80;       // short, concise speech output
    int  thinking_max_tokens = 256;      // internal analysis budget
    int  decode_interleave_tokens = 4;   // check input every N decode tokens

    static PersonaConfig from_config(const Config& cfg);
    void print() const;
};

// Structured config for Sensus (audio/speech perception pipeline)
struct SensusConfig {
    int       sample_rate          = 16000;
    int       chunk_duration_ms    = 30;
    std::string asr_model_dir;
    int       frcrn_batch_size     = 1;        // FRCRN noise suppression batch
    int       mossformer2_batch_size = 1;      // MossFormer2 enhancement batch

    static SensusConfig from_config(const Config& cfg);
    void print() const;
};

// Structured config for Thor platform specifics
struct ThorConfig {
    std::string platform         = "Thor";
    int         cuda_arch        = 1100;
    std::string cuda_arch_flag   = "sm_110a";
    bool        tmem_enabled     = true;       // Thor Tensort Memory (tmem) feature

    static ThorConfig from_config(const Config& cfg);
    void print() const;
};

} // namespace deusridet
