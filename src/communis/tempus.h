/**
 * @file tempus.h
 * @philosophical_role Three-tier TimeStamp triple (wall / monotonic / logical). Tempus is NOT a utility — it is the identity of a moment across the entity's perception, computation, and memory.
 * @serves Every subsystem that records an event; all DEVLOG-grade artifacts carry a Tempus.
 */
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace deusridet {
namespace tempus {

// ----- T0: real time -------------------------------------------------------

inline uint64_t now_t0_ns() noexcept {
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock::now().time_since_epoch()).count());
}

// ----- Business domains ----------------------------------------------------
enum class Domain : uint8_t {
    AUDIO         = 0,
    VIDEO         = 1,
    CONSCIOUSNESS = 2,
    TTS           = 3,
    DREAM         = 4,
    TOOL          = 5,
    COUNT         = 6
};

// ----- Clock anchor --------------------------------------------------------
struct ClockAnchor {
    uint64_t t0_anchor_ns = 0;
    uint64_t t1_zero      = 0;
    uint64_t period_ns    = 0;
};

namespace detail {
struct AnchorSlot {
    std::atomic<uint64_t> t0_anchor_ns{0};
    std::atomic<uint64_t> t1_zero{0};
    std::atomic<uint64_t> period_ns{0};
    std::atomic<uint32_t> seq_{0};
};
inline std::array<AnchorSlot, static_cast<size_t>(Domain::COUNT)>& anchors() {
    static std::array<AnchorSlot, static_cast<size_t>(Domain::COUNT)> s;
    return s;
}
}  // namespace detail

inline void anchor_register(Domain d,
                            uint64_t t0_anchor_ns,
                            uint64_t t1_zero,
                            uint64_t period_ns) noexcept {
    auto& slot = detail::anchors()[static_cast<size_t>(d)];
    slot.t0_anchor_ns.store(t0_anchor_ns, std::memory_order_release);
    slot.t1_zero.store(t1_zero,           std::memory_order_release);
    slot.period_ns.store(period_ns,       std::memory_order_release);
    slot.seq_.fetch_add(1, std::memory_order_acq_rel);
}

inline ClockAnchor anchor_of(Domain d) noexcept {
    const auto& slot = detail::anchors()[static_cast<size_t>(d)];
    ClockAnchor a;
    uint32_t seq;
    do {
        seq = slot.seq_.load(std::memory_order_acquire);
        a.t0_anchor_ns = slot.t0_anchor_ns.load(std::memory_order_acquire);
        a.t1_zero      = slot.t1_zero.load(std::memory_order_acquire);
        a.period_ns    = slot.period_ns.load(std::memory_order_acquire);
    } while (slot.seq_.load(std::memory_order_acquire) != seq);
    return a;
}

inline uint64_t t1_to_t0(Domain d, uint64_t t1) noexcept {
    const ClockAnchor a = anchor_of(d);
    if (a.period_ns == 0) return 0;
    if (t1 < a.t1_zero) return 0;
    const uint64_t delta = t1 - a.t1_zero;
    const double delta_ns = static_cast<double>(delta) * static_cast<double>(a.period_ns);
    return a.t0_anchor_ns + static_cast<uint64_t>(delta_ns);
}

inline uint64_t t0_to_t1(Domain d, uint64_t t0_ns) noexcept {
    const ClockAnchor a = anchor_of(d);
    if (a.period_ns == 0) return 0;
    const int64_t delta_ns = static_cast<int64_t>(t0_ns) - static_cast<int64_t>(a.t0_anchor_ns);
    return a.t1_zero + static_cast<uint64_t>(delta_ns / static_cast<int64_t>(a.period_ns));
}

// ----- TimeStamp: the canonical event time triple -------------------------
struct TimeStamp {
    uint64_t t0_ns        = 0;
    uint64_t t1_business  = 0;
    uint64_t t2_module    = 0;
    Domain   domain       = Domain::AUDIO;
    uint8_t  _pad[7]      = {0, 0, 0, 0, 0, 0, 0};
};
static_assert(sizeof(TimeStamp) == 32, "TimeStamp layout changed");

inline TimeStamp stamp_from_t1(Domain d, uint64_t t1, uint64_t t2 = 0) noexcept {
    TimeStamp ts;
    ts.domain      = d;
    ts.t1_business = t1;
    ts.t2_module   = t2;
    ts.t0_ns       = t1_to_t0(d, t1);
    return ts;
}

inline TimeStamp stamp_now(Domain d, uint64_t t2 = 0) noexcept {
    TimeStamp ts;
    ts.domain      = d;
    ts.t0_ns       = now_t0_ns();
    ts.t1_business = t0_to_t1(d, ts.t0_ns);
    ts.t2_module   = t2;
    return ts;
}

}  // namespace tempus
}  // namespace deusridet
