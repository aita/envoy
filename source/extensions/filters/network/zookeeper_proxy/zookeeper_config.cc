#include "extensions/filters/network/zookeeper_proxy/zookeeper_config.h"

#include <string>

#include "envoy/config/filter/network/zookeeper_proxy/v1alpha1/zookeeper_proxy.pb.validate.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "common/common/logger.h"

#include "extensions/filters/network/zookeeper_proxy/zookeeper_filter.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace ZooKeeperProxy {

/**
 * Config registration for the ZooKeeper proxy filter. @see NamedNetworkFilterConfigFactory.
 */
Network::FilterFactoryCb
NetworkFilters::ZooKeeperProxy::ZooKeeperConfigFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::network::zookeeper_proxy::v1alpha1::ZooKeeperProxy& proto_config,
    Server::Configuration::FactoryContext& context) {

  ASSERT(!proto_config.stat_prefix().empty());

  const std::string stat_prefix = fmt::format("{}.zookeeper.", proto_config.stat_prefix());
  const uint32_t max_packet_bytes =
      PROTOBUF_GET_WRAPPED_OR_DEFAULT(proto_config, max_packet_bytes, 1024 * 1024);

  ZooKeeperFilterConfigSharedPtr filter_config(
      std::make_shared<ZooKeeperFilterConfig>(stat_prefix, max_packet_bytes, context.scope()));
  return [filter_config](Network::FilterManager& filter_manager) -> void {
    filter_manager.addFilter(std::make_shared<ZooKeeperFilter>(filter_config));
  };
}

/**
 * Static registration for the ZooKeeper proxy filter. @see RegisterFactory.
 */
REGISTER_FACTORY(ZooKeeperConfigFactory, Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace ZooKeeperProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
