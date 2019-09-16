// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/bootstrap_cache.h"

#include <assert.h>
#include <string>
#include <limits>
#include <mutex>

#include "xpbase/base/top_log.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/proto/ledger.pb.h"

namespace top {
namespace kadmlia {

BootstrapCachePtr GetBootstrapCache(uint64_t service_type) {
    return BootstrapCacheManager::Instance()->GetBootStrapCache(service_type);
}

BootstrapCacheManager* BootstrapCacheManager::Instance() {
    static BootstrapCacheManager ins;
    static std::once_flag s_flag;
    std::call_once(s_flag, []{
        if (!ins.Init()) {
            assert(0);
        }
    });

    return &ins;
}

BootstrapCache::BootstrapCache() {}
BootstrapCache::~BootstrapCache() {}

bool BootstrapCache::GetCache(VecBootstrapEndpoint& vec_bootstrap_endpoint) {
    return manager_->GetCache(service_type_, vec_bootstrap_endpoint);
}

bool BootstrapCache::SetCache(const VecBootstrapEndpoint& vec_bootstrap_endpoint) {
    return manager_->SetCache(service_type_, vec_bootstrap_endpoint);
}

BootstrapCacheManager::BootstrapCacheManager() {}
BootstrapCacheManager::~BootstrapCacheManager() {}

bool BootstrapCacheManager::Init() {
    TOP_INFO("BootstrapCacheManager::Init() ...");
    Lock lock(mutex_);
    if (inited_) {
        TOP_INFO("inited before");
        return true;
    }

    inited_ = true;
    TOP_INFO("BootstrapCacheManager::Init() success");
    return true;
}

BootstrapCachePtr BootstrapCacheManager::GetBootStrapCache(uint64_t service_type) {
    Lock lock(mutex_);
    assert(inited_);

    auto it = map_.find(service_type);
    if (it != map_.end())
        return it->second;

    auto ptr = std::make_shared<BootstrapCache>();
    ptr->service_type_ = service_type;
    ptr->manager_ = this;
    map_[service_type] = ptr;
    TOP_INFO("create bootstrap cache[%llu]", (int)service_type);
    return ptr;
}

// size
// static std::string ToString(const VecBootstrapEndpoint& vec_bootstrap_endpoint) {
//     top::kadmlia::pb::PbBootstrapCache pb_cache;
//     for (const auto& ep : vec_bootstrap_endpoint) {
//         auto pb_ep = pb_cache.add_endpoints();
//         pb_ep->set_ip(ep.first);
//         pb_ep->set_port(ep.second);
//     }
//     std::string ret;
//     assert(pb_cache.SerializeToString(&ret));
//     return ret;
// }

// static bool ToVecBootstrapEndpoint(const std::string& str, VecBootstrapEndpoint& vec_bootstrap_endpoint) {
//     top::kadmlia::pb::PbBootstrapCache pb_cache;
//     if (!pb_cache.ParseFromString(str)) {
//         TOP_ERROR("cache.ParseFromString failed");
//         return false;
//     }

//     vec_bootstrap_endpoint.clear();
//     for (int i = 0; i < pb_cache.endpoints_size(); ++i) {
//         const auto& pb_ep = pb_cache.endpoints(i);
//         if (pb_ep.port() > std::numeric_limits<uint16_t>::max()) {
//             TOP_WARN("ignore invalid endpoint(%s:%d)", pb_ep.ip().c_str(), (int)pb_ep.port());
//             continue;
//         }
//         vec_bootstrap_endpoint.push_back(std::make_pair(pb_ep.ip(), (uint16_t)pb_ep.port()));
//     }

//     return true;
// }

bool BootstrapCacheManager::SetCache(uint64_t service_type, const VecBootstrapEndpoint& vec_bootstrap_endpoint) {
//     Lock lock(mutex_);
//     assert(inited_);
// 
//     const std::string field = std::to_string(service_type);
//     const std::string value = ToString(vec_bootstrap_endpoint);
//     const auto ret = top::storage::XLedgerDB::Instance()->map_set(
//         top::kadmlia::BOOTSTRAP_CACHE_DB_KEY,
//         field,
//         value);
//     if (ret != top::ledger::ok) {
//         TOP_ERROR("ledger map_set(%s, %s) failed, ret=%d",
//             top::kadmlia::BOOTSTRAP_CACHE_DB_KEY.c_str(),
//             field.c_str(),
//             ret);
//         return false;
//     }

    return true;
}

bool BootstrapCacheManager::GetCache(uint64_t service_type, VecBootstrapEndpoint& vec_bootstrap_endpoint) {
//     Lock lock(mutex_);
//     assert(inited_);
// 
//     const std::string field = std::to_string(service_type);
//     std::string value;
//     const auto ret = top::storage::XLedgerDB::Instance()->map_get(
//         top::kadmlia::BOOTSTRAP_CACHE_DB_KEY,
//         field,
//         value);
//     if (ret != top::ledger::ok) {
//         TOP_ERROR("ledger map_get(%s, %s) failed, ret=%d",
//             top::kadmlia::BOOTSTRAP_CACHE_DB_KEY.c_str(),
//             field.c_str(),
//             ret);
//         return false;
//     }
//     
//     if (!ToVecBootstrapEndpoint(value, vec_bootstrap_endpoint)) {
//         TOP_ERROR("parse vec_bootstrap_endpoint from value failed");
//         return false;
//     }

    return true;
}

}  // namespace kadmlia
}  // namespace top
