// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// routing table demo

#include <signal.h>
#include <iostream>
#include <thread>  // NOLINT

#include "xpbase/base/top_config.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"
#include "xtransport/transport.h"
#include "xtransport/udp_transport/udp_transport.h"
#include "xtransport/message_manager/multi_message_handler.h"
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/kad_message_handler.h"
#include "xkad/nat_detect/nat_manager_intf.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/routing_table/callback_manager.h"
#include "xgossip/include/gossip_utils.h"
#include "xgossip/include/gossip_bloomfilter_layer.h"
#include "xtransport/message_manager/message_manager_intf.h"

namespace top {
    
std::shared_ptr<base::KadmliaKey> global_xid;
uint32_t gloabl_platform_type = kPlatform;
std::string global_node_id = RandomString(256);
static kadmlia::KadMessageHandler kad_message_handler;
static const uint32_t kBroadcastMessageTest = gossip::kGossipMaxMessageType + 1;
std::shared_ptr<top::transport::MultiThreadHandler> multi_thread_message_handler = nullptr;
uint64_t global_b_time = 0;
std::mutex test_mutex;
static const uint32_t kTestNum = 2000000u;
std::atomic<uint32_t> receive_count(0);

std::shared_ptr<top::kadmlia::RoutingTable> CreateRoutingTable(
        top::transport::UdpTransportPtr udp_transport,
        const top::base::Config& config) {
    uint32_t zone_id = 0;
    if (!kadmlia::GetZoneIdFromConfig(config, zone_id)) {
        TOP_ERROR("get zone id from config failed!");
        return nullptr;
    }

    auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
    kad_key->set_xnetwork_id(kRoot);
    kad_key->set_zone_id(zone_id);
    kadmlia::LocalNodeInfoPtr local_node_ptr = kadmlia::CreateLocalInfoFromConfig(
        config,
        kad_key);
    if (!local_node_ptr) {
        TOP_WARN("create local_node_ptr for failed");
        return nullptr;
    }
    auto routing_table_ptr = std::make_shared<top::kadmlia::RoutingTable>(
            udp_transport,
            kNodeIdSize,
            local_node_ptr);
    if (!routing_table_ptr->Init()) {
        TOP_ERROR("init RoutingTable failed");
        return nullptr;
    }
    kad_message_handler.set_routing_ptr(routing_table_ptr);
    bool first_node = false;
    config.Get("node", "first_node", first_node);
    if (first_node) {
        return routing_table_ptr;
    }

    std::set<std::pair<std::string, uint16_t>> service_public_endpoints;
    top::kadmlia::GetPublicEndpointsConfig(config, service_public_endpoints);
    if (service_public_endpoints.empty()) {
        TOP_ERROR("has no bootstrap endpoint");
        return nullptr;
    }

    if (routing_table_ptr->MultiJoin(service_public_endpoints) != top::kadmlia::kKadSuccess) {
        TOP_ERROR("MultiJoin failed");
        return nullptr;
    }
    return routing_table_ptr;
}

int32_t recv(
        uint64_t from_xip_addr_low,
        uint64_t from_xip_addr_high,
        uint64_t to_xip_addr_low,
        uint64_t to_xip_addr_high,
        base::xpacket_t& packet,
        int32_t cur_thread_id,
        uint64_t timenow_ms,
        base::xendpoint_t* from_parent_end) {
    multi_thread_message_handler->HandleMessage(packet);
    return 0;
}

bool CreateTransport(
        const top::base::Config& config,
        transport::UdpTransportPtr& udp_transport,
        transport::UdpTransportPtr& nat_transport) {
    udp_transport.reset(new top::transport::UdpTransport());
    std::string local_ip;
    if (!config.Get("node", "local_ip", local_ip)) {
        TOP_ERROR("get node local_ip from config failed!");
        return false;
    }

    uint16_t local_port = 0;
    config.Get("node", "local_port", local_port);
    multi_thread_message_handler = std::make_shared<top::transport::MultiThreadHandler>();
    multi_thread_message_handler->Init();
    if (udp_transport->Start(
            local_ip,
            local_port,
            multi_thread_message_handler.get()) != top::kadmlia::kKadSuccess) {
        TOP_ERROR("start local udp transport failed!");
        return false;
    }
    std::cout << "udp_transport start: "
        << udp_transport->local_ip() << ":" << udp_transport->local_port()
        << std::endl;
    udp_transport->RegisterOfflineCallback(kadmlia::HeartbeatManagerIntf::OnHeartbeatCallback);
    udp_transport->register_on_receive_callback(std::bind(
            recv,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8));

    nat_transport.reset(new top::transport::UdpTransport());
    if (nat_transport->Start(
            local_ip,
            local_port != 0?(local_port +1):0,
            multi_thread_message_handler.get()) != top::kadmlia::kKadSuccess) {
        TOP_ERROR("start local udp transport failed!");
        return false;
    }

    nat_transport->register_on_receive_callback(std::bind(
            recv,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5,
            std::placeholders::_6,
            std::placeholders::_7,
            std::placeholders::_8));

    std::set<std::pair<std::string, uint16_t>> service_public_endpoints;
    top::kadmlia::GetPublicEndpointsConfig(config, service_public_endpoints);
    if (service_public_endpoints.empty()) {
        TOP_INFO("has no bootstrap endpoint");
        return false;
    }

    bool first_node = false;
    config.Get("node", "first_node", first_node);
    if (!top::kadmlia::NatManagerIntf::Instance()->Start(
            first_node,
            service_public_endpoints,
            multi_thread_message_handler.get(),
            udp_transport.get(),
            nat_transport.get())) {
        TOP_ERROR("blue nat nat manager start failed");
        return false;
    }
    return true;
}

void SignalCatch(int sig_no) {
    if (SIGTERM == sig_no || SIGINT == sig_no) {
        _Exit(0);
    }
}

void TestBroadcast(
        kadmlia::RoutingTablePtr& routing_table,
        transport::UdpTransportPtr& udp_transport) {
    auto b_time = GetCurrentTimeMsec();

    transport::protobuf::RoutingMessage local_global_message;
    auto node_ptr = routing_table->GetRandomNode();
    if (!node_ptr) {
        TOP_WARN("no node here.");
        return;
    }
    routing_table->SetFreqMessage(local_global_message);
    local_global_message.set_des_node_id(node_ptr->node_id);
    local_global_message.set_type(kBroadcastMessageTest);
    local_global_message.set_id(kadmlia::CallbackManager::MessageId());
    local_global_message.set_broadcast(true);
    auto gossip = local_global_message.mutable_gossip();
    gossip->set_neighber_count(3);
    gossip->set_stop_times(10);
    gossip->set_gossip_type(1);
    gossip->set_max_hop_num(10);
    gossip->set_evil_rate(0);

    std::string data;
    local_global_message.SerializeToString(&data);
    std::cout << "smallest protobuf broadcast size:" << data.size() << std::endl;


    transport::protobuf::RoutingMessage pbft_message;
    for (uint32_t i = 0; i < kTestNum; ++i) {
        /*
        transport::protobuf::RoutingMessage pbft_message;
        auto node_ptr = routing_table->GetRandomNode();
        if (!node_ptr) {
            TOP_WARN("no node here.");
            return;
        }
        routing_table->SetFreqMessage(pbft_message);
        pbft_message.set_des_node_id(node_ptr->node_id);
        pbft_message.set_type(kBroadcastMessageTest);
        pbft_message.set_id(kadmlia::CallbackManager::MessageId());
        pbft_message.set_broadcast(true);
        auto gossip = pbft_message.mutable_gossip();
        gossip->set_neighber_count(3);
        gossip->set_stop_times(10);
        gossip->set_gossip_type(1);
        gossip->set_max_hop_num(10);
        gossip->set_evil_rate(0);
        */
        pbft_message.Clear();
        pbft_message.CopyFrom(local_global_message);
        pbft_message.set_id(kadmlia::CallbackManager::MessageId());
//         routing_table->SendData(pbft_message, node_ptr);
        auto bloom_gossip_ptr = std::make_shared<gossip::GossipBloomfilterLayer>(udp_transport);
        bloom_gossip_ptr->Broadcast(
                routing_table->get_local_node_info()->id(),
                pbft_message,
                routing_table->nodes());
    }

    auto use_time_ms = double(GetCurrentTimeMsec() - b_time) / 1000.0;
    std::cout << "send " << kTestNum << " use time: " << use_time_ms
        << " sec. QPS: " << (uint32_t)((double)kTestNum / use_time_ms) << std::endl;
}

void TestPing(
        kadmlia::RoutingTablePtr& routing_table,
        transport::UdpTransportPtr& udp_transport) {
    auto b_time = GetCurrentTimeMsec();
    transport::protobuf::RoutingMessage local_global_message;
    auto node_ptr = routing_table->GetRandomNode();
    if (!node_ptr) {
        TOP_WARN("no node here.");
        return;
    }
    routing_table->SetFreqMessage(local_global_message);
    local_global_message.set_des_node_id(node_ptr->node_id);
    local_global_message.set_type(kBroadcastMessageTest);
    local_global_message.set_id(kadmlia::CallbackManager::MessageId());

    std::string data;
    local_global_message.SerializeToString(&data);
    std::cout << "smallest protobuf message size:" << data.size() << std::endl;
    for (uint32_t i = 0; i < kTestNum; ++i) {
        /*
        transport::protobuf::RoutingMessage pbft_message;
        auto node_ptr = routing_table->GetRandomNode();
        if (!node_ptr) {
            TOP_WARN("no node here.");
            return;
        }
        routing_table->SetFreqMessage(pbft_message);
        pbft_message.set_des_node_id(node_ptr->node_id);
        pbft_message.set_type(kBroadcastMessageTest);
        pbft_message.set_id(kadmlia::CallbackManager::MessageId());
        */

        //routing_table->SendData(pbft_message, node_ptr);  // send qps 16 w


        //routing_table->SendData(pbft_message, "127.0.0.1", 8900); // send qps 16.9w
        
        //std::string data;
        //pbft_message.SerializeToString(&data);

        //routing_table->SendData({data.begin(), data.end()}, "127.0.0.1", 8900); // send qps 16.9 w

        //std::string data;
        //local_global_message.SerializeToString(&data);
        udp_transport->SendData({ data.begin(), data.end() }, "127.0.0.1", 8900); // send qps 17w
        
        // if put  RoutingMessage outside, udp_transport->SendData send qps 21 w
    }

    auto use_time_ms = double(GetCurrentTimeMsec() - b_time) / 1000.0;
    std::cout << "send " << kTestNum << " use time: " << use_time_ms
        << " sec. QPS: " << (uint32_t)((double)kTestNum / use_time_ms) << std::endl;
}


void TestGossipMessageHandler(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (global_b_time == 0) {
        std::unique_lock<std::mutex> lock(test_mutex);
        if (global_b_time == 0) {
            global_b_time = GetCurrentTimeMsec();
        }
    }
    ++receive_count;
    if (receive_count % 10000 == 0) {
        auto use_time_ms = double(GetCurrentTimeMsec() - global_b_time) / 1000.0;
        std::cout << "receive " << receive_count << " use time: " << use_time_ms
            << " sec. QPS: " << (uint32_t)((double)receive_count / use_time_ms) << std::endl;
    }
}

}  // namespace top

