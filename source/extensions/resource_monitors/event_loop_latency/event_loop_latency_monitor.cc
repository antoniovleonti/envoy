#include "source/extensions/resource_monitors/event_loop_latency/event_loop_latency_monitor.h"

#include <chrono>

#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"

namespace Envoy {
namespace Extensions {
namespace ResourceMonitors {
namespace EventLoopLatencyMonitor {

EventLoopLatencyMonitor::EventLoopLatencyMonitor(
    const envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig&
        config,
    Server::Configuration::ResourceMonitorFactoryContext&)
    : max_latency_us_(config.max_latency().seconds() * 1000000 +
                      config.max_latency().nanos() / 1000) {
  if (max_latency_us_ == 0) {
    throw EnvoyException("event_loop_latency: max_latency must be greater than 0");
  }

  Event::LoopLatencyRegistry::instance().setSmoothingFactor(config.smoothing_factor());
  monitor_handle_ = Event::LoopLatencyRegistry::instance().registerMonitor();
}

void EventLoopLatencyMonitor::updateResourceUsage(Server::ResourceUpdateCallbacks& callbacks) {
  auto trackers = Event::LoopLatencyRegistry::instance().trackers();

  if (trackers.empty()) {
    callbacks.onSuccess({0.0});
    return;
  }

  uint64_t max_observed_latency_us = 0;
  for (const auto& tracker : trackers) {
    uint64_t tracker_avg = tracker.get().getAvgLatencyUs();
    max_observed_latency_us = std::max(max_observed_latency_us, tracker_avg);
  }

  double pressure = max_observed_latency_us / static_cast<double>(max_latency_us_);
  pressure = std::min(1.0, std::max(0.0, pressure));

  Server::ResourceUsage usage;
  usage.resource_pressure_ = pressure;

  ENVOY_LOG_MISC(
      trace, "EventLoopLatencyMonitor: max_observed_latency_us={}, max_latency_us={}, pressure={}",
      max_observed_latency_us, max_latency_us_, pressure);

  callbacks.onSuccess(usage);
}

} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
