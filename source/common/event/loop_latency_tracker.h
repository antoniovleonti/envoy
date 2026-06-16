#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include "source/common/event/loop_latency_registry.h"

namespace Envoy {
namespace Event {

class LoopLatencyWriter {
public:
  virtual ~LoopLatencyWriter() = default;

  virtual void reportPrepare(uint64_t prepare_time_us, bool timeout_set, uint64_t timeout_us) = 0;
  virtual void reportCheck(uint64_t check_time_us) = 0;
};

class LoopLatencyReader {
public:
  virtual ~LoopLatencyReader() = default;

  virtual const std::string& dispatcherName() const = 0;
  virtual uint64_t getAvgLatencyUs() const = 0;
};

class LoopLatencyTracker : public LoopLatencyWriter, public LoopLatencyReader {
public:
  LoopLatencyTracker(const std::string& dispatcher_name);
  ~LoopLatencyTracker() override = default;

  // LoopLatencyReader
  const std::string& dispatcherName() const override { return dispatcher_name_; }
  uint64_t getAvgLatencyUs() const override;

  // LoopLatencyWriter
  void reportPrepare(uint64_t prepare_time_us, bool timeout_set, uint64_t timeout_us) override;
  void reportCheck(uint64_t check_time_us) override;

  // Called by registry
  void setSmoothingFactor(double lambda);
  void enable(bool enabled);

private:
  const std::string dispatcher_name_;

  uint64_t prepare_time_us_{0};
  uint64_t check_time_us_{0};
  bool timeout_set_{false};
  uint64_t timeout_us_{0};
  uint64_t poll_delay_us_{0};

  std::atomic<bool> enabled_{false};
  std::atomic<double> smoothing_factor_{0.99};
  std::atomic<uint64_t> avg_latency_us_{0};

  LoopLatencyRegistry::HandlePtr registration_handle_;
};

} // namespace Event
} // namespace Envoy
