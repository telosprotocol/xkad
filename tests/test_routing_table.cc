// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
#include "xtransport/message_manager/multi_message_handler.h"
#include "node_mgr.h"
#define private public
#define protected public
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/local_node_info.h"
#include "xkad/routing_table/kad_message_handler.h"
#include "xkad/nat_detect/nat_manager_intf.h"

namespace top {

namespace kadmlia {

namespace test {

class TestRoutingTable : public testing::Test {
public:
    static void SetUpTestCase() {
        node_mgr_ = std::make_shared<NodeMgr>();
        node_mgr_->Init(true, "<firstnode>");
        kad_message_handler_.Init();

        base::Config config;
        ASSERT_TRUE(config.Init("/tmp/test_routing_table.conf"));
        std::string local_ip;
        ASSERT_TRUE(config.Get("node", "local_ip", local_ip));
        uint16_t local_port = 0;
        ASSERT_TRUE(config.Get("node", "local_port", local_port));
        NatManagerIntf::Instance()->SetNatType(kNatTypePublic);
        udp_transport_.reset(new top::transport::UdpTransport());
        thread_message_handler_ = std::make_shared<transport::MultiThreadHandler>();
        thread_message_handler_->Init();
        ASSERT_TRUE(udp_transport_->Start(
                local_ip,
                local_port,
                thread_message_handler_.get()) == top::kadmlia::kKadSuccess);
        auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
        kad_key->set_xnetwork_id(top::kRoot);
        uint32_t zone_id = 0;
        ASSERT_TRUE(GetZoneIdFromConfig(config, zone_id));
        kad_key->set_zone_id(zone_id);
        auto local_node_ptr = CreateLocalInfoFromConfig(config, kad_key);
        const uint64_t service_type = kRoot;
        local_node_ptr->set_service_type(service_type);
        routing_table_ptr_.reset(new top::kadmlia::RoutingTable(
                udp_transport_,
                kNodeIdSize,
                local_node_ptr));
        ASSERT_TRUE(routing_table_ptr_->Init());
        ASSERT_TRUE(routing_table_ptr_->StartBootstrapCacheSaver());
        kad_message_handler_.set_routing_ptr(routing_table_ptr_);
        // std::string peer(node_mgr_->LocalIp() + ":" + std::to_string(node_mgr_->RealLocalPort()));
        // top::base::LineParser line_split(peer.c_str(), ':', peer.size());
        // ASSERT_EQ(line_split.Count(), 2);
        std::set<std::pair<std::string, uint16_t>> boot_endpoints{
            { node_mgr_->LocalIp(), node_mgr_->RealLocalPort() }
        };

        int res = routing_table_ptr_->MultiJoin(boot_endpoints);
        ASSERT_EQ(res, kKadSuccess);
        ASSERT_TRUE(routing_table_ptr_->IsJoined());
        {
            std::unique_lock<std::mutex> lock(routing_table_ptr_->nodes_mutex_);
            ASSERT_FALSE(routing_table_ptr_->nodes_.empty());
        }
    }

    static void TearDownTestCase() {
        if (routing_table_ptr_) {
            routing_table_ptr_->UnInit();
            routing_table_ptr_ = nullptr;
        }

        if (udp_transport_) {
            udp_transport_->Stop();
            udp_transport_ = nullptr;
        }

        if (thread_message_handler_) {
            thread_message_handler_ = nullptr;
        }
    }

    virtual void SetUp() {
        ASSERT_TRUE(routing_table_ptr_);
    }

    virtual void TearDown() {
    }

