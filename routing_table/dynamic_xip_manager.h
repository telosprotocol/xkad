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

namespace base {
class XipParser;
};

namespace kadmlia {

struct ClientNodeInfo;
typedef std::shared_ptr<ClientNodeInfo> ClientNodeInfoPtr;

class DynamicXipManager {
public:
    int AddClientNode(const std::string& dy_xip, ClientNodeInfoPtr node_ptr);
    void RemoveClientNode(const std::string& dy_xip);
    ClientNodeInfoPtr FindClientNode(const std::string& dy_xip);
    std::string DispatchDynamicXip(const base::XipParser&  local_xip);
public:
    DynamicXipManager();
    ~DynamicXipManager();

    // key is dynamic xip str
    std::map<std::string, ClientNodeInfoPtr> dy_xip_map_;
    std::mutex dy_xip_map_mutex_;
    std::atomic<uint32_t> atom_xip_manager_id_;

    DISALLOW_COPY_AND_ASSIGN(DynamicXipManager);
};

typedef std::shared_ptr<DynamicXipManager> DynamicXipManagerPtr;

}  // namespace kadmlia

}  // namespace top
