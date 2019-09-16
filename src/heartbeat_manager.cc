// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/heartbeat_manager.h"
#include "xpbase/base/top_log.h"

namespace top {
namespace kadmlia {

HeartbeatManagerIntf* HeartbeatManagerIntf::Instance() {
    static HeartbeatManager ins;
    return &ins;
}

void HeartbeatManagerIntf::OnHeartbeatCallback(const std::string& ip, uint16_t port) {
    HeartbeatManagerIntf::Instance()->OnHeartbeatFailed(ip, port);
}

// ----------------------------------------------------------------
void HeartbeatManager::Register(const std::string& name, OfflineCallback cb) {
    std::unique_lock<std::mutex> lock(mutex_);
    vec_cb_.push_back(cb);
    TOP_INFO("[ht_cb] register %s", name.c_str());
}

void HeartbeatManager::OnHeartbeatFailed(const std::string& ip, uint16_t port) {
    TOP_INFO("[ht_cb] %s:%d heartbeat failed", ip.c_str(), (int)port);
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto& cb : vec_cb_) {
        cb(ip, port);
    }
}

}  // namespace kadmlia
}  // namespace top
