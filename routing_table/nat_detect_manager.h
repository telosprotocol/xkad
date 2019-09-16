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
#include "xkad/routing_table/node_info.h"

namespace top {

namespace kadmlia {

struct NodeInfo;
class RoutingTable;
class NatDetectSocket;

class NatDetectManager {
    struct NatDetectStruct {
        std::string ip;
        uint16_t port;
        uint16_t detect_port;
        int32_t message_type;
        int32_t delay_count{0};
        int32_t detect_count{0};
    };

public:
    NatDetectManager(RoutingTable* routing_table, int64_t service_type);
    ~NatDetectManager();
    void AddDetection(
            const std::string& ip,
            uint16_t port,
            uint16_t detect_port,
            int32_t message_type,
            int32_t delay_ms);
    void RemoveDetection(const std::string& ip, uint16_t port);
    bool Start();
    bool Stop();

private:
    void DetectProc();
    int DoHandshake(
        const std::string& peer_ip,
        uint16_t detect_port,
        int32_t message_type);

    RoutingTable* routing_table_{nullptr};
    int64_t service_type_{-1};
    std::map<std::string, std::shared_ptr<NatDetectStruct>> peers_;
    std::mutex mutex_;    
    std::shared_ptr<base::TimerRepeated> timer_{std::make_shared<base::TimerRepeated>()};

    DISALLOW_COPY_AND_ASSIGN(NatDetectManager);
};

}  // namespace kadmlia

}  // namespace top
