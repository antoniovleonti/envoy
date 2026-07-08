#include "source/extensions/resource_monitors/event_loop_latency/event_loop_latency_monitor.h"

#include <chrono>

#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"

namespace Envoy {
namespace Extensions {
namespace ResourceMonitors {
namespace EventLoopLatencyMonitor {

EventLoopLatencyMonitor::EventLoopLatencyMonitor(
    const envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig&,
    Server::Configuration::ResourceMonitorFactoryContext& context)
    : time_source_(context.api().timeSource()),
      registry_(Event::LoopLatencyRegistry::singleton(context.singletonManager())) {
  monitor_handle_ = registry_->registerMonitor();
  last_poll_time_ = time_source_.monotonicTime();
  for (const auto& tracker_ref : registry_->trackers()) {
    const auto& tracker = tracker_ref.get();
    last_combined_epoll_[&tracker] = tracker.getSumCombinedEpollUs();
  }
}

void EventLoopLatencyMonitor::updateResourceUsage(Server::ResourceUpdateCallbacks& callbacks) {
  auto current_time = time_source_.monotonicTime();
  auto delta_time_us =
      std::chrono::duration_cast<std::chrono::microseconds>(current_time - last_poll_time_).count();
  if (delta_time_us <= 0) {
    callbacks.onSuccess({0.0});
    return;
  }

  auto trackers = registry_->trackers();
  if (trackers.empty()) {
    last_poll_time_ = current_time;
    last_combined_epoll_.clear();
    callbacks.onSuccess({0.0});
    return;
  }

  double max_utilization = 0.0;
  for (const auto& tracker_ref : trackers) {
    const auto& tracker = tracker_ref.get();
    uint64_t new_combined_epoll = tracker.getSumCombinedEpollUs();
    auto it = last_combined_epoll_.find(&tracker);
    if (it != last_combined_epoll_.end()) {
      uint64_t last_epoll = it->second;
      uint64_t delta_combined_epoll =
          (new_combined_epoll > last_epoll) ? (new_combined_epoll - last_epoll) : 0;
      double new_utilization =
          static_cast<double>(delta_combined_epoll) / static_cast<double>(delta_time_us);
      max_utilization = std::max(max_utilization, new_utilization);
    }
    last_combined_epoll_[&tracker] = new_combined_epoll;
  }

  absl::flat_hash_map<const Event::LoopLatencyReader*, uint64_t> current_map;
  for (const auto& tracker_ref : trackers) {
    const auto& tracker = tracker_ref.get();
    current_map[&tracker] = last_combined_epoll_[&tracker];
  }
  last_combined_epoll_ = std::move(current_map);
  last_poll_time_ = current_time;

  max_utilization = std::min(1.0, std::max(0.0, max_utilization));

  Server::ResourceUsage usage;
  usage.resource_pressure_ = max_utilization;

  ENVOY_LOG_MISC(trace, "EventLoopLatencyMonitor: max_utilization={}, pressure={}",
                 max_utilization, usage.resource_pressure_);

  callbacks.onSuccess(usage);
}

} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
