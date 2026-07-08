#include "envoy/extensions/resource_monitors/event_loop_latency/v3/event_loop_latency.pb.h"
#include "envoy/server/resource_monitor.h"

#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"
#include "source/extensions/resource_monitors/event_loop_latency/event_loop_latency_monitor.h"
#include "source/server/resource_monitor_config_impl.h"

#include "test/mocks/runtime/mocks.h"
#include "test/mocks/server/options.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/simulated_time_system.h"
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
      : api_(Api::createApiForTest(time_system_)), dispatcher_(api_->allocateDispatcher("test_thread")) {}

  std::unique_ptr<EventLoopLatencyMonitor> createMonitor() {
    envoy::extensions::resource_monitors::event_loop_latency::v3::EventLoopLatencyConfig config;
    Server::Configuration::ResourceMonitorFactoryContextImpl context(
        *dispatcher_, options_, *api_, ProtobufMessage::getStrictValidationVisitor(), runtime_);
    return std::make_unique<EventLoopLatencyMonitor>(config, context);
  }

  Event::SimulatedTimeSystem time_system_;
  Api::ApiPtr api_;
  Event::DispatcherPtr dispatcher_;
  Server::MockOptions options_;
  testing::NiceMock<Runtime::MockLoader> runtime_;
  MockedCallbacks cb_;
};

TEST_F(EventLoopLatencyMonitorTest, EmptyRegistry) {
  auto monitor = createMonitor();
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.0}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, CalculateUtilization) {
  auto registry = Event::LoopLatencyRegistry::singleton(nullptr);
  auto tracker1 = registry->createTracker("worker_0");

  auto monitor = createMonitor();

  // Advance time by 100ms (100000us)
  time_system_.advanceTimeWait(std::chrono::milliseconds(100));

  // During this 100ms window, tracker1 accumulated 20ms (20000us) of combined epoll latency
  tracker1->reportPrepare(100000, true, 10000);
  tracker1->reportCheck(110000);             // Poll delay is 0
  tracker1->reportPrepare(130000, false, 0); // callback execution took 20ms. Combined = 20ms.

  // Query should report 20ms / 100ms = 0.2 pressure
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.2}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, MultipleTrackersMax) {
  auto registry = Event::LoopLatencyRegistry::singleton(nullptr);
  auto tracker1 = registry->createTracker("worker_0");
  auto tracker2 = registry->createTracker("worker_1");

  auto monitor = createMonitor();

  // Advance time by 100ms (100000us)
  time_system_.advanceTimeWait(std::chrono::milliseconds(100));

  // tracker1 accumulated 10ms (10000us)
  tracker1->reportPrepare(100000, true, 10000);
  tracker1->reportCheck(110000);
  tracker1->reportPrepare(120000, false, 0);

  // tracker2 accumulated 30ms (30000us)
  tracker2->reportPrepare(100000, true, 10000);
  tracker2->reportCheck(110000);
  tracker2->reportPrepare(140000, false, 0);

  // Query should report maximum of the two: 30000us / 100000us = 0.3 pressure
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{0.3}));
  monitor->updateResourceUsage(cb_);
}

TEST_F(EventLoopLatencyMonitorTest, ClampsPressure) {
  auto registry = Event::LoopLatencyRegistry::singleton(nullptr);
  auto tracker1 = registry->createTracker("worker_0");

  auto monitor = createMonitor();

  // Advance time by 50ms (50000us)
  time_system_.advanceTimeWait(std::chrono::milliseconds(50));

  // tracker1 accumulated 60ms (60000us)
  tracker1->reportPrepare(100000, true, 10000);
  tracker1->reportCheck(110000);
  tracker1->reportPrepare(170000, false, 0); // 60ms

  // Query should report 1.0 pressure (clamped)
  EXPECT_CALL(cb_, onSuccess(Server::ResourceUsage{1.0}));
  monitor->updateResourceUsage(cb_);
}

} // namespace
} // namespace EventLoopLatencyMonitor
} // namespace ResourceMonitors
} // namespace Extensions
} // namespace Envoy
