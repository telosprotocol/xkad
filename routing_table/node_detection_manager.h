// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <memory>

#include "xpbase/base/top_timer.h"
#include "xkad/routing_table/routing_utils.h"

namespace top {

namespace kadmlia {

struct NodeInfo;
class RoutingTable;

class NodeDetectionManager {
public:
    explicit NodeDetectionManager(base::TimerManager* timer_manager, RoutingTable& routing_table);
    ~NodeDetectionManager();
    void Join();
    int AddDetectionNode(std::shared_ptr<NodeInfo> node_ptr);
    void RemoveDetection(const std::string& ip, uint16_t port);
    bool Detected(const std::string& id);

private:
    void DoTetection();
    int Handshake(std::shared_ptr<NodeInfo> node_ptr);

    std::map<std::string, std::shared_ptr<NodeInfo>> detection_nodes_map_;
    std::mutex detection_nodes_map_mutex_;
    std::map<std::string, std::shared_ptr<NodeInfo>> detected_nodes_map_;
    std::mutex detected_nodes_map_mutex_;
    RoutingTable& routing_table_;
    base::TimerManager* timer_manager_{nullptr};
    std::shared_ptr<base::TimerRepeated> timer_;
    bool destroy_{ false };

    DISALLOW_COPY_AND_ASSIGN(NodeDetectionManager);
};

}  // namespace kadmlia

}  // namespace top
