#pragma once

#include "envoy/common/time.h"
#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.h"
#include "envoy/server/resource_monitor.h"
#include "envoy/server/resource_monitor_config.h"

#include "source/common/event/loop_latency_registry.h"

#include "absl/container/flat_hash_map.h"

namespace Envoy {
namespace Extensions {
namespace ResourceMonitors {
namespace EventLoopLatencyMonitor {

class EventLoopLatencyMonitor : public Server::ResourceMonitor {
public:
  EventLoopLatencyMonitor(
      const envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig&
          config,
      Server::Configuration::ResourceMonitorFactoryContext& context);
  ~EventLoopLatencyMonitor() override = default;

  // Server::ResourceMonitor
  void updateResourceUsage(Server::ResourceUpdateCallbacks& callbacks) override;

private:
  TimeSource& time_source_;
  std::shared_ptr<Event::LoopLatencyRegistry> registry_;
  Event::LoopLatencyRegistry::HandlePtr monitor_handle_;
  MonotonicTime last_poll_time_;
  absl::flat_hash_map<const Event::LoopLatencyReader*, uint64_t> last_combined_epoll_;
};

} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
