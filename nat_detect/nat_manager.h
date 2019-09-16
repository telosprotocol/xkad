// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xkad/nat_detect/nat_manager_intf.h"

#include <stdint.h>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <memory>
#include <set>
#include <utility>
#include <string>
#include <condition_variable>

#include "xbase/xpacket.h"
#include "xtransport/transport.h"
#include "xtransport/proto/transport.pb.h"
#include "xpbase/base/xbyte_buffer.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_timer.h"
#include "xkad/proto/kadmlia.pb.h"
#include "xkad/nat_detect/nat_defines.h"
#include "xkad/nat_detect/nat_handshake_manager.h"

namespace top {

namespace transport {
    class MultiThreadHandler;
}

namespace kadmlia {

class NatManager : public NatManagerIntf {
    using MessagePtr = std::shared_ptr<transport::protobuf::RoutingMessage>;
    using PacketPtr = std::shared_ptr<base::xpacket_t>;
    class MessageQueue {
    public:
        void PushMessage(MessagePtr message_ptr, PacketPtr packet_ptr);
        bool GetMessage(MessagePtr& message_ptr, PacketPtr& packet_ptr);
        void Clear();
        void SetName(const std::string& name);

    private:
        std::string name_{"<bluenat>"};
        std::mutex mutex_;
        std::condition_variable condv_;
        std::deque<std::pair<MessagePtr, PacketPtr>> queue_;
    };

public:
    NatManager();
    ~NatManager();
    bool GetLocalNatType(int32_t& nat_type);
    void PushMessage(MessagePtr message_ptr, PacketPtr packet_ptr);
    void PushMessage(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    bool Start(
            bool first_node,
            const std::set<std::pair<std::string, uint16_t>>& boot_endpoints,
            transport::MultiThreadHandler*,
            transport::Transport* transport,
            transport::Transport* nat_transport);
    void Stop();
    void SetNatType(NatType nat_type);

private:
    void HandleNatDetectRequest(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleNatDetectResponse(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleNatDetectHandshake2Node(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleNatDetectHandshake2Boot(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleNatDetectFinish(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void SendNatDetectRequest(
            const std::string& peer_ip,
            uint16_t peer_port);
    void SendNatDetectFinish(
            const std::string& peer_ip,
            uint16_t peer_port);
    bool MultiNatDetect(const std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    void ThreadProc();
    void ThreadJoin();
    xbyte_buffer_t SerializeMessage(const transport::protobuf::RoutingMessage& message);
    void OnTimerNatDetectTimeout();
    bool SetNatTypeAndNotify(NatType nat_type);

private:
    std::string name_{"<bluenat>"};
    std::atomic<bool> started_{false};
    std::atomic<bool> stopped_{false};
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<int32_t> nat_type_{kNatTypePublic};
    MessageQueue message_queue_;
    std::shared_ptr<NatHandshakeManager> nat_handshake_manager_;
    base::TimerManager* timer_manager_{base::TimerManager::Instance()};
    std::shared_ptr<base::Timer> timer_;
    transport::Transport* transport_{nullptr};
    transport::Transport* nat_transport_;
    std::atomic<bool> thread_destroy_{false};
    std::shared_ptr<std::thread> thread_;

private:
    DISALLOW_COPY_AND_ASSIGN(NatManager);
};

}  // namespace kadmlia
}  // namespace top
