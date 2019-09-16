// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdio.h>
#include <string.h>

#include <string>
#include <memory>
#include <fstream>

#include <gtest/gtest.h>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"
#include "xtransport/udp_transport/udp_transport.h"
#include "xtransport/src/message_manager.h"
#define private public
#define protected public
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/bootstrap_cache_helper.h"
#include "xkad/routing_table/local_node_info.h"
#include "xkad/routing_table/kad_message_handler.h"
#include "xtransport/message_manager/multi_message_handler.h"
#include "xkad/src/nat_manager.h"
#undef private
#undef protected
#include "xpbase/base/top_timer.h"

namespace top {
namespace kadmlia {
namespace test {

class NodeMgr {
public:
    NodeMgr();
    ~NodeMgr();
    bool Init(bool first_node, const std::string& name_prefix);
    bool NatDetect(const std::string& peer_ip, uint16_t peer_port);
    bool JoinRt(const std::string& peer_ip, uint16_t peer_port);
    std::string LocalIp();
    uint16_t RealLocalPort();
    void HandleMessage(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);

protected:
    std::shared_ptr<base::TimerManager> timer_manager_impl_;

    transport::UdpTransportPtr nat_transport_;
    std::shared_ptr<NatManager> nat_manager_;

    kadmlia::KadMessageHandler kad_message_handler_;
    transport::MessageManager message_manager_;
    std::shared_ptr<transport::MultiThreadHandler> thread_message_handler_;
    transport::UdpTransportPtr udp_transport_;
    std::shared_ptr<RoutingTable> routing_table_ptr_;
    bool first_node_{false};
    std::string name_prefix_;
    std::string name_{"<bluenm>"};
    const std::string local_ip_{"127.0.0.1"};
    const uint16_t local_port_{0};
    uint16_t real_local_port_{0};
};

}  // namespace test
}  // namespace kadmlia
}  // namespace top
