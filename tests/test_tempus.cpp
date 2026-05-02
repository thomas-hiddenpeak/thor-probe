// test_tempus.cpp — Unit tests for deusridet::tempus
// Tests: now_t0_ns, anchor_register/anchor_of, t1_to_t0, t0_to_t1 round-trip, TimeStamp

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>
#include "communis/tempus.h"

using namespace deusridet::tempus;

TEST_CASE("Tempus — now_t0_ns returns nonzero", "[tempus]") {
    uint64_t now = now_t0_ns();
    CHECK(now > 0);
    
    // Monotonicity check
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    uint64_t later = now_t0_ns();
    CHECK(later >= now);
}

TEST_CASE("Tempus — TimeStamp size", "[tempus]") {
    static_assert(sizeof(TimeStamp) == 32, "TimeStamp must be 32 bytes");
}

TEST_CASE("Tempus — anchor_register / anchor_of round-trip", "[tempus]") {
    // Register an anchor for AUDIO domain
    uint64_t t0_anchor = 1000000;
    uint64_t t1_zero   = 50000;
    uint64_t period_ns = 62500;  // 16kHz sample period
    
    anchor_register(Domain::AUDIO, t0_anchor, t1_zero, period_ns);
    
    ClockAnchor a = anchor_of(Domain::AUDIO);
    CHECK(a.t0_anchor_ns == t0_anchor);
    CHECK(a.t1_zero == t1_zero);
    CHECK(a.period_ns == period_ns);
}

TEST_CASE("Tempus — t1_to_t0 conversion", "[tempus]") {
    // Set up: t1=0 maps to t0=1000000, period=62500ns (16kHz)
    anchor_register(Domain::AUDIO, 1000000, 0, 62500);
    
    // t1=0 → t0=1000000
    CHECK(t1_to_t0(Domain::AUDIO, 0) == 1000000);
    
    // t1=1 → t0=1000000 + 62500 = 1062500
    CHECK(t1_to_t0(Domain::AUDIO, 1) == 1062500);
    
    // t1=100 → t0=1000000 + 62500*100 = 7250000
    CHECK(t1_to_t0(Domain::AUDIO, 100) == 7250000);
}

TEST_CASE("Tempus — t1_to_t0 with nonzero t1_zero", "[tempus]") {
    // t1_zero=500, so t1 must be >= 500 to convert
    anchor_register(Domain::VIDEO, 1000000, 500, 1000);
    
    // t1=500 → delta=0 → t0=1000000
    CHECK(t1_to_t0(Domain::VIDEO, 500) == 1000000);
    
    // t1=499 → below t1_zero → returns 0
    CHECK(t1_to_t0(Domain::VIDEO, 499) == 0);
}

TEST_CASE("Tempus — t0_to_t1 conversion", "[tempus]") {
    anchor_register(Domain::AUDIO, 1000000, 0, 62500);
    
    // t0=1000000 → t1=0
    CHECK(t0_to_t1(Domain::AUDIO, 1000000) == 0);
    
    // t0=1062500 → t1=1
    CHECK(t0_to_t1(Domain::AUDIO, 1062500) == 1);
    
    // t0=7250000 → t1=100
    CHECK(t0_to_t1(Domain::AUDIO, 7250000) == 100);
}

TEST_CASE("Tempus — round-trip t1→t0→t1", "[tempus]") {
    anchor_register(Domain::AUDIO, 1000000, 0, 62500);
    
    for (uint64_t t1 : {0, 1, 100, 1000, 10000}) {
        uint64_t t0 = t1_to_t0(Domain::AUDIO, t1);
        uint64_t back = t0_to_t1(Domain::AUDIO, t0);
        CHECK(back == t1);
    }
}

TEST_CASE("Tempus — zero period returns 0", "[tempus]") {
    // Default anchor has period_ns=0
    // Use a domain we haven't registered (TOOL = 5)
    CHECK(t1_to_t0(Domain::TOOL, 999) == 0);
    CHECK(t0_to_t1(Domain::TOOL, 999) == 0);
}

TEST_CASE("Tempus — stamp_from_t1", "[tempus]") {
    anchor_register(Domain::AUDIO, 1000000, 0, 62500);
    
    TimeStamp ts = stamp_from_t1(Domain::AUDIO, 100, 42);
    CHECK(ts.domain == Domain::AUDIO);
    CHECK(ts.t1_business == 100);
    CHECK(ts.t2_module == 42);
    CHECK(ts.t0_ns == 1000000 + 62500ULL * 100);
}

TEST_CASE("Tempus — stamp_now", "[tempus]") {
    anchor_register(Domain::VIDEO, 1000000, 0, 1000);
    
    TimeStamp ts = stamp_now(Domain::VIDEO, 7);
    CHECK(ts.domain == Domain::VIDEO);
    CHECK(ts.t2_module == 7);
    CHECK(ts.t0_ns > 0);
}

TEST_CASE("Tempus — Domain enum values", "[tempus]") {
    CHECK(static_cast<uint8_t>(Domain::AUDIO) == 0);
    CHECK(static_cast<uint8_t>(Domain::VIDEO) == 1);
    CHECK(static_cast<uint8_t>(Domain::COUNT) == 6);
}
