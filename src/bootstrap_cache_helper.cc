// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/bootstrap_cache_helper.h"

#include <assert.h>
#include <functional>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/check_cast.h"
#include "xpbase/base/endpoint_util.h"

namespace top {
namespace kadmlia {

bool BootstrapCacheHelper::Start(
        base::KadmliaKeyPtr kad_key,
        GetPublicNodes get_public_nodes,
        GetServicePublicNodes get_service_public_nodes) {
    timer_dump_public_endpoints_ = std::make_shared<base::TimerRepeated>(timer_manager_, "dump_public_endpoints");
    timer_Cache_Service_public_endpoints_ = std::make_shared<base::TimerRepeated>(timer_manager_, "Cache_Service_public_endpoints");

    assert(get_public_nodes != nullptr);
    std::unique_lock<std::mutex> lock(mutex_);

    if (inited_) {
        TOP_INFO("BootstrapCacheHelper::Start before");
        return true;
    }

    // usually service_type_ is self-routing-table-service_type
    kad_key_ = kad_key;
    get_public_nodes_ = get_public_nodes;
    if (kad_key->xnetwork_id() == top::kRoot) {
        // only set GetServicePublicNodes for kRoot
        get_service_public_nodes_ = get_service_public_nodes;
        TOP_INFO("BootstrapCacheHelper:: set GetServicePublicNodes Function");
        
        timer_Cache_Service_public_endpoints_->Start(
                1 * 1000 * 1000,  // call wait 1 second
                kCacheServiceBootstrapPeriod,
                std::bind(&BootstrapCacheHelper::RepeatCacheServicePublicNodes, shared_from_this()));
    }

    bootstrap_cache_ptr_ = GetBootstrapCache(kad_key->GetServiceType());
    LoadBootstrapCache();

    timer_dump_public_endpoints_->Start(
            kDumpBootstrapPeriod,
            kDumpBootstrapPeriod,
            std::bind(&BootstrapCacheHelper::DumpPublicEndpoints, shared_from_this()));

    inited_ = true;
    TOP_INFO("bootstrap cache helper(%llu) start success", kad_key->GetServiceType());
    return true;
}

BootstrapCacheHelper::BootstrapCacheHelper(base::TimerManager* timer_manager) {
    timer_manager_ = timer_manager;
}

BootstrapCacheHelper::~BootstrapCacheHelper() {
    // TOP_FATAL("~~~~~~~~~~~~~~~~~BootstrapCacheHelper()");
}

void BootstrapCacheHelper::Stop() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!inited_) {
            TOP_WARN("not inited");
            return;
        }

        inited_ = false;
    }

    if (timer_dump_public_endpoints_) {
        timer_dump_public_endpoints_->Join();
        timer_dump_public_endpoints_ = nullptr;
    }

    if (timer_Cache_Service_public_endpoints_) {
        timer_Cache_Service_public_endpoints_->Join();
        timer_Cache_Service_public_endpoints_ = nullptr;
    }

    bootstrap_cache_ptr_ = nullptr;
    get_public_nodes_ = nullptr;
    get_service_public_nodes_ = nullptr;
}

void BootstrapCacheHelper::GetPublicEndpoints(std::vector<std::string>& public_endpoints) {
    std::unique_lock<std::mutex> lock(public_endpoint_mutex_);
    public_endpoints = public_endpoints_;
}

void BootstrapCacheHelper::GetPublicEndpoints(std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    std::unique_lock<std::mutex> lock(public_endpoint_mutex_);
    base::ParseVecEndpoint(public_endpoints_, boot_endpoints);
}

uint64_t BootstrapCacheHelper::GetRandomCacheServiceType() {
    if (!get_service_public_nodes_) {
        return top::kInvalidType;
    }
    uint64_t this_time_service_type = top::kInvalidType;
    {
        std::unique_lock<std::mutex> lock(cache_service_types_mutex_);
        if (cache_service_types_.empty()) {
            //TOP_WARN("BootstrapCacheHelper:: no target cache service_type set");
            return top::kInvalidType;
        }
        // choose one target service_type randomly one time
        uint32_t rand_idx = base::GetRandomInt64() % cache_service_types_.size();
        auto it = cache_service_types_.begin();
        std::advance(it, rand_idx);
        this_time_service_type = *it;
    }
    if (this_time_service_type == top::kInvalidType) {
        TOP_ERROR("BootstrapCacheHelper:: this time service_type choose invalid");
        return top::kInvalidType;
    }
    return this_time_service_type;
}

