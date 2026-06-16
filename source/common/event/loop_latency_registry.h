#pragma once

#include <functional>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Event {

class LoopLatencyReader;
class LoopLatencyTracker;

class LoopLatencyRegistry {
public:
  class Handle {
  public:
    virtual ~Handle() = default;
  };
  using HandlePtr = std::unique_ptr<Handle>;

  static LoopLatencyRegistry& instance();

  HandlePtr registerTracker(LoopLatencyTracker& tracker);
  void unregisterTracker(LoopLatencyTracker& tracker);

  std::vector<std::reference_wrapper<const LoopLatencyReader>> trackers() const;
  void setSmoothingFactor(double lambda);

  HandlePtr registerMonitor();
  void unregisterMonitor();

private:
  LoopLatencyRegistry() = default;

  mutable absl::Mutex mutex_;
  std::vector<std::reference_wrapper<LoopLatencyTracker>> trackers_ ABSL_GUARDED_BY(mutex_);
  double smoothing_factor_ ABSL_GUARDED_BY(mutex_){0.99};
  size_t active_monitors_ ABSL_GUARDED_BY(mutex_){0};
};

} // namespace Event
} // namespace Envoy
