// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <set>
#include <memory>
#include "nat_defines.h"
#include "xtransport/transport_fwd.h"

namespace top {
namespace kadmlia {

class NatManagerIntf {
    using MessagePtr = std::shared_ptr<transport::protobuf::RoutingMessage>;
    using PacketPtr = std::shared_ptr<base::xpacket_t>;
public:
    static NatManagerIntf* Instance();
    virtual ~NatManagerIntf() {}
    virtual bool GetLocalNatType(int32_t& nat_type) = 0;
    virtual void PushMessage(MessagePtr message_ptr, PacketPtr packet_ptr) = 0;
    virtual void PushMessage(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet) = 0;
    virtual bool Start(
            bool first_node,
            const std::set<std::pair<std::string, uint16_t>>& boot_endpoints,
            transport::MultiThreadHandler* messager_handler,
            transport::Transport* transport,
            transport::Transport* nat_transport) = 0;
    virtual void Stop() = 0;
    virtual void SetNatType(NatType nat_type) = 0;
};

}  // namespace kadmlia
}  // namespace top
