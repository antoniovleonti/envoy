#include <memory>
#include <string>

#include "envoy/event/event_loop_tracker.h"
#include "source/common/event/event_loop_tracker_registry.h"

#include "test/mocks/event/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace Envoy {
namespace Event {
namespace {

class MockEventLoopTracker : public EventLoopTracker {
public:
  MOCK_METHOD(void, reportPrepare,
              (uint64_t prepare_time_us, bool timeout_set, uint64_t timeout_us));
  MOCK_METHOD(void, reportCheck, (uint64_t check_time_us));
};

class MockEventLoopTrackerFactory : public EventLoopTrackerFactory {
public:
  MOCK_METHOD(std::unique_ptr<EventLoopTracker>, createWorkerTracker, (const std::string&));
};

TEST(EventLoopTrackerRegistryTest, EmptyRegistryDoesNothing) {
  EventLoopTrackerRegistryImpl registry;
  StrictMock<MockDispatcher> dispatcher("worker_0");

  EXPECT_CALL(dispatcher, registerEventLoopTracker(_)).Times(0);
  registry.registerWorkerDispatcher(dispatcher);
}

TEST(EventLoopTrackerRegistryTest, StaticBootstrapRegistration) {
  EventLoopTrackerRegistryImpl registry;
  MockEventLoopTrackerFactory factory;
  StrictMock<MockDispatcher> dispatcher("worker_1");

  auto mock_tracker = std::make_unique<MockEventLoopTracker>();
  MockEventLoopTracker* tracker_ptr = mock_tracker.get();

  EXPECT_CALL(factory, createWorkerTracker("worker_1"))
      .WillOnce(Return(testing::ByMove(std::move(mock_tracker))));
  EXPECT_CALL(dispatcher, registerEventLoopTracker(_))
      .WillOnce(Invoke([tracker_ptr](std::unique_ptr<EventLoopTracker> tracker) {
        EXPECT_EQ(tracker_ptr, tracker.get());
      }));

  registry.registerTrackerFactory(factory);
  registry.registerWorkerDispatcher(dispatcher);

  registry.unregisterTrackerFactory(factory);
  registry.unregisterWorkerDispatcher(dispatcher);
}

TEST(EventLoopTrackerRegistryTest, DynamicRuntimeRegistration) {
  EventLoopTrackerRegistryImpl registry;
  MockEventLoopTrackerFactory factory;
  StrictMock<MockDispatcher> dispatcher("worker_2");

  registry.registerWorkerDispatcher(dispatcher);

  auto mock_tracker = std::make_unique<MockEventLoopTracker>();
  MockEventLoopTracker* tracker_ptr = mock_tracker.get();

  EXPECT_CALL(factory, createWorkerTracker("worker_2"))
      .WillOnce(Return(testing::ByMove(std::move(mock_tracker))));

  PostCb posted_cb;
  EXPECT_CALL(dispatcher, post(_)).WillOnce(Invoke([&posted_cb](PostCb cb) {
    posted_cb = std::move(cb);
  }));

  registry.registerTrackerFactory(factory);
  ASSERT_TRUE(posted_cb != nullptr);

  EXPECT_CALL(dispatcher, registerEventLoopTracker(_))
      .WillOnce(Invoke([tracker_ptr](std::unique_ptr<EventLoopTracker> tracker) {
        EXPECT_EQ(tracker_ptr, tracker.get());
      }));
  posted_cb();

  registry.unregisterTrackerFactory(factory);
  registry.unregisterWorkerDispatcher(dispatcher);
}

TEST(EventLoopTrackerRegistryTest, UnregisterWorkerDispatcher) {
  EventLoopTrackerRegistryImpl registry;
  MockEventLoopTrackerFactory factory;
  StrictMock<MockDispatcher> dispatcher("worker_3");

  registry.registerWorkerDispatcher(dispatcher);
  registry.unregisterWorkerDispatcher(dispatcher);

  EXPECT_CALL(factory, createWorkerTracker(_)).Times(0);
  EXPECT_CALL(dispatcher, post(_)).Times(0);

  registry.registerTrackerFactory(factory);
}

} // namespace
} // namespace Event
} // namespace Envoy
