// test_ring_buffer.cpp — Unit tests for deusridet::RingBuffer
// Tests: push/pop, capacity rounding, overflow, peek, reset, zero capacity

#include <catch2/catch_test_macros.hpp>
#include <vector>
#include "communis/ring_buffer.h"

using namespace deusridet;

static std::vector<uint8_t> make_data(size_t len) {
    std::vector<uint8_t> data(len);
    for (size_t i = 0; i < len; ++i) data[i] = static_cast<uint8_t>(i % 256);
    return data;
}

TEST_CASE("RingBuffer — basic push/pop", "[ring_buffer]") {
    RingBuffer rb(64);
    auto data = make_data(32);
    
    size_t pushed = rb.push(data.data(), data.size());
    CHECK(pushed == 32);
    CHECK(rb.available() == 32);
    CHECK(rb.free_space() == 32);

    std::vector<uint8_t> out(32);
    size_t popped = rb.pop(out.data(), out.size());
    CHECK(popped == 32);
    CHECK(rb.available() == 0);
    CHECK(out == data);
}

TEST_CASE("RingBuffer — capacity rounds up to power of 2", "[ring_buffer]") {
    RingBuffer rb(50);  // should round to 64
    CHECK(rb.capacity() == 64);
    
    RingBuffer rb2(64); // already power of 2
    CHECK(rb2.capacity() == 64);
    
    RingBuffer rb3(65); // rounds to 128
    CHECK(rb3.capacity() == 128);
}

TEST_CASE("RingBuffer — zero capacity throws", "[ring_buffer]") {
    CHECK_THROWS_AS(RingBuffer(0), std::invalid_argument);
}

TEST_CASE("RingBuffer — push to full returns partial", "[ring_buffer]") {
    RingBuffer rb(64);
    auto data = make_data(100);
    
    size_t pushed = rb.push(data.data(), data.size());
    CHECK(pushed == 64);
    CHECK(rb.available() == 64);
    CHECK(rb.free_space() == 0);
}

TEST_CASE("RingBuffer — pop from empty returns 0", "[ring_buffer]") {
    RingBuffer rb(64);
    std::vector<uint8_t> out(32);
    size_t popped = rb.pop(out.data(), out.size());
    CHECK(popped == 0);
}

TEST_CASE("RingBuffer — peek does not consume", "[ring_buffer]") {
    RingBuffer rb(64);
    auto data = make_data(32);
    rb.push(data.data(), data.size());
    
    std::vector<uint8_t> peek_out(32);
    size_t peeked = rb.peek(peek_out.data(), peek_out.size());
    CHECK(peeked == 32);
    CHECK(peek_out == data);
    CHECK(rb.available() == 32);  // still available
    
    // Now pop and verify same data
    std::vector<uint8_t> pop_out(32);
    rb.pop(pop_out.data(), pop_out.size());
    CHECK(pop_out == data);
}

TEST_CASE("RingBuffer — wrap-around (data crosses buffer boundary)", "[ring_buffer]") {
    RingBuffer rb(64);
    auto data1 = make_data(48);
    rb.push(data1.data(), data1.size());
    
    // Pop 32, leaving 16 at the end of buffer
    std::vector<uint8_t> tmp(32);
    rb.pop(tmp.data(), tmp.size());
    CHECK(rb.available() == 16);
    
    // Push 48 more — wraps around
    auto data2 = make_data(48);
    size_t pushed = rb.push(data2.data(), data2.size());
    // Available space: 64 - 16 = 48
    CHECK(pushed == 48);
    
    // Pop all 64
    std::vector<uint8_t> out(64);
    size_t popped = rb.pop(out.data(), out.size());
    CHECK(popped == 64);
    // First 16 bytes are from data1, next 48 from data2
    std::vector<uint8_t> expected;
    expected.insert(expected.end(), data1.begin() + 32, data1.end());
    expected.insert(expected.end(), data2.begin(), data2.end());
    CHECK(out == expected);
}

TEST_CASE("RingBuffer — reset clears data", "[ring_buffer]") {
    RingBuffer rb(64);
    auto data = make_data(32);
    rb.push(data.data(), data.size());
    CHECK(rb.available() == 32);
    
    rb.reset();
    CHECK(rb.available() == 0);
    CHECK(rb.free_space() == 64);
}

TEST_CASE("RingBuffer — event_fd is valid", "[ring_buffer]") {
    RingBuffer rb(64);
    CHECK(rb.event_fd() >= 0);
}

TEST_CASE("RingBuffer — multiple push/pop cycles", "[ring_buffer]") {
    RingBuffer rb(256);
    for (int round = 0; round < 10; ++round) {
        auto data = make_data(100);
        size_t pushed = rb.push(data.data(), data.size());
        CHECK(pushed == 100);
        
        std::vector<uint8_t> out(100);
        size_t popped = rb.pop(out.data(), out.size());
        CHECK(popped == 100);
        CHECK(out == data);
    }
    CHECK(rb.available() == 0);
}
