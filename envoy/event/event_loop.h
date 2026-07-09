#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "envoy/common/pure.h"

namespace Envoy {
namespace Event {

class Dispatcher;

namespace EventLoop {

class Tracker;
using TrackerPtr = std::unique_ptr<Tracker>;
class TrackerFactory;

/**
 * An interface for tracking timing and latency metrics across event loop iterations on
 * dispatchers. This hook is invoked by DispatcherImpl on every epoll check and prepare cycle.
 */
class Tracker {
public:
  virtual ~Tracker() = default;

  /**
   * Returns the factory that created this tracker.
   * Used when unregistering a factory to remove its associated trackers from dispatchers.
   * @return const TrackerFactory& the factory instance.
   */
  virtual const TrackerFactory& factory() const = 0;

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
 * Factory interface for creating EventLoop::Tracker instances for dispatchers.
 * Subsystems (such as resource monitors or profiling modules) implement this to receive event
 * loop callbacks on event loop threads.
 */
class TrackerFactory {
public:
  virtual ~TrackerFactory() = default;

  /**
   * Creates a new Tracker for a given dispatcher.
   * Called on the main thread when a dispatcher is allocated or a factory is registered.
   * @param dispatcher_name the name of the dispatcher being created.
   * @return TrackerPtr the tracker for this thread, or nullptr if disabled.
   */
  virtual TrackerPtr createTracker(const std::string& dispatcher_name) = 0;
};

/**
 * Registry for managing TrackerFactory registrations and instantiating trackers for
 * dispatchers. This is accessible via Api::Api::eventLoopRegistry().
 */
class Registry {
public:
  virtual ~Registry() = default;

  /**
   * RAII handle for a registered TrackerFactory. Destroying this handle unregisters the factory
   * and removes its associated trackers from all active dispatchers.
   */
  class TrackerFactoryHandle {
  public:
    virtual ~TrackerFactoryHandle() = default;
  };
  using TrackerFactoryHandlePtr = std::unique_ptr<TrackerFactoryHandle>;

  /**
   * RAII handle for a registered Dispatcher. Destroying this handle unregisters the dispatcher.
   */
  class DispatcherHandle {
  public:
    virtual ~DispatcherHandle() = default;
  };
  using DispatcherHandlePtr = std::unique_ptr<DispatcherHandle>;

  /**
   * Registers a factory to receive tracker creation calls.
   * @param factory the factory to register.
   * @return TrackerFactoryHandlePtr RAII handle; destroying this handle unregisters the factory.
   */
  virtual TrackerFactoryHandlePtr registerTrackerFactory(TrackerFactory& factory) = 0;

  /**
   * Registers an active dispatcher so that existing and future tracker factories
   * can create and attach trackers to its event loop.
   * @param dispatcher the dispatcher to register.
   * @return DispatcherHandlePtr RAII handle; destroying this handle unregisters the dispatcher.
   */
  virtual DispatcherHandlePtr registerDispatcher(Dispatcher& dispatcher) = 0;
};

} // namespace EventLoop
} // namespace Event
} // namespace Envoy
