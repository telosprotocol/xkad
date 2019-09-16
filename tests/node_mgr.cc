// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node_mgr.h"
#include "xpbase/base/top_log_name.h"

namespace top {
namespace kadmlia {
namespace test {

bool NodeMgr::Init(bool first_node, const std::string& name_prefix) {
    TOP_FATAL_NAME("NodeMgr::Init");
    timer_manager_impl_ = base::TimerManager::CreateInstance();
    timer_manager_impl_->Start(1);

    first_node_ = first_node;
    name_prefix_ = name_prefix;
    name_ = name_prefix + name_;

    // basic config
    const bool client_mode = false;
    const std::string idtype = "";
    const std::string str_key = name_;
    const bool hash_tag = true;
    auto kad_key = std::make_shared<base::PlatformKadmliaKey>(str_key, hash_tag);

    // init nat manager
    nat_manager_ = std::make_shared<NatManager>();
    nat_manager_->name_ = name_prefix_ + nat_manager_->name_;
    nat_manager_->timer_manager_ = timer_manager_impl_.get();

    // register message handler
    kad_message_handler_.message_manager_ = &this->message_manager_;
    kad_message_handler_.nat_manager_ = this->nat_manager_.get();
    kad_message_handler_.Init();

    // init message process threads
    thread_message_handler_ = std::make_shared<transport::MultiThreadHandler>();
    thread_message_handler_->m_woker_threads_count = 1;
    thread_message_handler_->Init();
    for (auto& thread : thread_message_handler_->m_worker_threads) {
        thread->message_manager_ = &this->message_manager_;
    }

    // init udp socket
    {
        udp_transport_.reset(new top::transport::UdpTransport());
        auto ret = udp_transport_->Start(local_ip_, 0, thread_message_handler_.get());
        if (ret != top::kadmlia::kKadSuccess) {
            TOP_FATAL_NAME("udp_transport start failed!");
            return false;
        }
        udp_transport_->RegisterOfflineCallback(kadmlia::HeartbeatManagerIntf::OnHeartbeatCallback);
        real_local_port_ = udp_transport_->local_port();
        TOP_FATAL_NAME("real local port: %d", (int)real_local_port_);
    }

    // init nat
    if (first_node_) {
        auto ret = NatDetect("", 0);
        assert(ret);
    }

    // init local node info
    auto local_node_ptr = std::make_shared<LocalNodeInfo>();
    if (!local_node_ptr->Init(
            local_ip_,
            local_port_,
            first_node,
            client_mode,
            idtype,
            kad_key,
            kad_key->xnetwork_id(),
            kRoleInvalid)) {
        TOP_FATAL_NAME("local_node_ptr init failed!");
        return false;
    }
    local_node_ptr->set_service_type(kRoot);

    // init routing table
    routing_table_ptr_.reset(new top::kadmlia::RoutingTable(
            udp_transport_,
            kNodeIdSize,
            local_node_ptr));
    routing_table_ptr_->name_ = name_prefix + routing_table_ptr_->name_;
    routing_table_ptr_->timer_manager_ = timer_manager_impl_.get();

    if (!routing_table_ptr_->Init()) {
        TOP_FATAL_NAME("routing_table_ptr init failed!");
        return false;
    }

    kad_message_handler_.set_routing_ptr(routing_table_ptr_);
    TOP_FATAL_NAME("init success");
    return true;
}

NodeMgr::NodeMgr() {
    // TOP_FATAL_NAME("---------------------------------------- new NodeMgr(%p)", this);
}

NodeMgr::~NodeMgr() {
    // TOP_FATAL_NAME("---------------------------------------- delete NodeMgr(%p)", this);
    if (nat_transport_) {
        nat_transport_->Stop();
    }

    if (udp_transport_) {
        udp_transport_->Stop();
    }

    // TODO: merge nat transport to nat manager!
    if (nat_manager_) {
        nat_manager_->Stop();
    }

    if (thread_message_handler_) {
        thread_message_handler_->Stop();
    }

    if (routing_table_ptr_) {
        routing_table_ptr_->UnInit();
    }

    timer_manager_impl_->Stop();

    // sleep(5);
}

bool NodeMgr::NatDetect(const std::string& peer_ip, uint16_t peer_port) {
    {
        nat_transport_.reset(new top::transport::UdpTransport());
        auto ret = nat_transport_->Start(local_ip_, 0, thread_message_handler_.get());
        if (ret != top::kadmlia::kKadSuccess) {
            TOP_FATAL_NAME("nat_transport start failed!");
            return false;
        }
    }

    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    boot_endpoints.insert({peer_ip, peer_port});
    transport::MultiThreadHandler* messager_handler = nullptr;
    if (!nat_manager_->Start(
            first_node_,
            boot_endpoints,
            messager_handler,
            udp_transport_.get(),
            nat_transport_.get())) {
        TOP_FATAL_NAME("nat detect failed");
        return false;
    }

    int32_t nat_type = kNatTypeUnknown;
    nat_manager_->GetLocalNatType(nat_type);
    TOP_FATAL_NAME("nat detect over: %d", nat_type);
    return true;
}

bool NodeMgr::JoinRt(const std::string& peer_ip, uint16_t peer_port) {
    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    boot_endpoints.insert({peer_ip, peer_port});
    int ret = routing_table_ptr_->MultiJoin(boot_endpoints);
    if (ret != kKadSuccess) {
        TOP_FATAL_NAME("join failed");
        return false;
    }

    TOP_FATAL_NAME("join ok");
    return true;
}

std::string NodeMgr::LocalIp() {
    return local_ip_;
}

uint16_t NodeMgr::RealLocalPort() {
    return real_local_port_;
}

void NodeMgr::HandleMessage(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    message_manager_.HandleMessage(message, packet);
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
