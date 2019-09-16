// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <set>

#include "xpbase/base/top_timer.h"
#include "xpbase/base/rand_util.h"
#include "xpbase/base/kad_key/kadmlia_key.h"
#include "xkad/routing_table/bootstrap_cache.h"
#include "xkad/routing_table/node_info.h"

namespace top {
namespace kadmlia {

// get public nodes from self routing table
using GetPublicNodes = std::function<void(std::vector<NodeInfoPtr>&)>;
// get some kind service public node from root-routing-table
using GetServicePublicNodes = std::function<void(uint64_t service_type, std::vector<NodeInfoPtr>&)>;
typedef std::shared_ptr<std::vector<NodeInfoPtr>> VecNodeInfoPtr;

class BootstrapCacheHelper : public std::enable_shared_from_this<BootstrapCacheHelper> {
    static const int32_t kDumpBootstrapPeriod = 60 * 1000 * 1000;  // 60s
    static const int32_t kCacheServiceBootstrapPeriod = 3 * 1000 * 1000;  // 60s
    static const int32_t kCacheServiceNodesSize = 8;   // keep 8 nodes enough
    static const int32_t kCacheServiceNodesDeleteSize = 4; // if more than 8 nodes, delete 4 nodes base insert time
public:
    bool Start(
            base::KadmliaKeyPtr kad_key,
            GetPublicNodes get_public_nodes,
            GetServicePublicNodes get_service_public_nodes = nullptr);
    void Stop();
    void GetPublicEndpoints(std::vector<std::string>& public_endpoints);
    void GetPublicEndpoints(std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    // add target service_type to be cached
    bool SetCacheServiceType(uint64_t service_type);
    // get cache nodes of service_type give
    bool GetCacheServicePublicNodes(
            uint64_t service_type,
            std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    BootstrapCacheHelper(base::TimerManager* timer_manager);
    ~BootstrapCacheHelper();

private:
    void DumpPublicEndpoints();
    void LoadBootstrapCache();
    void RepeatCacheServicePublicNodes();
    void CacheServicePublicNodes(uint64_t service_type);
    uint64_t GetRandomCacheServiceType();

private:
    std::mutex mutex_;
    bool inited_{false};
    base::KadmliaKeyPtr kad_key_;
    std::mutex public_endpoint_mutex_;
    std::vector<std::string> public_endpoints_;
    top::kadmlia::VecBootstrapEndpoint vec_endpoints_;
    base::TimerManager* timer_manager_{nullptr};
    std::shared_ptr<base::TimerRepeated> timer_dump_public_endpoints_;
    std::shared_ptr<base::TimerRepeated> timer_Cache_Service_public_endpoints_;
    kadmlia::BootstrapCachePtr bootstrap_cache_ptr_;
    GetPublicNodes get_public_nodes_;
    // get some kind service public node from root-routing-table
    GetServicePublicNodes get_service_public_nodes_;
    // key is service_type, value is node of the service_type
    std::map<uint64_t, VecNodeInfoPtr> service_public_nodes_;
    std::mutex service_public_nodes_mutex_;
    // keep target service_types which need to be cached
    std::set<uint64_t> cache_service_types_;
    std::mutex cache_service_types_mutex_;
};

}  // namespace kadmlia
}  // namespace top
