/*
 * tests/test_pool_pipeline.cpp
 *
 * Minimal test suite (no external framework required — uses assert).
 * Build:
 *   g++ -std=c++11 tests/test_pool_pipeline.cpp src/FramePool.cpp \
 *       -I include $(pkg-config --cflags --libs opencv4) -lpthread -o test_runner
 *   ./test_runner
 */

#include "FramePool.h"
#include "Types.h"

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// ─── helpers ─────────────────────────────────────────────────────────────────
static void pass(const char* name) {
    std::cout << "  [PASS] " << name << "\n";
}

// ─── TEST 1: pool capacity and available count ────────────────────────────────
static void test_pool_capacity() {
    FramePool pool(4, 640, 480, CV_8UC3);
    assert(pool.capacity()  == 4);
    assert(pool.available() == 4);

    auto f1 = pool.acquire();
    assert(pool.available() == 3);
    auto f2 = pool.acquire();
    assert(pool.available() == 2);

    f1.reset();
    assert(pool.available() == 3); // buffer returned via custom deleter
    pass("pool_capacity_and_return");
}

// ─── TEST 2: tryAcquire returns nullptr when exhausted ────────────────────────
static void test_pool_exhaustion() {
    FramePool pool(2, 64, 64, CV_8UC3);
    auto f1 = pool.tryAcquire();
    auto f2 = pool.tryAcquire();
    assert(f1 != nullptr);
    assert(f2 != nullptr);

    auto f3 = pool.tryAcquire();
    assert(f3 == nullptr); // pool exhausted

    f1.reset();
    auto f4 = pool.tryAcquire();
    assert(f4 != nullptr); // now available again
    pass("pool_exhaustion");
}

// ─── TEST 3: zero-copy — mat data pointer matches pool buffer ─────────────────
static void test_zero_copy() {
    FramePool pool(1, 128, 128, CV_8UC3);
    auto frame = pool.acquire();
    assert(frame->mat != nullptr);
    assert(!frame->mat->empty());

    // Write a known pattern into the mat
    frame->mat->at<cv::Vec3b>(0, 0) = cv::Vec3b(42, 43, 44);

    // The data pointer should still be within the pool's backing storage
    // (we can't easily test the exact address without exposing internals,
    //  but we can verify the mat is correctly sized and writable)
    assert(frame->mat->cols  == 128);
    assert(frame->mat->rows  == 128);
    assert(frame->mat->type() == CV_8UC3);
    assert(frame->mat->at<cv::Vec3b>(0, 0)[0] == 42);
    pass("zero_copy_mat_writable");
}

// ─── TEST 4: std::move semantics — no extra refcount ─────────────────────────
static void test_move_semantics() {
    FramePool pool(2, 64, 64, CV_8UC3);
    auto f1 = pool.acquire();
    assert(pool.available() == 1);

    // Move into a queue (simulating InferenceEngine::enqueue)
    FramePtr moved = std::move(f1);
    assert(f1 == nullptr);           // f1 is now empty
    assert(moved != nullptr);        // moved holds the frame
    assert(pool.available() == 1);   // buffer still checked out

    moved.reset();
    assert(pool.available() == 2);   // buffer returned after move-out owner releases
    pass("move_semantics");
}

// ─── TEST 5: concurrent acquire / release from multiple threads ───────────────
static void test_concurrent_access() {
    constexpr int POOL_SIZE   = 4;
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_EACH    = 50;

    FramePool pool(POOL_SIZE, 128, 128, CV_8UC3);
    std::atomic<int> errorCount{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_EACH; ++i) {
            FramePtr f = pool.acquire();
            if (!f || !f->mat) { ++errorCount; continue; }

            // Simulate a short work burst
            std::this_thread::sleep_for(std::chrono::microseconds(100));

            // Write to the buffer (tests there's no data race on pool memory)
            f->mat->at<cv::Vec3b>(0, 0) = cv::Vec3b(1, 2, 3);

            // Frame released at end of scope → buffer returns to pool
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t)
        threads.emplace_back(worker);
    for (auto& t : threads)
        t.join();

    assert(errorCount.load() == 0);
    assert(pool.available() == POOL_SIZE); // all buffers returned
    pass("concurrent_acquire_release");
}

// ─── TEST 6: frameId is monotonically increasing ──────────────────────────────
static void test_frame_ids() {
    FramePool pool(3, 64, 64, CV_8UC3);
    auto f1 = pool.acquire();
    auto f2 = pool.acquire();
    auto f3 = pool.acquire();
    assert(f2->frameId > f1->frameId);
    assert(f3->frameId > f2->frameId);
    pass("frame_ids_monotonic");
}

// ─── main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "=== FramePool Tests ===\n";
    test_pool_capacity();
    test_pool_exhaustion();
    test_zero_copy();
    test_move_semantics();
    test_concurrent_access();
    test_frame_ids();
    std::cout << "All tests passed.\n";
    return 0;
}
