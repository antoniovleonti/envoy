#pragma once

#include <memory>

#include "envoy/common/optref.h"
#include "envoy/common/pure.h"

#include "source/common/common/assert.h"

#include "absl/status/status.h"

namespace Envoy {
namespace Server {

// Struct for reporting usage for a particular resource.
struct ResourceUsage {
  bool operator==(const ResourceUsage& rhs) const {
    return resource_pressure_ == rhs.resource_pressure_;
  }

  // Fraction of (resource usage)/(resource limit).
  double resource_pressure_;
};

/**
 * Notifies caller of updated resource usage.
 */
class ResourceUpdateCallbacks {
public:
  virtual ~ResourceUpdateCallbacks() = default;

  /**
   * Called when the request for updated resource usage succeeds.
   * @param usage the updated resource usage
   */
  virtual void onSuccess(const ResourceUsage& usage) PURE;

  /**
   * Called when the request for updated resource usage fails.
   * @param error the status describing the failure
   */
  virtual void onFailure(const absl::Status& error) PURE;
};

class ResourceMonitor {
public:
  virtual ~ResourceMonitor() = default;

  /**
   * Recalculate resource usage.
   * This must be non-blocking so if RPCs need to be made they should be
   * done asynchronously and invoke the callback when finished.
   */
  virtual void updateResourceUsage(ResourceUpdateCallbacks& callbacks) PURE;
};

using ResourceMonitorPtr = std::unique_ptr<ResourceMonitor>;

/**
 * Interface for resource monitors that can be queried synchronously on the request hot path
 * (e.g. during LoadShedPoint::shouldShedLoad()) in addition to periodic monitoring.
 * Implementations must be thread-safe and low-overhead as methods are invoked concurrently
 * across worker threads.
 */
class RealtimeResourceMonitor : public ResourceMonitor {
public:
  ~RealtimeResourceMonitor() override = default;

  /**
   * Synchronously returns the current resource usage.
   * Implementations may return thread-local usage specific to the calling worker thread.
   */
  virtual ResourceUsage getResourceUsage() = 0;

  /**
   * Default implementation of ResourceMonitor::updateResourceUsage so that
   * RealtimeResourceMonitors automatically participate in periodic stats
   * reporting (overload.<name>.pressure) and OverloadActions on the main thread.
   */
  void updateResourceUsage(ResourceUpdateCallbacks& callbacks) override {
    callbacks.onSuccess(getResourceUsage());
  }

  /**
   * Optional callback invoked synchronously when a load shed check passes at the given
   * LoadShedPoint and the load is accepted.
   * Pure observational monitors can leave this as a no-op. Stateful budget or token-bucket
   * monitors can override this to account for admitted load.
   */
  virtual void onLoadAccepted(absl::string_view load_shed_point_name) {
    UNREFERENCED_PARAMETER(load_shed_point_name);
  }
};

using RealtimeResourceMonitorPtr = std::unique_ptr<RealtimeResourceMonitor>;
using RealtimeResourceMonitorOptRef = OptRef<RealtimeResourceMonitor>;

} // namespace Server
} // namespace Envoy