void BootstrapCacheHelper::RepeatCacheServicePublicNodes() {
    uint64_t this_time_service_type = GetRandomCacheServiceType();
    if (this_time_service_type == top::kInvalidType) {
        return;
    }
    CacheServicePublicNodes(this_time_service_type);
}

void BootstrapCacheHelper::CacheServicePublicNodes(uint64_t service_type) {
    if (!get_service_public_nodes_) {
        return;
    }
    {
        std::unique_lock<std::mutex> lock(cache_service_types_mutex_);
        if (cache_service_types_.empty()) {
            TOP_WARN("BootstrapCacheHelper:: no target cache service_type set");
            return;
        }
        if (cache_service_types_.find(service_type) == cache_service_types_.end()) {
            TOP_ERROR("BootstrapCacheHelper:: service_type[%llu] not set yet", service_type);
            return;
        }
    }
    uint64_t this_time_service_type = service_type;

    TOP_DEBUG("BootstrapCacheHelper:: begin cache service public node for [%d]",
            this_time_service_type);
    std::vector<NodeInfoPtr> service_nodes;
    get_service_public_nodes_(this_time_service_type, service_nodes);
    if (service_nodes.empty()) {
        TOP_WARN("BootstrapCacheHelper:: cache service public node for %llu failed",
                this_time_service_type);
        return;
    }
    
    std::unique_lock<std::mutex> lock(service_public_nodes_mutex_);
    VecNodeInfoPtr vec_node_info_ptr = nullptr;
    auto ifind = service_public_nodes_.find(this_time_service_type);
    if (ifind == service_public_nodes_.end()) {
        vec_node_info_ptr = std::make_shared<std::vector<NodeInfoPtr>>();
        service_public_nodes_[this_time_service_type] = vec_node_info_ptr;
    } else {
        vec_node_info_ptr = ifind->second;
    }

    // reserve() is optional - just to improve performance
    uint32_t re_size = vec_node_info_ptr->size()
        + std::distance(service_nodes.begin(),service_nodes.end());

    vec_node_info_ptr->reserve(re_size);
    vec_node_info_ptr->insert(
            vec_node_info_ptr->end(),
            service_nodes.begin(),
            service_nodes.end());

    if (vec_node_info_ptr->size() > kCacheServiceNodesSize) {
        for (int32_t i = 0; i < kCacheServiceNodesDeleteSize; ++i) {
            vec_node_info_ptr->erase(vec_node_info_ptr->begin());
        }
        TOP_DEBUG("BootstrapCacheHelper:: die out %d service nodes, now has[%d]",
                kCacheServiceNodesDeleteSize,
                vec_node_info_ptr->size());
    }

    TOP_DEBUG("BootstrapCacheHelper:: put %d service[%d] node into cache vector, now has[%d]",
            service_nodes.size(),
            this_time_service_type,
            vec_node_info_ptr->size());
    return;
}

bool BootstrapCacheHelper::SetCacheServiceType(uint64_t service_type) {
    {
        std::unique_lock<std::mutex> lock(cache_service_types_mutex_);
        cache_service_types_.insert(service_type);
    }
    TOP_INFO("BootstrapCacheHelper:: insert service_type[%d] to cache_service_types", service_type);
    /*
    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    if (!GetCacheServicePublicNodes(service_type, boot_endpoints)) {
        CacheServicePublicNodes(service_type);
    }*/
    return true;
}

