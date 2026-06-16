#include <chrono>

#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Event {
namespace {

TEST(LoopLatencyTrackerTest, Registration) {
  LoopLatencyRegistry& registry = LoopLatencyRegistry::instance();
  size_t initial_size = registry.trackers().size();

  {
    LoopLatencyTracker tracker("test_tracker");
    EXPECT_EQ(registry.trackers().size(), initial_size + 1);
    EXPECT_EQ(&registry.trackers().back().get(), static_cast<const LoopLatencyReader*>(&tracker));
    EXPECT_EQ(tracker.dispatcherName(), "test_tracker");
  }

  EXPECT_EQ(registry.trackers().size(), initial_size);
}

TEST(LoopLatencyTrackerTest, DisabledByDefault) {
  LoopLatencyTracker tracker("test_tracker");

  // Disabled by default when no monitor is active
  tracker.reportPrepare(1000, true, 1000);
  tracker.reportCheck(1500);
  tracker.reportPrepare(1700, true, 1000);

  EXPECT_EQ(tracker.getAvgLatencyUs(), 0);
}

TEST(LoopLatencyTrackerTest, RecordRollingAverage) {
  LoopLatencyTracker tracker("test_tracker");
  tracker.enable(true);

  // First prepare: no completed iteration yet
  tracker.reportPrepare(1000, true, 1000);

  // Check after 500us (poll took 500us). Timeout is 1000us, so poll delay is 0.
  tracker.reportCheck(1500);

  // Second prepare after 200us (callback execution took 200us).
  // Combined latency of first iteration: poll delay (0) + callback execution (200) = 200us.
  // First sample sets rolling average to exactly 200us.
  tracker.reportPrepare(1700, true, 1000);

  EXPECT_EQ(tracker.getAvgLatencyUs(), 200);
  // Reading it should NOT reset it to 0
  EXPECT_EQ(tracker.getAvgLatencyUs(), 200);

  // Second iteration with a 1300us latency (poll delay 1000 + callback 300)
  tracker.reportCheck(3700);
  tracker.reportPrepare(4000, false, 0);

  // New EWMA formula: 0.99 * 200 + 0.01 * 1300 = 198 + 13 = 211us
  EXPECT_EQ(tracker.getAvgLatencyUs(), 211);
}

TEST(LoopLatencyTrackerTest, ConfigurableSmoothingFactor) {
  LoopLatencyRegistry::instance().setSmoothingFactor(0.9);
  LoopLatencyTracker tracker("test_tracker");
  tracker.enable(true);

  tracker.reportPrepare(1000, true, 1000);
  tracker.reportCheck(1500);
  tracker.reportPrepare(1700, true, 1000);
  EXPECT_EQ(tracker.getAvgLatencyUs(), 200);

  tracker.reportCheck(3700);
  tracker.reportPrepare(4000, false, 0);
  // New formula with lambda=0.9: 0.9 * 200 + 0.1 * 1300 = 180 + 130 = 310us
  EXPECT_EQ(tracker.getAvgLatencyUs(), 310);

  LoopLatencyRegistry::instance().setSmoothingFactor(0.99);
}

} // namespace
} // namespace Event
} // namespace Envoy
