// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <map>

#include "xbase/xlog.h"
#include "xbase/xobject.h"
#include "xbase/xthread.h"
#include "xbase/xtimer.h"
#include "xbase/xdata.h"
#include "xbase/xpacket.h"
#include "xbase/xsocket.h"
#include "xbase/xutl.h"

#include "xtransport/proto/transport.pb.h"
#include "xkad/proto/kadmlia.pb.h"
#include "xkad/routing_table/routing_utils.h"
#include "xwrouter/register_message_handler.h"
#include "xtransport/message_manager/message_manager_intf.h"
#include "xkad/nat_detect/nat_manager_intf.h"

namespace top {

namespace kadmlia {

class RoutingTable;
class ThreadHandler;
typedef std::shared_ptr<RoutingTable> RoutingTablePtr;

class KadMessageHandler {
public:
    KadMessageHandler();
    ~KadMessageHandler();
    void Init();
    void set_routing_ptr(std::shared_ptr<RoutingTable> routing_ptr);
    std::shared_ptr<RoutingTable> routing_ptr() { return routing_ptr_; }

private:
    void AddBaseHandlers();
    int HandleClientMessage(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleConnectRequest(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleHandshake(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleBootstrapJoinRequest(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleBootstrapJoinResponse(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleFindNodesRequest(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleFindNodesResponse(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void SendAck(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleHeartbeatRequest(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    void HandleHeartbeatResponse(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);

    std::shared_ptr<RoutingTable> routing_ptr_;
    transport::MessageManagerIntf* message_manager_{transport::MessageManagerIntf::Instance()};
    kadmlia::NatManagerIntf* nat_manager_{kadmlia::NatManagerIntf::Instance()};
};

}  // namespace kadmlia

}  // namespace top
