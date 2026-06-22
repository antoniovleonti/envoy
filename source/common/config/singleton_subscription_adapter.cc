#include "source/common/config/singleton_subscription_adapter.h"

#include <utility>

namespace Envoy {
namespace Config {

SingletonSubscriptionCallbacksAdapter::SingletonSubscriptionCallbacksAdapter(
    SingletonSubscriptionCallbacks& callbacks)
    : callbacks_(callbacks) {}

absl::Status SingletonSubscriptionCallbacksAdapter::onConfigUpdate(
    const std::vector<DecodedResourceRef>& resources, const std::string& version_info) {
  if (resources.empty()) {
    callbacks_.onResourceRemoved();
    return absl::OkStatus();
  }
  return callbacks_.onResourceUpdate(resources[0].get(), version_info);
}

absl::Status SingletonSubscriptionCallbacksAdapter::onConfigUpdate(
    const std::vector<DecodedResourceRef>& added_resources,
    const Protobuf::RepeatedPtrField<std::string>& removed_resources,
    const std::string& /*system_version_info*/) {
  if (!removed_resources.empty()) {
    callbacks_.onResourceRemoved();
    return absl::OkStatus();
  }
  RELEASE_ASSERT(!added_resources.empty(), "Delta xDS update with no additions or removals");
  return callbacks_.onResourceUpdate(added_resources[0].get(), added_resources[0].get().version());
}

void SingletonSubscriptionCallbacksAdapter::onConfigUpdateFailed(ConfigUpdateFailureReason reason,
                                                                 const EnvoyException* e) {
  callbacks_.onFailure(reason, e);
}

SingletonSubscriptionImpl::SingletonSubscriptionImpl(
    SubscriptionPtr sub, absl::string_view resource_name,
    std::unique_ptr<SingletonSubscriptionCallbacksAdapter> adapter)
    : adapter_(std::move(adapter)), sub_(std::move(sub)), resource_name_(resource_name) {}

void SingletonSubscriptionImpl::start() {
  sub_->start({resource_name_});
}

} // namespace Config
} // namespace Envoy
