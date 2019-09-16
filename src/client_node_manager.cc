// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/client_node_manager.h"

#include "xbase/xutl.h"
#include "xkad/routing_table/routing_table.h"
#include "xpbase/base/top_log.h"

namespace top {

namespace kadmlia {

ClientNodeInfo::ClientNodeInfo()
        : node_id(),
        public_ip(),
        public_port(0),
        src_service_type(0) {}

ClientNodeInfo::ClientNodeInfo(const std::string& id)
        : node_id(id),
        public_ip(),
        public_port(0),
        src_service_type(0) {}

ClientNodeInfo::~ClientNodeInfo() {}

ClientNodeManager::ClientNodeManager()
        : client_nodes_map_(),
        client_nodes_map_mutex_() {}

ClientNodeManager::~ClientNodeManager() {}

ClientNodeManager* ClientNodeManager::Instance() {
    static ClientNodeManager ins;
    return &ins;
}

int ClientNodeManager::AddClientNode(ClientNodeInfoPtr node_ptr) {
    std::unique_lock<std::mutex> lock(client_nodes_map_mutex_);
    std::string key = node_ptr->node_id;
    client_nodes_map_[key] = node_ptr;
    return kKadSuccess;
}

void ClientNodeManager::RemoveClientNode(const std::string& node_id) {
    std::unique_lock<std::mutex> lock(client_nodes_map_mutex_);
    std::string key = node_id;
    auto iter = client_nodes_map_.find(key);
    if (iter != client_nodes_map_.end()) {
        client_nodes_map_.erase(iter);
    }
}

ClientNodeInfoPtr ClientNodeManager::FindClientNode(const std::string& node_id) {
    std::unique_lock<std::mutex> lock(client_nodes_map_mutex_);
    std::string key = node_id;
    auto iter = client_nodes_map_.find(key);
    if (iter != client_nodes_map_.end()) {
        return iter->second;
    }
    return nullptr;
}

}  // namespace kadmlia

}  // namespace top
