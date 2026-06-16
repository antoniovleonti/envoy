#include "source/common/event/loop_latency_tracker.h"

#include <algorithm>

#include "source/common/event/loop_latency_registry.h"

namespace Envoy {
namespace Event {

LoopLatencyTracker::LoopLatencyTracker(const std::string& dispatcher_name)
    : dispatcher_name_(dispatcher_name) {
  registration_handle_ = LoopLatencyRegistry::instance().registerTracker(*this);
}

void LoopLatencyTracker::reportPrepare(uint64_t prepare_time_us, bool timeout_set,
                                       uint64_t timeout_us) {
  if (!enabled_.load(std::memory_order_relaxed)) {
    return;
  }

  if (check_time_us_ != 0) {
    uint64_t duration = (prepare_time_us > check_time_us_) ? (prepare_time_us - check_time_us_) : 0;
    uint64_t combined = duration + poll_delay_us_;

    double lambda = smoothing_factor_.load(std::memory_order_relaxed);
    uint64_t current_avg = avg_latency_us_.load(std::memory_order_relaxed);
    if (current_avg == 0) {
      avg_latency_us_.store(combined, std::memory_order_relaxed);
    } else {
      double updated = lambda * current_avg + (1.0 - lambda) * combined;
      avg_latency_us_.store(static_cast<uint64_t>(updated), std::memory_order_relaxed);
    }
  }

  timeout_set_ = timeout_set;
  timeout_us_ = timeout_us;
  prepare_time_us_ = prepare_time_us;
}

void LoopLatencyTracker::reportCheck(uint64_t check_time_us) {
  if (!enabled_.load(std::memory_order_relaxed)) {
    return;
  }

  uint64_t delay = 0;
  if (timeout_set_ && prepare_time_us_ != 0) {
    uint64_t poll_time =
        (check_time_us > prepare_time_us_) ? (check_time_us - prepare_time_us_) : 0;
    delay = (poll_time > timeout_us_) ? (poll_time - timeout_us_) : 0;
  }

  poll_delay_us_ = delay;
  check_time_us_ = check_time_us;
}

uint64_t LoopLatencyTracker::getAvgLatencyUs() const {
  return avg_latency_us_.load(std::memory_order_relaxed);
}

void LoopLatencyTracker::setSmoothingFactor(double lambda) {
  if (lambda >= 0.0 && lambda <= 1.0) {
    smoothing_factor_.store(lambda, std::memory_order_relaxed);
  }
}

void LoopLatencyTracker::enable(bool enabled) {
  enabled_.store(enabled, std::memory_order_relaxed);
  if (!enabled) {
    avg_latency_us_.store(0, std::memory_order_relaxed);
    prepare_time_us_ = 0;
    check_time_us_ = 0;
  }
}

} // namespace Event
} // namespace Envoy
