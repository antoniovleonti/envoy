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
  void registerWorkerDispatcher(Dispatcher& dispatcher) override;
  void unregisterWorkerDispatcher(Dispatcher& dispatcher) override;

private:
  absl::Mutex mutex_;
  std::vector<EventLoopTrackerFactory*> factories_ ABSL_GUARDED_BY(mutex_);
  std::vector<Dispatcher*> worker_dispatchers_ ABSL_GUARDED_BY(mutex_);
};

} // namespace Event
} // namespace Envoy
