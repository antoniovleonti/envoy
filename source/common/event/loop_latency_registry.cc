#include "source/common/event/loop_latency_registry.h"

#include <algorithm>

#include "source/common/event/loop_latency_tracker.h"

namespace Envoy {
namespace Event {

SINGLETON_MANAGER_REGISTRATION(loop_latency_registry);

namespace {

class TrackerHandle : public LoopLatencyRegistry::Handle {
public:
  TrackerHandle(LoopLatencyRegistry& registry, LoopLatencyTracker& tracker)
      : registry_(registry), tracker_(tracker) {}
  ~TrackerHandle() override { registry_.unregisterTracker(tracker_); }

private:
  LoopLatencyRegistry& registry_;
  LoopLatencyTracker& tracker_;
};

class MonitorHandle : public LoopLatencyRegistry::Handle {
public:
  explicit MonitorHandle(LoopLatencyRegistry& registry) : registry_(registry) {}
  ~MonitorHandle() override { registry_.unregisterMonitor(); }

private:
  LoopLatencyRegistry& registry_;
};

} // namespace

std::shared_ptr<LoopLatencyRegistry>
LoopLatencyRegistry::singleton(Singleton::Manager* manager) {
  if (manager == nullptr) {
    static auto fallback_instance = std::make_shared<LoopLatencyRegistry>();
    return fallback_instance;
  }
  return manager->getTyped<LoopLatencyRegistry>(
      SINGLETON_MANAGER_REGISTERED_NAME(loop_latency_registry),
      [] { return std::make_shared<LoopLatencyRegistry>(); },
      /*pin=*/true);
}

std::unique_ptr<LoopLatencyTracker>
LoopLatencyRegistry::createTracker(const std::string& dispatcher_name) {
  return std::make_unique<LoopLatencyTracker>(dispatcher_name, shared_from_this());
}

LoopLatencyRegistry::HandlePtr LoopLatencyRegistry::registerTracker(LoopLatencyTracker& tracker) {
  absl::MutexLock lock(&mutex_);
  tracker.enable(active_monitors_ > 0);
  trackers_.push_back(tracker);
  return std::make_unique<TrackerHandle>(*this, tracker);
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

LoopLatencyRegistry::HandlePtr LoopLatencyRegistry::registerMonitor() {
  absl::MutexLock lock(&mutex_);
  ++active_monitors_;
  if (active_monitors_ == 1) {
    for (LoopLatencyTracker& tracker : trackers_) {
      tracker.enable(true);
    }
  }
  return std::make_unique<MonitorHandle>(*this);
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
