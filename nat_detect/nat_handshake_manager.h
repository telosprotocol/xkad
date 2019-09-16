// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <memory>

#include "xtransport/transport.h"
#include "xpbase/base/xbyte_buffer.h"
#include "xpbase/base/top_timer.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/routing_table/node_info.h"

namespace top {
namespace kadmlia {

class NatHandshakeManager {
    struct NatDetectStruct {
        std::string ip;
        uint16_t port;
        transport::Transport* transport;
        uint16_t detect_port;
        int32_t message_type;
        int32_t delay_count{0};
        int32_t detect_count{0};
    };

public:
    NatHandshakeManager(base::TimerManager* timer_manager);
    ~NatHandshakeManager();
    void AddDetection(
            const std::string& ip,
            uint16_t port,
            transport::Transport* transport,
            uint16_t detect_port,
            int32_t message_type,
            int32_t delay_ms);
    void RemoveDetection(const std::string& ip, uint16_t port);
    bool Start();
    void Stop();
    void SetName(const std::string& name);

private:
    void DetectProc();
    int DoHandshake(
            transport::Transport* transport,
            const std::string& peer_ip,
            uint16_t detect_port,
            int32_t message_type);

    std::string name_{"<bluenat>"};
    std::atomic<bool> started_{false};
    std::map<std::string, std::shared_ptr<NatDetectStruct>> peers_;
    std::mutex mutex_;
    base::TimerManager* timer_manager_{nullptr};
    std::shared_ptr<base::TimerRepeated> timer_;

    DISALLOW_COPY_AND_ASSIGN(NatHandshakeManager);
};

}  // namespace kadmlia
}  // namespace top
