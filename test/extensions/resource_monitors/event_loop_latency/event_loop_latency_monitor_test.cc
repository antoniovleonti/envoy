#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.h"
#include "envoy/server/resource_monitor.h"

#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"
#include "source/extensions/resource_monitors/event_loop_latency/event_loop_latency_monitor.h"
#include "source/server/resource_monitor_config_impl.h"

#include "test/mocks/runtime/mocks.h"
#include "test/mocks/server/options.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace ResourceMonitors {
namespace EventLoopLatencyMonitor {
namespace {

class MockedCallbacks : public Server::ResourceUpdateCallbacks {
public:
  MOCK_METHOD(void, onSuccess, (const Server::ResourceUsage&));
  MOCK_METHOD(void, onFailure, (const EnvoyException&));
};

class EventLoopLatencyMonitorTest : public testing::Test {
protected:
  EventLoopLatencyMonitorTest()
      : api_(Api::createApiForTest()), dispatcher_(api_->allocateDispatcher("test_thread")) {}

  std::unique_ptr<EventLoopLatencyMonitor> createMonitor(uint64_t max_latency_ms) {
    envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig config;
    config.mutable_max_latency()->set_nanos(max_latency_ms * 1000000);
    Server::Configuration::ResourceMonitorFactoryContextImpl context(
        *dispatcher_, options_, *api_, ProtobufMessage::getStrictValidationVisitor(), runtime_);
    return std::make_unique<EventLoopLatencyMonitor>(config, context);
  }

  std::unique_ptr<EventLoopLatencyMonitor> createMonitorWithSmoothing(uint64_t max_latency_ms,
                                                                      double lambda) {
    envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig config;
    config.mutable_max_latency()->set_nanos(max_latency_ms * 1000000);
    config.set_smoothing_factor(lambda);
    Server::Configuration::ResourceMonitorFactoryContextImpl context(
        *dispatcher_, options_, *api_, ProtobufMessage::getStrictValidationVisitor(), runtime_);
    return std::make_unique<EventLoopLatencyMonitor>(config, context);
  }

  Api::ApiPtr api_;
  Event::DispatcherPtr dispatcher_;
  Server::MockOptions options_;
  testing::NiceMock<Runtime::MockLoader> runtime_;
  MockedCallbacks cb_;
};

TEST_F(EventLoopLatencyMonitorTest, EmptyRegistry) {
  auto monitor = createMonitor(50);
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.0}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, CalculatePressureFromRollingAverage) {
  auto monitor = createMonitor(50); // 50ms max latency = 50000us

  Event::LoopLatencyTracker tracker1("worker_0");

  // First sample initialized EWMA rolling average to exactly 20ms = 20000us
  tracker1.reportPrepare(100000, true, 10000);
  tracker1.reportCheck(110000);             // Poll delay is 0
  tracker1.reportPrepare(130000, false, 0); // callback execution took 20ms. Combined = 20ms.

  // Query should report 20ms / 50ms = 0.4 pressure
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.4}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, CalculatePressureWithConfiguredSmoothing) {
  auto monitor = createMonitorWithSmoothing(50, 0.9); // 50ms max, 0.9 lambda

  Event::LoopLatencyTracker tracker1("worker_0");

  tracker1.reportPrepare(100000, true, 10000);
  tracker1.reportCheck(110000);             // Poll delay is 0
  tracker1.reportPrepare(120000, false, 0); // 10ms. First sample sets EWMA to exactly 10ms.

  // Second sample: 30ms latency.
  tracker1.reportCheck(130000);
  tracker1.reportPrepare(160000, false, 0); // 30ms.

  // New EWMA with lambda=0.9: 0.9 * 10 + 0.1 * 30 = 9 + 3 = 12ms = 12000us
  // Pressure: 12ms / 50ms = 0.24
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.24}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, MultipleTrackersMax) {
  auto monitor = createMonitor(50); // 50ms max

  Event::LoopLatencyTracker tracker1("worker_0");
  Event::LoopLatencyTracker tracker2("worker_1");

  // tracker1 has 10ms rolling avg
  tracker1.reportPrepare(100000, true, 10000);
  tracker1.reportCheck(110000);
  tracker1.reportPrepare(120000, false, 0); // 10ms

  // tracker2 has 30ms rolling avg
  tracker2.reportPrepare(100000, true, 10000);
  tracker2.reportCheck(110000);
  tracker2.reportPrepare(140000, false, 0); // 30ms

  // Query should report maximum of the two: 30ms / 50ms = 0.6 pressure
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.6}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, ClampsPressure) {
  auto monitor = createMonitor(50); // 50ms max

  Event::LoopLatencyTracker tracker1("worker_0");

  // tracker1 has 60ms rolling avg
  tracker1.reportPrepare(100000, true, 10000);
  tracker1.reportCheck(110000);
  tracker1.reportPrepare(170000, false, 0); // 60ms

  // Query should report 1.0 pressure (clamped)
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{1.0}));
  monitor->updateResourceUsage(cb_);
}

} // namespace
} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
