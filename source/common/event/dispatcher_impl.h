#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <vector>

#include "envoy/api/api.h"
#include "envoy/common/time.h"
#include "envoy/event/deferred_deletable.h"
#include "envoy/event/dispatcher.h"
#include "envoy/network/connection_handler.h"

#include "common/common/logger.h"
#include "common/common/thread.h"
#include "common/event/libevent.h"
#include "common/event/libevent_scheduler.h"

namespace Envoy {
namespace Event {

/**
 * libevent implementation of Event::Dispatcher.
 */
class DispatcherImpl : Logger::Loggable<Logger::Id::main>, public Dispatcher {
public:
  DispatcherImpl(Api::Api& api, Event::TimeSystem& time_system);
  DispatcherImpl(Buffer::WatermarkFactoryPtr&& factory, Api::Api& api,
                 Event::TimeSystem& time_system);
  ~DispatcherImpl();

  /**
   * @return event_base& the libevent base.
   */
  event_base& base() { return base_scheduler_.base(); }

  // Event::Dispatcher
  TimeSource& timeSource() override { return api_.timeSource(); }
  void clearDeferredDeleteList() override;
  Network::ConnectionPtr
  createServerConnection(Network::ConnectionSocketPtr&& socket,
                         Network::TransportSocketPtr&& transport_socket) override;
  Network::ClientConnectionPtr
  createClientConnection(Network::Address::InstanceConstSharedPtr address,
                         Network::Address::InstanceConstSharedPtr source_address,
                         Network::TransportSocketPtr&& transport_socket,
                         const Network::ConnectionSocket::OptionsSharedPtr& options) override;
  Network::DnsResolverSharedPtr createDnsResolver(
      const std::vector<Network::Address::InstanceConstSharedPtr>& resolvers) override;
  FileEventPtr createFileEvent(int fd, FileReadyCb cb, FileTriggerType trigger,
                               uint32_t events) override;
  Filesystem::WatcherPtr createFilesystemWatcher() override;
  Network::ListenerPtr createListener(Network::Socket& socket, Network::ListenerCallbacks& cb,
                                      bool bind_to_port,
                                      bool hand_off_restored_destination_connections) override;
  Network::ListenerPtr createUdpListener(Network::Socket& socket,
                                         Network::UdpListenerCallbacks& cb) override;
  TimerPtr createTimer(TimerCb cb) override;
  void deferredDelete(DeferredDeletablePtr&& to_delete) override;
  void exit() override;
  SignalEventPtr listenForSignal(int signal_num, SignalCb cb) override;
  void post(std::function<void()> callback) override;
  void run(RunType type) override;
  Buffer::WatermarkFactory& getWatermarkFactory() override { return *buffer_factory_; }

private:
  void runPostCallbacks();

  // Validate that an operation is thread safe, i.e. it's invoked on the same thread that the
  // dispatcher run loop is executing on. We allow run_tid_ == nullptr for tests where we don't
  // invoke run().
  bool isThreadSafe() const { return run_tid_ == nullptr || run_tid_->isCurrentThreadId(); }

  Api::Api& api_;
  Thread::ThreadIdPtr run_tid_;
  Buffer::WatermarkFactoryPtr buffer_factory_;
  LibeventScheduler base_scheduler_;
  SchedulerPtr scheduler_;
  TimerPtr deferred_delete_timer_;
  TimerPtr post_timer_;
  std::vector<DeferredDeletablePtr> to_delete_1_;
  std::vector<DeferredDeletablePtr> to_delete_2_;
  std::vector<DeferredDeletablePtr>* current_to_delete_;
  Thread::MutexBasicLockable post_lock_;
  std::list<std::function<void()>> post_callbacks_ GUARDED_BY(post_lock_);
  bool deferred_deleting_{};
};

} // namespace Event
} // namespace Envoy
