#pragma once

#include <vector>

#include "envoy/event/event_loop_tracker.h"

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Event {

class Dispatcher;

class EventLoopTrackerRegistryImpl : public EventLoopTrackerRegistry {
public:
  void registerTrackerFactory(EventLoopTrackerFactory& factory) override;
  void unregisterTrackerFactory(EventLoopTrackerFactory& factory) override;
  void registerDispatcher(Dispatcher& dispatcher) override;
  void unregisterDispatcher(Dispatcher& dispatcher) override;

private:
  absl::Mutex mutex_;
  std::vector<EventLoopTrackerFactory*> factories_ ABSL_GUARDED_BY(mutex_);
  std::vector<Dispatcher*> dispatchers_ ABSL_GUARDED_BY(mutex_);
};

} // namespace Event
} // namespace Envoy
