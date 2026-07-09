#pragma once

#include <vector>

#include "envoy/event/event_loop.h"

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Event {
namespace EventLoop {

class RegistryImpl : public Registry {
public:
  TrackerFactoryHandlePtr registerTrackerFactory(TrackerFactory& factory) override;
  DispatcherHandlePtr registerDispatcher(Dispatcher& dispatcher) override;

private:
  class TrackerFactoryHandleImpl : public TrackerFactoryHandle {
  public:
    TrackerFactoryHandleImpl(RegistryImpl& registry, TrackerFactory& factory)
        : registry_(registry), factory_(factory) {}
    ~TrackerFactoryHandleImpl() override { registry_.removeTrackerFactory(factory_); }

  private:
    RegistryImpl& registry_;
    TrackerFactory& factory_;
  };

  class DispatcherHandleImpl : public DispatcherHandle {
  public:
    DispatcherHandleImpl(RegistryImpl& registry, Dispatcher& dispatcher)
        : registry_(registry), dispatcher_(dispatcher) {}
    ~DispatcherHandleImpl() override { registry_.removeDispatcher(dispatcher_); }

  private:
    RegistryImpl& registry_;
    Dispatcher& dispatcher_;
  };

  void removeTrackerFactory(TrackerFactory& factory);
  void removeDispatcher(Dispatcher& dispatcher);

  absl::Mutex mutex_;
  std::vector<TrackerFactory*> factories_ ABSL_GUARDED_BY(mutex_);
  std::vector<Dispatcher*> dispatchers_ ABSL_GUARDED_BY(mutex_);
};

} // namespace EventLoop
} // namespace Event
} // namespace Envoy
