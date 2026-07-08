#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "envoy/singleton/instance.h"
#include "envoy/singleton/manager.h"

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Event {

class LoopLatencyReader;
class LoopLatencyTracker;

class LoopLatencyRegistry : public Singleton::Instance,
                            public std::enable_shared_from_this<LoopLatencyRegistry> {
public:
  class Handle {
  public:
    virtual ~Handle() = default;
  };
  using HandlePtr = std::unique_ptr<Handle>;

  static std::shared_ptr<LoopLatencyRegistry> singleton(Singleton::Manager* manager);

  std::unique_ptr<LoopLatencyTracker> createTracker(const std::string& dispatcher_name);

  HandlePtr registerTracker(LoopLatencyTracker& tracker);
  void unregisterTracker(LoopLatencyTracker& tracker);

  std::vector<std::reference_wrapper<const LoopLatencyReader>> trackers() const;

  HandlePtr registerMonitor();
  void unregisterMonitor();

private:
  mutable absl::Mutex mutex_;
  std::vector<std::reference_wrapper<LoopLatencyTracker>> trackers_ ABSL_GUARDED_BY(mutex_);
  size_t active_monitors_ ABSL_GUARDED_BY(mutex_){0};
};

} // namespace Event
} // namespace Envoy
