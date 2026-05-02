/**
 * @file timeline_logger.cpp
 * @philosophical_role Append-only event timeline with Tempus stamps. The entity's first-person transcript of its own existence; re-readable post hoc by Somnium for memory consolidation.
 * @serves Conscientia (frame events), Auditus (ASR/VAD events), Nexus (WS events), Vigilia (wakefulness transitions).
 */
#include "communis/timeline_logger.h"
#include "communis/tempus.h"
#include "communis/json_util.h"

namespace deusridet {

void TimelineLogger::log_stats(const AudioPipelineStats& st,
                                float wlecapa_margin,
                                float change_sim, bool change_valid) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!fp_) return;

    double stream_sec = st.audio_t1_processed / 16000.0;
    uint64_t t0_ns = tempus::t1_to_t0(tempus::Domain::AUDIO,
                                       st.audio_t1_processed);

    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
        R"({"t":"stats","t0":%lu,"audio_t1":%lu,"audio_t1_in":%lu,"s":%.4f,)"
        R"("speech":%s,"energy":%.2f,"rms":%.4f,)"
        R"("silero_p":%.3f,"silero_sp":%s,)",
        (unsigned long)t0_ns,
        (unsigned long)st.audio_t1_processed,
        (unsigned long)st.audio_t1_in,
        stream_sec,
        st.is_speech ? "true" : "false", st.last_energy, st.last_rms,
        st.silero_prob, st.silero_speech ? "true" : "false");

    if (st.wlecapa_active) {
        n += snprintf(buf + n, sizeof(buf) - n,
            R"("wle_active":true,"wle_id":%d,"wle_name":"%s","wle_sim":%.3f,)"
            R"("wle_early":%s,"wle_margin":%.2f,)",
            st.wlecapa_id, st.wlecapa_name, st.wlecapa_sim,
            st.wlecapa_is_early ? "true" : "false",
            wlecapa_margin);
        if (change_valid) {
            n += snprintf(buf + n, sizeof(buf) - n,
                R"("change_sim":%.4f,)", change_sim);
        }
    }

    n += snprintf(buf + n, sizeof(buf) - n,
        R"("asr_buf":%.2f,"asr_buf_sp":%s,"asr_busy":%s,)"
        R"("asr_sil_ms":%d,"asr_eff_sil":%d)",
        st.asr_buf_sec,
        st.asr_buf_has_speech ? "true" : "false",
        st.asr_busy ? "true" : "false",
        st.asr_post_silence_ms, st.asr_effective_silence_ms);

    // Guard against truncation: if n >= sizeof(buf), snprintf already truncated
    if (n < (int)sizeof(buf) - 2) {
        buf[n++] = '}';
        buf[n++] = '\n';
        buf[n] = '\0';
    } else {
        // Overwrite last byte with closing brace to avoid truncation
        int end = sizeof(buf) - 2;
        buf[end++] = '}';
        buf[end++] = '\n';
        buf[end] = '\0';
    }

    fputs(buf, fp_);
    stats_count_++;
}

void TimelineLogger::log_asr(const char* text, float stream_start, float stream_end,
                               float latency_ms, float audio_sec,
                               const char* trigger,
                               int spk_id, const char* spk_name, float spk_sim,
                               float spk_conf, const char* spk_src) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!fp_) return;

    std::string esc = communis::json_escape(text);
    uint64_t t0_ns = tempus::now_t0_ns();

    char buf[2048];
    int n = snprintf(buf, sizeof(buf),
        R"({"t":"asr","t0":%lu,"s":%.2f,"e":%.2f,"text":"%s",)"
        R"("trigger":"%s","latency":%.1f,"audio":%.2f,)"
        R"("spk_id":%d,"spk_name":"%s","spk_sim":%.3f,"spk_conf":%.3f,"spk_src":"%s"})"
        "\n",
        (unsigned long)t0_ns,
        stream_start, stream_end, esc.c_str(),
        trigger ? trigger : "", latency_ms, audio_sec,
        spk_id, spk_name ? spk_name : "", spk_sim, spk_conf,
        spk_src ? spk_src : "");
    (void)n;

    fputs(buf, fp_);
    asr_count_++;
}

void TimelineLogger::log_vad(bool is_speech, bool segment_start, bool segment_end,
                               int frame_idx, float energy, uint64_t audio_t1) {
    if (!segment_start && !segment_end) return;

    std::lock_guard<std::mutex> lk(mu_);
    if (!fp_) return;

    uint64_t t0_ns = tempus::t1_to_t0(tempus::Domain::AUDIO, audio_t1);

    char buf[256];
    snprintf(buf, sizeof(buf),
        R"({"t":"vad","t0":%lu,"audio_t1":%lu,"event":"%s","speech":%s,"frame":%d,"energy":%.2f})"
        "\n",
        (unsigned long)t0_ns,
        (unsigned long)audio_t1,
        segment_start ? "start" : "end",
        is_speech ? "true" : "false",
        frame_idx, energy);

    fputs(buf, fp_);
    vad_count_++;
}

void TimelineLogger::log_drop(uint64_t t1_from, uint64_t t1_to,
                              const char* reason, size_t bytes) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!fp_) return;

    uint64_t t0_ns = tempus::now_t0_ns();
    uint64_t n_samples = (t1_to > t1_from) ? (t1_to - t1_from) : 0;
    double sec = n_samples / 16000.0;

    char buf[256];
    snprintf(buf, sizeof(buf),
        R"({"t":"drop","t0":%lu,"domain":"AUDIO","t1_from":%lu,"t1_to":%lu,)"
        R"("samples":%lu,"sec":%.4f,"bytes":%lu,"reason":"%s"})"
        "\n",
        (unsigned long)t0_ns,
        (unsigned long)t1_from, (unsigned long)t1_to,
        (unsigned long)n_samples, sec,
        (unsigned long)bytes,
        reason ? reason : "unknown");

    fputs(buf, fp_);
    drop_count_++;
}

}  // namespace deusridet
