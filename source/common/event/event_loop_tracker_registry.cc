#include "source/common/event/event_loop_tracker_registry.h"

#include <algorithm>

#include "envoy/event/dispatcher.h"

namespace Envoy {
namespace Event {

void EventLoopTrackerRegistryImpl::registerTrackerFactory(EventLoopTrackerFactory& factory) {
  absl::MutexLock lock(&mutex_);
  if (std::find(factories_.begin(), factories_.end(), &factory) != factories_.end()) {
    return;
  }
  factories_.push_back(&factory);
  for (auto* dispatcher : worker_dispatchers_) {
    if (auto tracker = factory.createWorkerTracker(dispatcher->name()); tracker != nullptr) {
      dispatcher->post([disp = dispatcher, t = std::move(tracker)]() mutable {
        disp->registerEventLoopTracker(std::move(t));
      });
    }
  }
}

void EventLoopTrackerRegistryImpl::unregisterTrackerFactory(EventLoopTrackerFactory& factory) {
  absl::MutexLock lock(&mutex_);
  auto it = std::find(factories_.begin(), factories_.end(), &factory);
  if (it != factories_.end()) {
    factories_.erase(it);
  }
}

void EventLoopTrackerRegistryImpl::registerWorkerDispatcher(Dispatcher& dispatcher) {
  absl::MutexLock lock(&mutex_);
  if (std::find(worker_dispatchers_.begin(), worker_dispatchers_.end(), &dispatcher) ==
      worker_dispatchers_.end()) {
    worker_dispatchers_.push_back(&dispatcher);
    for (auto* factory : factories_) {
      if (auto tracker = factory->createWorkerTracker(dispatcher.name()); tracker != nullptr) {
        dispatcher.registerEventLoopTracker(std::move(tracker));
      }
    }
  }
}

void EventLoopTrackerRegistryImpl::unregisterWorkerDispatcher(Dispatcher& dispatcher) {
  absl::MutexLock lock(&mutex_);
  auto it = std::find(worker_dispatchers_.begin(), worker_dispatchers_.end(), &dispatcher);
  if (it != worker_dispatchers_.end()) {
    worker_dispatchers_.erase(it);
  }
}

} // namespace Event
} // namespace Envoy
