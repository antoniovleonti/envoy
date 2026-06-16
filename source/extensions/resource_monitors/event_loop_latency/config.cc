#include "source/extensions/resource_monitors/event_loop_latency/config.h"

#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.h"
#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.validate.h"
#include "envoy/registry/registry.h"

#include "source/common/protobuf/utility.h"
#include "source/extensions/resource_monitors/event_loop_latency/event_loop_latency_monitor.h"

namespace Envoy {
namespace Extensions {
namespace ResourceMonitors {
namespace EventLoopLatencyMonitor {

Server::ResourceMonitorPtr EventLoopLatencyMonitorFactory::createResourceMonitorFromProtoTyped(
    const envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig&
        config,
    Server::Configuration::ResourceMonitorFactoryContext& context) {
  return std::make_unique<EventLoopLatencyMonitor>(config, context);
}

/**
 * Static registration for the event loop latency resource monitor factory. @see RegistryFactory.
 */
REGISTER_FACTORY(EventLoopLatencyMonitorFactory, Server::Configuration::ResourceMonitorFactory);

} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
