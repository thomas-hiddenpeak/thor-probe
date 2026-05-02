/**
 * @file timeline_logger.h
 * @philosophical_role Declaration of the Tempus-stamped append-only timeline. Enforces the invariant that no event is written without a full three-tier timestamp.
 * @serves All subsystems that emit timeline events.
 */
#pragma once

#include "tempus.h"

#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>
#include <mutex>
#include <sys/stat.h>

namespace deusridet {

struct AudioPipelineStats;

class TimelineLogger {
public:
    TimelineLogger() = default;
    ~TimelineLogger() { close(); }

    TimelineLogger(const TimelineLogger&) = delete;
    TimelineLogger& operator=(const TimelineLogger&) = delete;

    bool open(const std::string& base_dir = "logs/timeline") {
        std::lock_guard<std::mutex> lk(mu_);
        if (fp_) return false;

        mkdir(base_dir.c_str(), 0755);
        auto slash = base_dir.find('/');
        if (slash != std::string::npos) {
            mkdir(base_dir.substr(0, slash).c_str(), 0755);
            mkdir(base_dir.c_str(), 0755);
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm tm;
        localtime_r(&ts.tv_sec, &tm);

        char fname[128];
        snprintf(fname, sizeof(fname), "%s/tl_%04d%02d%02d_%02d%02d%02d.jsonl",
                 base_dir.c_str(),
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
        path_ = fname;

        fp_ = fopen(fname, "w");
        if (!fp_) return false;

        setvbuf(fp_, nullptr, _IOLBF, 0);

        char iso[64];
        snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);

        fprintf(fp_, R"({"t":"header","version":3,"started":"%s","t0":%lu})" "\n",
                iso, (unsigned long)tempus::now_t0_ns());
        clock_gettime(CLOCK_MONOTONIC, &t0_);
        stats_count_ = asr_count_ = vad_count_ = drop_count_ = 0;
        return true;
    }

    void close() {
        std::lock_guard<std::mutex> lk(mu_);
        if (!fp_) return;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double dur = (now.tv_sec - t0_.tv_sec) + (now.tv_nsec - t0_.tv_nsec) / 1e9;

        struct timespec rt;
        clock_gettime(CLOCK_REALTIME, &rt);
        struct tm tm;
        localtime_r(&rt.tv_sec, &tm);
        char iso[64];
        snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);

        fprintf(fp_,
            R"({"t":"footer","ended":"%s","t0":%lu,"duration_sec":%.1f,"counts":{"stats":%u,"asr":%u,"vad":%u,"drop":%u}})"
            "\n", iso, (unsigned long)tempus::now_t0_ns(),
            dur, stats_count_, asr_count_, vad_count_, drop_count_);
        fclose(fp_);
        fp_ = nullptr;
    }

    bool is_open() const { return fp_ != nullptr; }
    const std::string& path() const { return path_; }

    void log_stats(const AudioPipelineStats& st,
                   float wlecapa_margin,
                   float change_sim, bool change_valid);

    void log_asr(const char* text, float stream_start, float stream_end,
                 float latency_ms, float audio_sec,
                 const char* trigger,
                 int spk_id, const char* spk_name, float spk_sim,
                 float spk_conf, const char* spk_src);

    void log_vad(bool is_speech, bool segment_start, bool segment_end,
                 int frame_idx, float energy, uint64_t audio_t1);

    void log_drop(uint64_t t1_from, uint64_t t1_to,
                  const char* reason, size_t bytes);

private:
    FILE* fp_ = nullptr;
    std::string path_;
    std::mutex mu_;
    struct timespec t0_ = {};
    unsigned stats_count_ = 0;
    unsigned asr_count_ = 0;
    unsigned vad_count_ = 0;
    unsigned drop_count_ = 0;
};

}  // namespace deusridet