    static std::shared_ptr<NodeMgr> node_mgr_;
    static std::shared_ptr<RoutingTable> routing_table_ptr_;
    static top::transport::UdpTransportPtr udp_transport_;
    static std::shared_ptr<transport::MultiThreadHandler> thread_message_handler_;
    static kadmlia::KadMessageHandler kad_message_handler_;
};

std::shared_ptr<NodeMgr> TestRoutingTable::node_mgr_;
std::shared_ptr<RoutingTable> TestRoutingTable::routing_table_ptr_ = nullptr;
top::transport::UdpTransportPtr TestRoutingTable::udp_transport_ = nullptr;
std::shared_ptr<transport::MultiThreadHandler> TestRoutingTable::thread_message_handler_ = nullptr;
kadmlia::KadMessageHandler TestRoutingTable::kad_message_handler_;

TEST_F(TestRoutingTable, WakeBootstrap) {
    NodeInfoPtr first_add_ptr;
    first_add_ptr.reset(new NodeInfo("test"));
    routing_table_ptr_->WakeBootstrap();
}

TEST_F(TestRoutingTable, FindClosestNodes) {
    std::vector<NodeInfoPtr> find_nodes;
    routing_table_ptr_->FindClosestNodes(1, 10, find_nodes);
}

TEST_F(TestRoutingTable, HeartbeatProc) {
    routing_table_ptr_->HeartbeatProc();
}

TEST_F(TestRoutingTable, HeartbeatCheckProc) {
    routing_table_ptr_->HeartbeatCheckProc();
}

TEST_F(TestRoutingTable, ResetNodeHeartbeat) {
    std::string id = "1234";
    routing_table_ptr_->ResetNodeHeartbeat(id);
}

TEST_F(TestRoutingTable, RecursiveSend) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadAck);
    message.set_retry(1);
    int retry_times = 2;
    routing_table_ptr_->RecursiveSend(message, retry_times);
}

TEST_F(TestRoutingTable, ClosestToTarget) {
    std::string local_id = routing_table_ptr_->local_node_ptr_->id();
    std::string local_id_hex = top::HexEncode(local_id);
    auto node_size = routing_table_ptr_->local_node_ptr_->id().size();
    local_id_hex[node_size - 1] = 'f';
    std::string tmp_id = HexDecode(local_id_hex);
    bool closest = false;
    int res = routing_table_ptr_->ClosestToTarget(tmp_id, closest);
    // TODO(smaug)
    //ASSERT_EQ(res, kKadSuccess);
    ASSERT_TRUE(closest);
    local_id_hex[0] = 'f';
    closest = false;

    NodeInfoPtr closest_node;
    {
        std::unique_lock<std::mutex> lock(routing_table_ptr_->nodes_mutex_);
        if (routing_table_ptr_->nodes_.empty()) {
            ASSERT_TRUE(false);
        }
        closest_node = routing_table_ptr_->nodes_[0];
    }

    res = routing_table_ptr_->ClosestToTarget(closest_node->node_id, closest);
    ASSERT_EQ(res, kKadSuccess);
    ASSERT_FALSE(closest);
}

TEST_F(TestRoutingTable, SendHeartbeat) {
    NodeInfoPtr closest_node;
    {
        std::unique_lock<std::mutex> lock(routing_table_ptr_->nodes_mutex_);
        if (routing_table_ptr_->nodes_.empty()) {
            ASSERT_TRUE(false);
        }
        closest_node = routing_table_ptr_->nodes_[0];
    }

    routing_table_ptr_->SendHeartbeat(closest_node,kRoot);
}

TEST_F(TestRoutingTable, DropNode) {
    NodeInfoPtr drop_node;
    {
        std::unique_lock<std::mutex> lock(routing_table_ptr_->nodes_mutex_);
        if (routing_table_ptr_->nodes_.empty()) {
            ASSERT_TRUE(false);
        }
        drop_node = routing_table_ptr_->nodes_[0];
    }
    int res = routing_table_ptr_->DropNode(drop_node);
    ASSERT_EQ(res, kKadSuccess);
}

TEST_F(TestRoutingTable, Rejoin) {
    routing_table_ptr_->joined_ = true;
    {
        std::unique_lock<std::mutex> lock(routing_table_ptr_->nodes_mutex_);
        routing_table_ptr_->nodes_.clear();
        routing_table_ptr_->node_id_map_.clear();
    }
    routing_table_ptr_->Rejoin();
    SleepUs(1 * 1000 * 1000);
    ASSERT_TRUE(routing_table_ptr_->joined_);
    {
        std::unique_lock<std::mutex> lock(routing_table_ptr_->nodes_mutex_);
        ASSERT_FALSE(routing_table_ptr_->nodes_.empty());
    }
}

TEST_F(TestRoutingTable, GetPubEndpoints) {
    for (int i = 0; i < 1000; ++i) {
        std::string id = GenRandomID("CN", "VPN");
        NodeInfoPtr node_ptr;
        node_ptr.reset(new NodeInfo(id));
        node_ptr->local_ip = "127.0.0.1";
        node_ptr->local_port = 1000 + i;
        node_ptr->public_ip = "127.0.0.1";
        node_ptr->public_port = 1000 + i;
        if (routing_table_ptr_->CanAddNode(node_ptr)) {
            routing_table_ptr_->AddNode(node_ptr);
        }
    }

    std::vector<std::string> public_endpoints;
    routing_table_ptr_->GetPubEndpoints(public_endpoints);
}

TEST_F(TestRoutingTable, GetRandomNode) {
    routing_table_ptr_->GetRandomNode();
}

TEST_F(TestRoutingTable, GetRangeNodes_uint64) {
    const uint64_t min = 1;
    const uint64_t max = 2;
    std::vector<NodeInfoPtr> vec;
    routing_table_ptr_->GetRangeNodes(min, max, vec);
}

TEST_F(TestRoutingTable, GetRangeNodes_uint32) {
    const uint32_t min = 1;
    const uint32_t max = 2;
    std::vector<NodeInfoPtr> vec;
    routing_table_ptr_->GetRangeNodes(min, max, vec);
}

TEST_F(TestRoutingTable, GetSelfIndex) {
    routing_table_ptr_->GetSelfIndex();
}

TEST_F(TestRoutingTable, ClosestToTarget_2) {
    const std::string target;
    const std::set<std::string> exclude;
    bool closest = true;
    ASSERT_EQ(kKadFailed, routing_table_ptr_->ClosestToTarget(target, exclude, closest));
}

TEST_F(TestRoutingTable, FindLocalNode) {
    std::string node_id;
    ASSERT_EQ(nullptr, routing_table_ptr_->FindLocalNode(node_id));
}

TEST_F(TestRoutingTable, FindCloseNodesWithEndpoint) {
    const std::string des_node_id;
    const std::pair<std::string, uint16_t> boot_endpoints;
    routing_table_ptr_->FindCloseNodesWithEndpoint(des_node_id, boot_endpoints);
}

TEST_F(TestRoutingTable, HandleMessage) {
    transport::protobuf::RoutingMessage message;
    base::xpacket_t packet;
    routing_table_ptr_->HandleMessage(message, packet);
}

TEST_F(TestRoutingTable, TellNeighborsDropAllNode) {
    routing_table_ptr_->TellNeighborsDropAllNode();
}

TEST_F(TestRoutingTable, SendDropNodeRequest) {
    const std::string id;
    routing_table_ptr_->SendDropNodeRequest(id);
}

TEST_F(TestRoutingTable, HandleNodeQuit) {
    transport::protobuf::RoutingMessage message;
    base::xpacket_t packet;
    routing_table_ptr_->HandleNodeQuit(message, packet);
}

TEST_F(TestRoutingTable, MultiJoinAsync) {
    const std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    routing_table_ptr_->MultiJoinAsync(boot_endpoints);
}

TEST_F(TestRoutingTable, FindNeighbours) {
    routing_table_ptr_->FindNeighbours();
}

TEST_F(TestRoutingTable, SortNodesByTargetXid) {
    const std::string target_xid;
    std::vector<NodeInfoPtr> nodes;
    routing_table_ptr_->SortNodesByTargetXid(target_xid, nodes);
}

TEST_F(TestRoutingTable, SortNodesByTargetXip) {
    const std::string target_xip;
    int number = 3;
    routing_table_ptr_->SortNodesByTargetXip(target_xip, number);
}

TEST_F(TestRoutingTable, SupportSecurityJoin) {
    routing_table_ptr_->SupportSecurityJoin();
}

TEST_F(TestRoutingTable, CheckAndSendRelay) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadAck);
    routing_table_ptr_->CheckAndSendRelay(message);
}

