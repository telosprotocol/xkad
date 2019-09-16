// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/dynamic_xip_manager.h"

//#include "xbase/xutl.h"
#include "xkad/routing_table/client_node_manager.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/xip_parser.h"

#define d_get_process_id(id) ( ((id) & 0xF00000) >> 16 )
#define d_get_router_id(id) ( ((id) & 0x0F0000) >> 16 )
#define d_get_switch_id(id) ( ((id) & 0xFF00) >> 8 )
#define d_get_local_id(id) ( ((id) & 0xFF) )

namespace top {

namespace kadmlia {

DynamicXipManager::DynamicXipManager()
        : dy_xip_map_(),
        dy_xip_map_mutex_() {}

DynamicXipManager::~DynamicXipManager() {}

// dispatch xip for bootstrap node
std::string DynamicXipManager::DispatchDynamicXip(const base::XipParser&  local_xip) {
    base::XipParser dynamic_xip(local_xip);
    dynamic_xip.set_xip_type(enum_xip_type_dynamic);

    // TODO(smaug) not sure about bit operation
    uint32_t now_id = ++atom_xip_manager_id_;
    dynamic_xip.set_process_id(d_get_process_id(now_id));
    dynamic_xip.set_router_id(d_get_router_id(now_id));
    dynamic_xip.set_switch_id(d_get_switch_id(now_id));
    dynamic_xip.set_local_id(d_get_local_id(now_id));
    return dynamic_xip.xip();
}

int DynamicXipManager::AddClientNode(const std::string& dy_xip, ClientNodeInfoPtr node_ptr) {
    std::unique_lock<std::mutex> lock(dy_xip_map_mutex_);
    dy_xip_map_[dy_xip] = node_ptr;
    return kKadSuccess;
}

void DynamicXipManager::RemoveClientNode(const std::string& dy_xip) {
    std::unique_lock<std::mutex> lock(dy_xip_map_mutex_);
    std::string key = dy_xip;
    auto iter = dy_xip_map_.find(key);
    if (iter != dy_xip_map_.end()) {
        dy_xip_map_.erase(iter);
    }
}

ClientNodeInfoPtr DynamicXipManager::FindClientNode(const std::string& dy_xip) {
    std::unique_lock<std::mutex> lock(dy_xip_map_mutex_);
    std::string key = dy_xip;
    auto iter = dy_xip_map_.find(key);
    if (iter != dy_xip_map_.end()) {
        return iter->second;
    }
    return nullptr;
}

}  // namespace kadmlia

}  // namespace top