int main(int argc,char** argv) {
    kad_message_handler.Init();
    if (signal(SIGTERM, top::SignalCatch) == SIG_ERR ||
            signal(SIGINT, top::SignalCatch) == SIG_ERR) {
        return 1;
    }

    const pid_t current_sys_process_id = getpid();
    std::string init_log_file("./log/kad.log");
    xinit_log(init_log_file.c_str(), true, true);
    xset_log_level(enum_xlog_level_error);
    top::base::Config config;
    
    bool send_broadcast = false;
    std::string config_file("./conf/kaddemo.conf");
    for (uint32_t i = 0; i < argc; ++i) {
        if (std::string(argv[i]).compare("-b") == 0) {
            send_broadcast = true;
        }
        if (std::string(argv[i]).compare("-c") == 0) {
            config_file = std::string(argv[i+1]);
        }
    }
    if (!config.Init(config_file)) {
        std::cout << "init config file failed: ./conf/kaddemo.conf" << std::endl;
        return 1;
    }
    
    top::kadmlia::CreateGlobalXid(config);
    top::transport::UdpTransportPtr udp_transport;
    top::transport::UdpTransportPtr nat_transport;
    if (!top::CreateTransport(config, udp_transport, nat_transport)) {
        assert(0);
    }

    // register gossip message handler
    top::transport::MessageManagerIntf::Instance()->RegisterMessageProcessor(
            top::kBroadcastMessageTest,
            top::TestGossipMessageHandler);
    top::kadmlia::NatManagerIntf::Instance()->SetNatTypeAndNotify(top::kadmlia::kNatTypePublic);
    auto routing_ptr = top::CreateRoutingTable(udp_transport, config);
    assert(routing_ptr);
    std::cout << "routing table start ok." << std::endl;

    if (send_broadcast) {
        // send gossip test
        top::TestBroadcast(routing_ptr, udp_transport);
        // send ping 
        //top::TestPing(routing_ptr, udp_transport);
    }

    while (true) {
        sleep(10);
    }
}
