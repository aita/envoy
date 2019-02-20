#include "extensions/filters/network/memcache_proxy/command_splitter_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "envoy/stats/scope.h"

#include "common/common/assert.h"
#include "common/common/fmt.h"

#include "extensions/filters/network/memcache_proxy/supported_commands.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MemcacheProxy {
namespace CommandSplitter {

RespValuePtr Utility::makeError(const std::string& error) {
  RespValuePtr response(new RespValue());
  response->type(RespType::Error);
  response->asString() = error;
  return response;
}

void SplitRequestBase::onWrongNumberOfArguments(SplitCallbacks& callbacks,
                                                const RespValue& request) {
  callbacks.onResponse(Utility::makeError(
      fmt::format("wrong number of arguments for '{}' command", request.asArray()[0].asString())));
}

void SplitRequestBase::updateStats(const bool success) {
  if (success) {
    command_stats_.success_.inc();
  } else {
    command_stats_.error_.inc();
  }
  command_latency_ms_->complete();
}

SingleServerRequest::~SingleServerRequest() { ASSERT(!handle_); }

void SingleServerRequest::onResponse(RespValuePtr&& response) {
  handle_ = nullptr;
  updateStats(true);
  callbacks_.onResponse(std::move(response));
}

void SingleServerRequest::onFailure() {
  handle_ = nullptr;
  updateStats(false);
  callbacks_.onResponse(Utility::makeError(Response::get().UpstreamFailure));
}

void SingleServerRequest::cancel() {
  handle_->cancel();
  handle_ = nullptr;
}

SplitRequestPtr SimpleRequest::create(ConnPool::Instance& conn_pool,
                                      const RespValue& incoming_request, SplitCallbacks& callbacks,
                                      CommandStats& command_stats, TimeSource& time_source) {
  std::unique_ptr<SimpleRequest> request_ptr{
      new SimpleRequest(callbacks, command_stats, time_source)};

  request_ptr->handle_ = conn_pool.makeRequest(incoming_request.asArray()[1].asString(),
                                               incoming_request, *request_ptr);
  if (!request_ptr->handle_) {
    request_ptr->callbacks_.onResponse(Utility::makeError(Response::get().NoUpstreamHost));
    return nullptr;
  }

  return std::move(request_ptr);
}

FragmentedRequest::~FragmentedRequest() {
#ifndef NDEBUG
  for (const PendingRequest& request : pending_requests_) {
    ASSERT(!request.handle_);
  }
#endif
}

void FragmentedRequest::cancel() {
  for (PendingRequest& request : pending_requests_) {
    if (request.handle_) {
      request.handle_->cancel();
      request.handle_ = nullptr;
    }
  }
}

void FragmentedRequest::onChildFailure(uint32_t index) {
  onChildResponse(Utility::makeError(Response::get().UpstreamFailure), index);
}

SplitRequestPtr GETRequest::create(ConnPool::Instance& conn_pool, const RespValue& incoming_request,
                                   SplitCallbacks& callbacks, CommandStats& command_stats,
                                   TimeSource& time_source) {
  std::unique_ptr<GETRequest> request_ptr{new GETRequest(callbacks, command_stats, time_source)};

  request_ptr->num_pending_responses_ = incoming_request.asArray().size() - 1;
  request_ptr->pending_requests_.reserve(request_ptr->num_pending_responses_);

  request_ptr->pending_response_ = std::make_unique<RespValue>();
  request_ptr->pending_response_->type(RespType::Array);
  std::vector<RespValue> responses(request_ptr->num_pending_responses_);
  request_ptr->pending_response_->asArray().swap(responses);

  std::vector<RespValue> values(2);
  values[0].type(RespType::BulkString);
  values[0].asString() = "get";
  values[1].type(RespType::BulkString);
  RespValue single_mget;
  single_mget.type(RespType::Array);
  single_mget.asArray().swap(values);

  for (uint64_t i = 1; i < incoming_request.asArray().size(); i++) {
    request_ptr->pending_requests_.emplace_back(*request_ptr, i - 1);
    PendingRequest& pending_request = request_ptr->pending_requests_.back();

    single_mget.asArray()[1].asString() = incoming_request.asArray()[i].asString();
    ENVOY_LOG(debug, "memcache: parallel get: '{}'", single_mget.toString());
    pending_request.handle_ = conn_pool.makeRequest(incoming_request.asArray()[i].asString(),
                                                    single_mget, pending_request);
    if (!pending_request.handle_) {
      pending_request.onResponse(Utility::makeError(Response::get().NoUpstreamHost));
    }
  }

  return request_ptr->num_pending_responses_ > 0 ? std::move(request_ptr) : nullptr;
}

void GETRequest::onChildResponse(RespValuePtr&& value, uint32_t index) {
  pending_requests_[index].handle_ = nullptr;

  pending_response_->asArray()[index].type(value->type());
  switch (value->type()) {
  case RespType::Array:
  case RespType::Integer:
  case RespType::SimpleString: {
    pending_response_->asArray()[index].type(RespType::Error);
    pending_response_->asArray()[index].asString() = Response::get().UpstreamProtocolError;
    error_count_++;
    break;
  }
  case RespType::Error: {
    error_count_++;
    FALLTHRU;
  }
  case RespType::BulkString: {
    pending_response_->asArray()[index].asString().swap(value->asString());
    break;
  }
  case RespType::Null:
    break;
  }

  ASSERT(num_pending_responses_ > 0);
  if (--num_pending_responses_ == 0) {
    updateStats(error_count_ == 0);
    ENVOY_LOG(debug, "memcache: response: '{}'", pending_response_->toString());
    callbacks_.onResponse(std::move(pending_response_));
  }
}

SplitRequestPtr SplitKeysSumResultRequest::create(ConnPool::Instance& conn_pool,
                                                  const RespValue& incoming_request,
                                                  SplitCallbacks& callbacks,
                                                  CommandStats& command_stats,
                                                  TimeSource& time_source) {
  std::unique_ptr<SplitKeysSumResultRequest> request_ptr{
      new SplitKeysSumResultRequest(callbacks, command_stats, time_source)};

  request_ptr->num_pending_responses_ = incoming_request.asArray().size() - 1;
  request_ptr->pending_requests_.reserve(request_ptr->num_pending_responses_);

  request_ptr->pending_response_ = std::make_unique<RespValue>();
  request_ptr->pending_response_->type(RespType::Integer);

  std::vector<RespValue> values(2);
  values[0].type(RespType::BulkString);
  values[0].asString() = incoming_request.asArray()[0].asString();
  values[1].type(RespType::BulkString);
  RespValue single_fragment;
  single_fragment.type(RespType::Array);
  single_fragment.asArray().swap(values);

  for (uint64_t i = 1; i < incoming_request.asArray().size(); i++) {
    request_ptr->pending_requests_.emplace_back(*request_ptr, i - 1);
    PendingRequest& pending_request = request_ptr->pending_requests_.back();

    single_fragment.asArray()[1].asString() = incoming_request.asArray()[i].asString();
    ENVOY_LOG(debug, "memcache: parallel {}: '{}'", incoming_request.asArray()[0].asString(),
              single_fragment.toString());
    pending_request.handle_ = conn_pool.makeRequest(incoming_request.asArray()[i].asString(),
                                                    single_fragment, pending_request);
    if (!pending_request.handle_) {
      pending_request.onResponse(Utility::makeError(Response::get().NoUpstreamHost));
    }
  }

  return request_ptr->num_pending_responses_ > 0 ? std::move(request_ptr) : nullptr;
}

void SplitKeysSumResultRequest::onChildResponse(RespValuePtr&& value, uint32_t index) {
  pending_requests_[index].handle_ = nullptr;

  switch (value->type()) {
  case RespType::Integer: {
    total_ += value->asInteger();
    break;
  }
  default: {
    error_count_++;
    break;
  }
  }

  ASSERT(num_pending_responses_ > 0);
  if (--num_pending_responses_ == 0) {
    updateStats(error_count_ == 0);
    if (error_count_ == 0) {
      pending_response_->asInteger() = total_;
      callbacks_.onResponse(std::move(pending_response_));
    } else {
      callbacks_.onResponse(
          Utility::makeError(fmt::format("finished with {} error(s)", error_count_)));
    }
  }
}

InstanceImpl::InstanceImpl(ConnPool::InstancePtr&& conn_pool, Stats::Scope& scope,
                           const std::string& stat_prefix, TimeSource& time_source)
    : conn_pool_(std::move(conn_pool)), simple_command_handler_(*conn_pool_),
      get_handler_(*conn_pool_), split_keys_sum_result_handler_(*conn_pool_),
      stats_{ALL_COMMAND_SPLITTER_STATS(POOL_COUNTER_PREFIX(scope, stat_prefix + "splitter."))},
      time_source_(time_source) {
  for (const std::string& command : SupportedCommands::simpleCommands()) {
    addHandler(scope, stat_prefix, command, simple_command_handler_);
  }

  for (const std::string& command : SupportedCommands::hashMultipleSumResultCommands()) {
    addHandler(scope, stat_prefix, command, split_keys_sum_result_handler_);
  }

  addHandler(scope, stat_prefix, SupportedCommands::get(), get_handler_);
}

SplitRequestPtr InstanceImpl::makeRequest(const RespValue& request, SplitCallbacks& callbacks) {
  if (request.type() != RespType::Array) {
    onInvalidRequest(callbacks);
    return nullptr;
  }

  std::string to_lower_string(request.asArray()[0].asString());
  to_lower_table_.toLowerCase(to_lower_string);

  if (request.asArray().size() < 2) {
    // Commands other than PING all have at least two arguments.
    onInvalidRequest(callbacks);
    return nullptr;
  }

  for (const RespValue& value : request.asArray()) {
    if (value.type() != RespType::BulkString) {
      onInvalidRequest(callbacks);
      return nullptr;
    }
  }

  auto handler = handler_lookup_table_.find(to_lower_string.c_str());
  if (handler == nullptr) {
    stats_.unsupported_command_.inc();
    callbacks.onResponse(Utility::makeError(
        fmt::format("unsupported command '{}'", request.asArray()[0].asString())));
    return nullptr;
  }
  ENVOY_LOG(debug, "memcache: splitting '{}'", request.toString());
  handler->command_stats_.total_.inc();
  SplitRequestPtr request_ptr = handler->handler_.get().startRequest(
      request, callbacks, handler->command_stats_, time_source_);
  return request_ptr;
}

void InstanceImpl::onInvalidRequest(SplitCallbacks& callbacks) {
  stats_.invalid_request_.inc();
  callbacks.onResponse(Utility::makeError(Response::get().InvalidRequest));
}

void InstanceImpl::addHandler(Stats::Scope& scope, const std::string& stat_prefix,
                              const std::string& name, CommandHandler& handler) {
  std::string to_lower_name(name);
  to_lower_table_.toLowerCase(to_lower_name);
  const std::string command_stat_prefix = fmt::format("{}command.{}.", stat_prefix, to_lower_name);
  handler_lookup_table_.add(
      to_lower_name.c_str(),
      std::make_shared<HandlerData>(HandlerData{
          CommandStats{ALL_COMMAND_STATS(POOL_COUNTER_PREFIX(scope, command_stat_prefix),
                                         POOL_HISTOGRAM_PREFIX(scope, command_stat_prefix))},
          handler}));
}

} // namespace CommandSplitter
} // namespace MemcacheProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
