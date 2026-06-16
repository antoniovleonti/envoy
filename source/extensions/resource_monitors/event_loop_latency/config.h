#pragma once

#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.h"
#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.validate.h"
#include "envoy/server/resource_monitor_config.h"

#include "source/extensions/resource_monitors/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace ResourceMonitors {
namespace EventLoopLatencyMonitor {

class EventLoopLatencyMonitorFactory
    : public Common::FactoryBase<
          envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig> {
public:
  EventLoopLatencyMonitorFactory() : FactoryBase("envoy.resource_monitors.event_loop_latency") {}

private:
  Server::ResourceMonitorPtr createResourceMonitorFromProtoTyped(
      const envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig&
          config,
      Server::Configuration::ResourceMonitorFactoryContext& context) override;
};

} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