TEST_F(TestRoutingTable, CheckAndSendMultiRelay) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadAck);
    routing_table_ptr_->CheckAndSendMultiRelay(message);
}

TEST_F(TestRoutingTable, SetMultiRelayMsg) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadAck);
    transport::protobuf::RoutingMessage res_message;
    routing_table_ptr_->SetMultiRelayMsg(message, res_message);
}

TEST_F(TestRoutingTable, SmartSendReply) {
    transport::protobuf::RoutingMessage res_message;
    res_message.set_type(kKadAck);
    routing_table_ptr_->SmartSendReply(res_message);
}

TEST_F(TestRoutingTable, SmartSendReply_2) {
    transport::protobuf::RoutingMessage res_message;
    res_message.set_type(kKadAck);
    bool add_hop = true;
    routing_table_ptr_->SmartSendReply(res_message, add_hop);
}

TEST_F(TestRoutingTable, GetBootstrapCache) {
    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    routing_table_ptr_->GetBootstrapCache(boot_endpoints);
}

TEST_F(TestRoutingTable, AddHeartbeatInfo) {
    const std::string key;
    const std::string value;
    routing_table_ptr_->AddHeartbeatInfo(key, value);
}

TEST_F(TestRoutingTable, ClearHeartbeatInfo) {
    routing_table_ptr_->ClearHeartbeatInfo();
}

TEST_F(TestRoutingTable, RegisterHeartbeatInfoCallback) {
    auto cb = [](std::map<std::string, std::string>&){};
    routing_table_ptr_->RegisterHeartbeatInfoCallback(cb);
}

TEST_F(TestRoutingTable, UnRegisterHeartbeatInfoCallback) {
    routing_table_ptr_->UnRegisterHeartbeatInfoCallback();
}

}  // namespace test

}  // namespace kadmlia

}  // namespace top
