// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <memory>

#include "xkad/routing_table/routing_utils.h"

namespace top {

namespace kadmlia {

struct ClientNodeInfo {
public:
    ClientNodeInfo();
    explicit ClientNodeInfo(const std::string& id);
    ~ClientNodeInfo();

    std::string node_id;
    std::string public_ip;
    uint16_t public_port;
    uint64_t src_service_type;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientNodeInfo);
};

typedef std::shared_ptr<ClientNodeInfo> ClientNodeInfoPtr;

class ClientNodeManager {
public:
    static ClientNodeManager* Instance();
    int AddClientNode(ClientNodeInfoPtr node_ptr);
    void RemoveClientNode(const std::string& node_id);
    ClientNodeInfoPtr FindClientNode(const std::string& node_id);

private:
    ClientNodeManager();
    ~ClientNodeManager();

    std::map<std::string, ClientNodeInfoPtr> client_nodes_map_;
    std::mutex client_nodes_map_mutex_;

    DISALLOW_COPY_AND_ASSIGN(ClientNodeManager);
};

}  // namespace kadmlia

}  // namespace top
