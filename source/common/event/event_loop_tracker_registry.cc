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
  for (auto* dispatcher : dispatchers_) {
    if (auto tracker = factory.createTracker(dispatcher->name()); tracker != nullptr) {
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
    for (auto* dispatcher : dispatchers_) {
      dispatcher->post([disp = dispatcher, &factory]() {
        disp->unregisterEventLoopTracker(factory);
      });
    }
  }
}

void EventLoopTrackerRegistryImpl::registerDispatcher(Dispatcher& dispatcher) {
  absl::MutexLock lock(&mutex_);
  if (std::find(dispatchers_.begin(), dispatchers_.end(), &dispatcher) == dispatchers_.end()) {
    dispatchers_.push_back(&dispatcher);
    for (auto* factory : factories_) {
      if (auto tracker = factory->createTracker(dispatcher.name()); tracker != nullptr) {
        dispatcher.registerEventLoopTracker(std::move(tracker));
      }
    }
  }
}

void EventLoopTrackerRegistryImpl::unregisterDispatcher(Dispatcher& dispatcher) {
  absl::MutexLock lock(&mutex_);
  auto it = std::find(dispatchers_.begin(), dispatchers_.end(), &dispatcher);
  if (it != dispatchers_.end()) {
    dispatchers_.erase(it);
  }
}

} // namespace Event
} // namespace Envoy
