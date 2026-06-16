#include "source/common/event/loop_latency_registry.h"

#include <algorithm>

#include "source/common/event/loop_latency_tracker.h"

namespace Envoy {
namespace Event {

namespace {

class TrackerHandle : public LoopLatencyRegistry::Handle {
public:
  explicit TrackerHandle(LoopLatencyTracker& tracker) : tracker_(tracker) {}
  ~TrackerHandle() override { LoopLatencyRegistry::instance().unregisterTracker(tracker_); }

private:
  LoopLatencyTracker& tracker_;
};

class MonitorHandle : public LoopLatencyRegistry::Handle {
public:
  MonitorHandle() = default;
  ~MonitorHandle() override { LoopLatencyRegistry::instance().unregisterMonitor(); }
};

} // namespace

LoopLatencyRegistry& LoopLatencyRegistry::instance() {
  static auto* instance = new LoopLatencyRegistry();
  return *instance;
}

LoopLatencyRegistry::HandlePtr LoopLatencyRegistry::registerTracker(LoopLatencyTracker& tracker) {
  absl::MutexLock lock(&mutex_);
  tracker.setSmoothingFactor(smoothing_factor_);
  tracker.enable(active_monitors_ > 0);
  trackers_.push_back(tracker);
  return std::make_unique<TrackerHandle>(tracker);
}

void LoopLatencyRegistry::unregisterTracker(LoopLatencyTracker& tracker) {
  absl::MutexLock lock(&mutex_);
  trackers_.erase(std::remove_if(trackers_.begin(), trackers_.end(),
                                 [&](std::reference_wrapper<LoopLatencyTracker> item) {
                                   return &item.get() == &tracker;
                                 }),
                  trackers_.end());
}

std::vector<std::reference_wrapper<const LoopLatencyReader>> LoopLatencyRegistry::trackers() const {
  absl::MutexLock lock(&mutex_);
  std::vector<std::reference_wrapper<const LoopLatencyReader>> result;
  result.reserve(trackers_.size());
  for (LoopLatencyTracker& tracker : trackers_) {
    result.push_back(tracker);
  }
  return result;
}

void LoopLatencyRegistry::setSmoothingFactor(double lambda) {
  if (lambda >= 0.0 && lambda <= 1.0) {
    absl::MutexLock lock(&mutex_);
    smoothing_factor_ = lambda;
    for (LoopLatencyTracker& tracker : trackers_) {
      tracker.setSmoothingFactor(lambda);
    }
  }
}

LoopLatencyRegistry::HandlePtr LoopLatencyRegistry::registerMonitor() {
  absl::MutexLock lock(&mutex_);
  ++active_monitors_;
  if (active_monitors_ == 1) {
    for (LoopLatencyTracker& tracker : trackers_) {
      tracker.enable(true);
    }
  }
  return std::make_unique<MonitorHandle>();
}

void LoopLatencyRegistry::unregisterMonitor() {
  absl::MutexLock lock(&mutex_);
  if (active_monitors_ > 0) {
    --active_monitors_;
    if (active_monitors_ == 0) {
      for (LoopLatencyTracker& tracker : trackers_) {
        tracker.enable(false);
      }
    }
  }
}

} // namespace Event
} // namespace Envoy
