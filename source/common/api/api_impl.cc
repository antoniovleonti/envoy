#include "source/common/api/api_impl.h"

#include <chrono>
#include <string>

#include "envoy/config/bootstrap/v3/bootstrap.pb.h"

#include "source/common/common/thread.h"
#include "source/common/event/dispatcher_impl.h"
#include "source/common/event/loop_latency_registry.h"
#include "source/common/event/loop_latency_tracker.h"

namespace Envoy {
namespace Api {

Impl::Impl(Thread::ThreadFactory& thread_factory, Stats::Store& store,
           Event::TimeSystem& time_system, Filesystem::Instance& file_system,
           Random::RandomGenerator& random_generator,
           const envoy::config::bootstrap::v3::Bootstrap& bootstrap,
           const ProcessContextOptRef& process_context,
           Buffer::WatermarkFactorySharedPtr watermark_factory,
           Singleton::Manager* singleton_manager)
    : thread_factory_(thread_factory), store_(store), time_system_(time_system),
      file_system_(file_system), random_generator_(random_generator), bootstrap_(bootstrap),
      process_context_(process_context), watermark_factory_(std::move(watermark_factory)),
      singleton_manager_(singleton_manager) {}

Event::DispatcherPtr Impl::allocateDispatcher(const std::string& name) {
  return std::make_unique<Event::DispatcherImpl>(name, *this, time_system_, watermark_factory_);
}

Event::DispatcherPtr
Impl::allocateDispatcher(const std::string& name,
                         const Event::ScaledRangeTimerManagerFactory& scaled_timer_factory) {
  return std::make_unique<Event::DispatcherImpl>(name, *this, time_system_, scaled_timer_factory,
                                                 watermark_factory_);
}

Event::DispatcherPtr
Impl::allocateWorkerDispatcher(const std::string& name,
                               const Event::ScaledRangeTimerManagerFactory& scaled_timer_factory) {
  std::unique_ptr<Event::LoopLatencyTracker> tracker;
  if (singleton_manager_ != nullptr) {
    tracker = Event::LoopLatencyRegistry::singleton(singleton_manager_)->createTracker(name);
  } else {
    tracker = std::make_unique<Event::LoopLatencyTracker>(name, nullptr);
  }
  return std::make_unique<Event::DispatcherImpl>(name, *this, time_system_, scaled_timer_factory,
                                                 watermark_factory_,
                                                 std::move(tracker));
}

Event::DispatcherPtr Impl::allocateDispatcher(const std::string& name,
                                              Buffer::WatermarkFactoryPtr&& factory) {
  return std::make_unique<Event::DispatcherImpl>(name, *this, time_system_, std::move(factory));
}

} // namespace Api
} // namespace Envoy
