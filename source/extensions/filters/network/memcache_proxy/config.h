#pragma once

#include <string>

#include "envoy/config/filter/network/memcache_proxy/v1alpha1/memcache_proxy.pb.h"
#include "envoy/config/filter/network/memcache_proxy/v1alpha1/memcache_proxy.pb.validate.h"

#include "extensions/filters/network/common/factory_base.h"
#include "extensions/filters/network/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace MemcacheProxy {

/**
 * Config registration for the memcache proxy filter. @see NamedNetworkFilterConfigFactory.
 */
class MemcacheProxyFilterConfigFactory
    : public Common::FactoryBase<
          envoy::config::filter::network::memcache_proxy::v1alpha1::MemcacheProxy> {
public:
  MemcacheProxyFilterConfigFactory() : FactoryBase(NetworkFilterNames::get().MemcacheProxy) {}

  // // NamedNetworkFilterConfigFactory
  // Network::FilterFactoryCb
  // createFilterFactory(const Json::Object& json_config,
  //                     Server::Configuration::FactoryContext& context) override;

private:
  Network::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::config::filter::network::memcache_proxy::v1alpha1::MemcacheProxy& proto_config,
      Server::Configuration::FactoryContext& context) override;
};

} // namespace MemcacheProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
