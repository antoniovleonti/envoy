#include <atomic>
#include <chrono>
#include <memory>
#include <string>

#include "envoy/api/api.h"
#include "envoy/event/event_loop.h"
#include "envoy/server/worker.h"

#include "source/common/api/api_impl.h"
#include "source/common/common/thread.h"
#include "source/common/event/event_loop_registry.h"
#include "source/common/event/scaled_range_timer_manager_impl.h"
#include "source/server/worker_impl.h"

#include "test/mocks/network/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/server/guard_dog.h"
#include "test/mocks/server/overload_manager.h"
#include "test/mocks/thread_local/mocks.h"

#include "absl/synchronization/notification.h"
#include "gtest/gtest.h"

using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Server {
namespace {

class FakeEventLoopTracker : public Event::EventLoop::Tracker {
public:
  FakeEventLoopTracker(const Event::EventLoop::TrackerFactory& factory,
                       std::atomic<uint64_t>& prepare_count, std::atomic<uint64_t>& check_count)
      : factory_(factory), prepare_count_(prepare_count), check_count_(check_count) {}

  const Event::EventLoop::TrackerFactory& factory() const override { return factory_; }

  void reportPrepare(uint64_t, bool, uint64_t) override {
    ASSERT_IS_NOT_MAIN_OR_TEST_THREAD();
    prepare_count_++;
  }

  void reportCheck(uint64_t) override {
    ASSERT_IS_NOT_MAIN_OR_TEST_THREAD();
    check_count_++;
  }

private:
  const Event::EventLoop::TrackerFactory& factory_;
  std::atomic<uint64_t>& prepare_count_;
  std::atomic<uint64_t>& check_count_;
};

class FakeResourceMonitorTrackerFactory : public Event::EventLoop::TrackerFactory {
public:
  Event::EventLoop::TrackerPtr createTracker(const std::string&) override {
    ASSERT_IS_MAIN_OR_TEST_THREAD();
    created_trackers_++;
    return std::make_unique<FakeEventLoopTracker>(*this, prepare_count_, check_count_);
  }

  void pollMetrics() {
    ASSERT_IS_MAIN_OR_TEST_THREAD();
    EXPECT_GT(prepare_count_.load(), 0);
    EXPECT_GT(check_count_.load(), 0);
  }

  std::atomic<uint64_t> created_trackers_{0};
  std::atomic<uint64_t> prepare_count_{0};
  std::atomic<uint64_t> check_count_{0};
};

class TestWorkerFactory : public ProdWorkerFactory {
public:
  TestWorkerFactory(ThreadLocal::Instance& tls, Api::Api& api, ListenerHooks& hooks)
      : ProdWorkerFactory(tls, api, hooks), tls_(tls), api_(api), hooks_(hooks),
        stat_names_(api.rootScope().symbolTable()) {}

  WorkerPtr createWorker(uint32_t, OverloadManager& overload_manager, OverloadManager&,
                         const std::string& worker_name) override {
    auto disp = api_.allocateDispatcher(worker_name, overload_manager.scaledTimerFactory());
    worker_dispatcher_ = disp.get();
    auto conn_handler =
        Network::ConnectionHandlerPtr{new NiceMock<Network::MockConnectionHandler>()};
    return std::make_unique<WorkerImpl>(tls_, hooks_, std::move(disp), std::move(conn_handler),
                                        overload_manager, api_, stat_names_);
  }

  ThreadLocal::Instance& tls_;
  Api::Api& api_;
  ListenerHooks& hooks_;
  WorkerStatNames stat_names_;
  Event::Dispatcher* worker_dispatcher_{nullptr};
};

class EventLoopTrackerIntegrationTest : public testing::Test {
public:
  EventLoopTrackerIntegrationTest()
      : api_(Api::createApiForTest()), stat_names_(api_->rootScope().symbolTable()) {
    ON_CALL(overload_manager_, scaledTimerFactory())
        .WillByDefault(Return([](Event::Dispatcher& dispatcher) {
          return std::make_unique<Event::ScaledRangeTimerManagerImpl>(dispatcher);
        }));
  }

  void cycleWorkerEventLoop(Event::Dispatcher& worker_dispatcher) {
    for (int i = 0; i < 2; ++i) {
      absl::Notification cycled;
      worker_dispatcher.post([&cycled]() { cycled.Notify(); });
      cycled.WaitForNotification();
    }
  }

  NiceMock<ThreadLocal::MockInstance> tls_;
  DefaultListenerHooks hooks_;
  NiceMock<MockGuardDog> guard_dog_;
  NiceMock<MockOverloadManager> overload_manager_;
  Api::ApiPtr api_;
  WorkerStatNames stat_names_;
};

TEST_F(EventLoopTrackerIntegrationTest, StaticBootstrapRegistration) {
  FakeResourceMonitorTrackerFactory monitor_factory;
  auto handle = api_->eventLoopRegistry().registerTrackerFactory(monitor_factory);

  TestWorkerFactory worker_factory(tls_, *api_, hooks_);
  WorkerPtr worker =
      worker_factory.createWorker(0, overload_manager_, overload_manager_, "worker_0");
  EXPECT_EQ(1, monitor_factory.created_trackers_.load());
  ASSERT_NE(nullptr, worker_factory.worker_dispatcher_);

  absl::Notification started;
  Event::TimerPtr keepalive_timer;
  worker->start(guard_dog_, [&worker_factory, &started, &keepalive_timer]() {
    keepalive_timer = worker_factory.worker_dispatcher_->createTimer([]() {});
    keepalive_timer->enableTimer(std::chrono::hours(1));
    started.Notify();
  });
  started.WaitForNotification();

  cycleWorkerEventLoop(*worker_factory.worker_dispatcher_);

  monitor_factory.pollMetrics();

  absl::Notification stopped_timer;
  worker_factory.worker_dispatcher_->post([&keepalive_timer, &stopped_timer]() {
    keepalive_timer.reset();
    stopped_timer.Notify();
  });
  stopped_timer.WaitForNotification();

  worker->stop();
  handle.reset();
}

TEST_F(EventLoopTrackerIntegrationTest, DynamicRuntimeRegistration) {
  TestWorkerFactory worker_factory(tls_, *api_, hooks_);
  WorkerPtr worker =
      worker_factory.createWorker(0, overload_manager_, overload_manager_, "worker_1");
  ASSERT_NE(nullptr, worker_factory.worker_dispatcher_);

  absl::Notification started;
  Event::TimerPtr keepalive_timer;
  worker->start(guard_dog_, [&worker_factory, &started, &keepalive_timer]() {
    keepalive_timer = worker_factory.worker_dispatcher_->createTimer([]() {});
    keepalive_timer->enableTimer(std::chrono::hours(1));
    started.Notify();
  });
  started.WaitForNotification();

  // Register the factory AFTER the worker thread has already spawned and is running its event loop.
  FakeResourceMonitorTrackerFactory dynamic_monitor_factory;
  auto handle = api_->eventLoopRegistry().registerTrackerFactory(dynamic_monitor_factory);

  EXPECT_EQ(1, dynamic_monitor_factory.created_trackers_.load());

  // Post a callback to ensure the attachment posted by registerTrackerFactory has run
  // and at least one subsequent event loop iteration has occurred on the running worker thread.
  cycleWorkerEventLoop(*worker_factory.worker_dispatcher_);

  dynamic_monitor_factory.pollMetrics();

  absl::Notification stopped_timer;
  worker_factory.worker_dispatcher_->post([&keepalive_timer, &stopped_timer]() {
    keepalive_timer.reset();
    stopped_timer.Notify();
  });
  stopped_timer.WaitForNotification();

  worker->stop();
  handle.reset();
}

TEST_F(EventLoopTrackerIntegrationTest, DynamicRuntimeUnregistration) {
  TestWorkerFactory worker_factory(tls_, *api_, hooks_);
  WorkerPtr worker =
      worker_factory.createWorker(0, overload_manager_, overload_manager_, "worker_2");
  ASSERT_NE(nullptr, worker_factory.worker_dispatcher_);

  absl::Notification started;
  Event::TimerPtr keepalive_timer;
  worker->start(guard_dog_, [&worker_factory, &started, &keepalive_timer]() {
    keepalive_timer = worker_factory.worker_dispatcher_->createTimer([]() {});
    keepalive_timer->enableTimer(std::chrono::hours(1));
    started.Notify();
  });
  started.WaitForNotification();

  FakeResourceMonitorTrackerFactory dynamic_monitor_factory;
  auto handle = api_->eventLoopRegistry().registerTrackerFactory(dynamic_monitor_factory);
  EXPECT_EQ(1, dynamic_monitor_factory.created_trackers_.load());

  cycleWorkerEventLoop(*worker_factory.worker_dispatcher_);
  dynamic_monitor_factory.pollMetrics();

  // Unregister the factory while the worker thread is still actively running via RAII handle reset!
  handle.reset();

  // Cycle the event loop to ensure the unregister callback has run on the worker thread.
  cycleWorkerEventLoop(*worker_factory.worker_dispatcher_);

  const uint64_t prepare_count_after_unregister = dynamic_monitor_factory.prepare_count_.load();
  const uint64_t check_count_after_unregister = dynamic_monitor_factory.check_count_.load();

  // Cycle multiple times to confirm callbacks are no longer firing on the worker thread.
  cycleWorkerEventLoop(*worker_factory.worker_dispatcher_);
  cycleWorkerEventLoop(*worker_factory.worker_dispatcher_);

  EXPECT_EQ(prepare_count_after_unregister, dynamic_monitor_factory.prepare_count_.load());
  EXPECT_EQ(check_count_after_unregister, dynamic_monitor_factory.check_count_.load());

  absl::Notification stopped_timer;
  worker_factory.worker_dispatcher_->post([&keepalive_timer, &stopped_timer]() {
    keepalive_timer.reset();
    stopped_timer.Notify();
  });
  stopped_timer.WaitForNotification();

  worker->stop();
}

} // namespace
} // namespace Server
} // namespace Envoy