bool BootstrapCacheHelper::GetCacheServicePublicNodes(
        uint64_t service_type,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    std::unique_lock<std::mutex> lock(service_public_nodes_mutex_);
    auto ifind = service_public_nodes_.find(service_type);
    VecNodeInfoPtr vec_node_info_ptr = nullptr;
    if (ifind != service_public_nodes_.end()) {
        vec_node_info_ptr = ifind->second;
        if (!vec_node_info_ptr->empty()) {
            for (auto& v : *vec_node_info_ptr) {
                boot_endpoints.insert(std::make_pair(v->public_ip, v->public_port));
            }
            TOP_DEBUG("BootstrapCacheHelper:: GetCacheServicePublicNodes of %d success, %d nodes get",
                    service_type,
                    boot_endpoints.size());
            return true;
        }
    } else {
        vec_node_info_ptr = std::make_shared<std::vector<NodeInfoPtr>>();
        service_public_nodes_[service_type] = vec_node_info_ptr;
    }
    // get node of service failed, make sure try one time
    if (!get_service_public_nodes_) {
        TOP_WARN("not set get_service_public_nodes_ function, get error!");
        return false;
    }
    std::vector<NodeInfoPtr> service_nodes;
    get_service_public_nodes_(service_type, service_nodes);
    if (service_nodes.empty()) {
        TOP_ERROR("BootstrapCacheHelper:: GetCacheServicePublicNodes failed for service_type[%llu]",
                service_type);
        return false;
    }

    for (auto& v : service_nodes) {
        boot_endpoints.insert(std::make_pair(v->public_ip, v->public_port));
    }

    // reserve() is optional - just to improve performance
    uint32_t re_size = vec_node_info_ptr->size()
        + std::distance(service_nodes.begin(),service_nodes.end());

    vec_node_info_ptr->reserve(re_size);
    vec_node_info_ptr->insert(
            vec_node_info_ptr->end(),
            service_nodes.begin(),
            service_nodes.end());

    return true;
}

void BootstrapCacheHelper::DumpPublicEndpoints() {
    std::vector<NodeInfoPtr> nodes;
    get_public_nodes_(nodes);

    {
        std::unique_lock<std::mutex> lock(public_endpoint_mutex_);
        public_endpoints_.clear();
        vec_endpoints_.clear();
        for (auto& node_ptr : nodes) {
            const std::string ip = node_ptr->public_ip;
            const uint16_t port = node_ptr->public_port;
            const std::string endpoint = ip + ":" + check_cast<std::string>(port);
            auto efind = std::find(
                public_endpoints_.begin(),
                public_endpoints_.end(),
                endpoint);
            if (efind == public_endpoints_.end()) {
                public_endpoints_.push_back(endpoint);
                vec_endpoints_.push_back(std::make_pair(ip, port));
            }
        }
    }

    // TODO(blueshi) replace node base something else, such as stability
    if (!vec_endpoints_.empty()) {
        bootstrap_cache_ptr_->SetCache(vec_endpoints_);
    }
}

void BootstrapCacheHelper::LoadBootstrapCache() {
    top::kadmlia::VecBootstrapEndpoint vec;
    if (!bootstrap_cache_ptr_->GetCache(vec)) {
        TOP_WARN("<blueshi> GetCache failed");
        return;
    }

    TOP_INFO("<blueshi> loading routing_table(%llu) cache:", kad_key_->GetServiceType());
    std::unique_lock<std::mutex> lock(public_endpoint_mutex_);
    public_endpoints_.clear();
    vec_endpoints_.clear();
    for (auto& ep : vec) {
        const std::string endpoint = ep.first + ":" + check_cast<std::string>(ep.second);
        auto efind = std::find(
            public_endpoints_.begin(),
            public_endpoints_.end(),
            endpoint);
        if (efind == public_endpoints_.end()) {
            TOP_INFO("<blueshi>   [%llu] endpoint(%s)",
                kad_key_->GetServiceType(), endpoint.c_str());
            public_endpoints_.push_back(endpoint);
            vec_endpoints_.push_back(ep);
        }
    }

    TOP_INFO("<blueshi>[%llu] LoadBootstrapCache success", kad_key_->GetServiceType());
}

}  // namespace kadmlia
}  // namespace top
