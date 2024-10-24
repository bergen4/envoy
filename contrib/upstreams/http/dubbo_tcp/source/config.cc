#include "config.h"

#include "upstream_request.h"

namespace Envoy {
namespace Extensions {
namespace Upstreams {
namespace Http {
namespace DubboTcp {

Router::GenericConnPoolPtr DubboTcpGenericConnPoolFactory::createGenericConnPool(
    Upstream::ThreadLocalCluster& thread_local_cluster, UpstreamProtocol,
    const Router::RouteEntry& route_entry,
    absl::optional<Envoy::Http::Protocol>,
    Upstream::LoadBalancerContext* ctx) const {
  auto ret = std::make_unique<TcpConnPool>(thread_local_cluster, route_entry, ctx);
  return (ret->valid() ? std::move(ret) : nullptr);
}

REGISTER_FACTORY(DubboTcpGenericConnPoolFactory, Router::GenericConnPoolFactory);

} // namespace DubboTcp
} // namespace Http
} // namespace Upstreams
} // namespace Extensions
} // namespace Envoy
