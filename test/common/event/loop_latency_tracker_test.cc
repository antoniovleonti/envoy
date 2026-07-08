#include <chrono>

#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Event {
namespace {

TEST(LoopLatencyTrackerTest, Registration) {
  auto registry = LoopLatencyRegistry::singleton(nullptr);
  size_t initial_size = registry->trackers().size();

  {
    auto tracker = registry->createTracker("test_tracker");
    EXPECT_EQ(registry->trackers().size(), initial_size + 1);
    EXPECT_EQ(&registry->trackers().back().get(), static_cast<const LoopLatencyReader*>(tracker.get()));
    EXPECT_EQ(tracker->dispatcherName(), "test_tracker");
  }

  EXPECT_EQ(registry->trackers().size(), initial_size);
}

TEST(LoopLatencyTrackerTest, DisabledByDefault) {
  auto registry = LoopLatencyRegistry::singleton(nullptr);
  auto tracker = registry->createTracker("test_tracker");

  // Disabled by default when no monitor is active
  tracker->reportPrepare(1000, true, 1000);
  tracker->reportCheck(1500);
  tracker->reportPrepare(1700, true, 1000);

  EXPECT_EQ(tracker->getSumCombinedEpollUs(), 0);
}

TEST(LoopLatencyTrackerTest, RecordCumulativeLatency) {
  auto registry = LoopLatencyRegistry::singleton(nullptr);
  auto tracker = registry->createTracker("test_tracker");
  tracker->enable(true);

  // First prepare: no completed iteration yet
  tracker->reportPrepare(1000, true, 1000);

  // Check after 500us (poll took 500us). Timeout is 1000us, so poll delay is 0.
  tracker->reportCheck(1500);

  // Second prepare after 200us (callback execution took 200us).
  // Combined latency of first iteration: poll delay (0) + callback execution (200) = 200us.
  tracker->reportPrepare(1700, true, 1000);

  EXPECT_EQ(tracker->getSumCombinedEpollUs(), 200);
  // Reading it should NOT reset it to 0
  EXPECT_EQ(tracker->getSumCombinedEpollUs(), 200);

  // Second iteration with a 1300us latency (poll delay 1000 + callback 300)
  tracker->reportCheck(3700);
  tracker->reportPrepare(4000, false, 0);

  // Cumulative sum: 200 + 1300 = 1500us
  EXPECT_EQ(tracker->getSumCombinedEpollUs(), 1500);
}

} // namespace
} // namespace Event
} // namespace Envoy
