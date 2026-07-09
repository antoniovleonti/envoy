#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "envoy/common/pure.h"

namespace Envoy {
namespace Event {

class Dispatcher;

/**
 * An interface for tracking timing and latency metrics across event loop iterations on worker
 * threads. This hook is invoked by DispatcherImpl on every epoll check and prepare cycle.
 */
class EventLoopTracker {
public:
  virtual ~EventLoopTracker() = default;

  /**
   * Called during epoll/event loop prepare phase (before sleeping/polling for events).
   * @param prepare_time_us monotonic time in microseconds at prepare.
   * @param timeout_set whether a timeout was set on the epoll wait.
   * @param timeout_us the duration of the timeout in microseconds if timeout_set is true.
   */
  virtual void reportPrepare(uint64_t prepare_time_us, bool timeout_set, uint64_t timeout_us) = 0;

  /**
   * Called during epoll/event loop check phase (after waking up from polling for events).
   * @param check_time_us monotonic time in microseconds at check.
   */
  virtual void reportCheck(uint64_t check_time_us) = 0;
};

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

} // namespace Event
} // namespace Envoy
