#pragma once

#include <cstdint>

namespace Envoy {
namespace Event {

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

} // namespace Event
} // namespace Envoy
