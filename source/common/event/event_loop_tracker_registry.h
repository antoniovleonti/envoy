#pragma once

#include <memory>
#include <string>
#include <vector>

#include "envoy/common/pure.h"

#include "source/common/event/event_loop_tracker.h"

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace Envoy {
namespace Event {

class Dispatcher;

/**
 * Factory interface for creating worker thread EventLoopTracker instances.
 * Subsystems (such as resource monitors or profiling modules) implement this to receive event
 * loop callbacks on worker threads.
 */
class EventLoopTrackerFactory {
public:
  virtual ~EventLoopTrackerFactory() = default;

  /**
   * Creates a new EventLoopTracker for a given worker dispatcher.
   * Called on the main thread when a worker dispatcher is allocated.
   * @param dispatcher_name the name of the worker dispatcher being created.
   * @return std::unique_ptr<EventLoopTracker> the tracker for this worker thread, or nullptr if
   * disabled.
   */
  virtual std::unique_ptr<EventLoopTracker>
  createWorkerTracker(const std::string& dispatcher_name) = 0;
};

/**
 * Registry for managing EventLoopTrackerFactory registrations and instantiating trackers for worker
 * threads. This is accessible via Api::Api::eventLoopTrackerRegistry().
 */
class EventLoopTrackerRegistry {
public:
  virtual ~EventLoopTrackerRegistry() = default;

  /**
   * Registers a factory to receive worker tracker creation calls.
   * @param factory the factory to register.
   */
  virtual void registerTrackerFactory(EventLoopTrackerFactory& factory) = 0;

  /**
   * Unregisters a previously registered factory.
   * @param factory the factory to unregister.
   */
  virtual void unregisterTrackerFactory(EventLoopTrackerFactory& factory) = 0;

  /**
   * Registers an active worker dispatcher so that existing and future tracker factories
   * can create and attach trackers to its event loop.
   * @param dispatcher the worker dispatcher to register.
   */
  virtual void registerWorkerDispatcher(Dispatcher& dispatcher) = 0;

  /**
   * Unregisters a worker dispatcher when it is terminating.
   * @param dispatcher the worker dispatcher to unregister.
   */
  virtual void unregisterWorkerDispatcher(Dispatcher& dispatcher) = 0;
};

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
