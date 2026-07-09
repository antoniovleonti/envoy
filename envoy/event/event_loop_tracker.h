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
 * Factory interface for creating EventLoopTracker instances for dispatchers.
 * Subsystems (such as resource monitors or profiling modules) implement this to receive event
 * loop callbacks on event loop threads.
 */
class EventLoopTrackerFactory {
public:
  virtual ~EventLoopTrackerFactory() = default;

  /**
   * Creates a new EventLoopTracker for a given dispatcher.
   * Called on the main thread when a dispatcher is allocated or a factory is registered.
   * @param dispatcher_name the name of the dispatcher being created.
   * @return std::unique_ptr<EventLoopTracker> the tracker for this thread, or nullptr if
   * disabled.
   */
  virtual std::unique_ptr<EventLoopTracker>
  createTracker(const std::string& dispatcher_name) = 0;
};

/**
 * Registry for managing EventLoopTrackerFactory registrations and instantiating trackers for
 * dispatchers. This is accessible via Api::Api::eventLoopTrackerRegistry().
 */
class EventLoopTrackerRegistry {
public:
  virtual ~EventLoopTrackerRegistry() = default;

  /**
   * Registers a factory to receive tracker creation calls.
   * @param factory the factory to register.
   */
  virtual void registerTrackerFactory(EventLoopTrackerFactory& factory) = 0;

  /**
   * Unregisters a previously registered factory.
   * @param factory the factory to unregister.
   */
  virtual void unregisterTrackerFactory(EventLoopTrackerFactory& factory) = 0;

  /**
   * Registers an active dispatcher so that existing and future tracker factories
   * can create and attach trackers to its event loop.
   * @param dispatcher the dispatcher to register.
   */
  virtual void registerDispatcher(Dispatcher& dispatcher) = 0;

  /**
   * Unregisters a dispatcher when it is terminating.
   * @param dispatcher the dispatcher to unregister.
   */
  virtual void unregisterDispatcher(Dispatcher& dispatcher) = 0;
};

} // namespace Event
} // namespace Envoy
