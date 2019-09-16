// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/nat_detect/nat_manager.h"

#include "xtransport/message_manager/multi_message_handler.h"
#include "xkad/nat_detect/nat_handshake_manager.h"
#include "xkad/nat_detect/nat_defines.h"
// #include "xkad/nat_detect/nat_log.h"
#include "xkad/routing_table/callback_manager.h"
#include "xpbase/base/top_log_name.h"

namespace top {
namespace kadmlia {

void NatManager::MessageQueue::PushMessage(MessagePtr message_ptr, PacketPtr packet_ptr) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.size() > 1000) {
        TOP_WARN_NAME("message queue.size() > 1000, and drop the new one(type=%s from %s:%d)",
            message_ptr->type(),
            packet_ptr->get_from_ip_addr().c_str(),
            packet_ptr->get_from_ip_port());
        return;
    }

    queue_.push_back(std::make_pair(message_ptr, packet_ptr));
}

bool NatManager::MessageQueue::GetMessage(MessagePtr& message_ptr, PacketPtr& packet_ptr) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!condv_.wait_for(lock, std::chrono::milliseconds(500), [this]{
            return !queue_.empty(); })) {
        // TOP_DEBUG_NAME("message condv wait_for timeout");
        return false;
    }

    message_ptr = queue_.front().first;
    packet_ptr = queue_.front().second;
    queue_.pop_front();
    return true;
}

void NatManager::MessageQueue::Clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.clear();
    TOP_INFO_NAME("NatManager::MessageQueue::Clear");
}

void NatManager::MessageQueue::SetName(const std::string& name) {
    name_ = name;
}

void NatManager::PushMessage(MessagePtr message_ptr, PacketPtr packet_ptr) {
    message_queue_.PushMessage(message_ptr, packet_ptr);
}

void NatManager::PushMessage(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    auto message_ptr = std::make_shared<transport::protobuf::RoutingMessage>();
    message_ptr->CopyFrom(message);
    auto packet_ptr = std::make_shared<base::xpacket_t>();
    packet_ptr->copy_from(packet);
    PushMessage(message_ptr, packet_ptr);
}

NatManagerIntf* NatManagerIntf::Instance() {
    static NatManager ins;
    return &ins;
}

bool NatManager::MultiNatDetect(const std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    TOP_INFO_NAME("MultiNatDetect(size=%d) ...", (int)boot_endpoints.size());

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!started_) {
            TOP_ERROR_NAME("start first");
            return false;
        }
    }

    if (nat_type_ != kNatTypeUnknown) {
        return true;
    }

    if (boot_endpoints.empty()) {
        TOP_INFO_NAME("no boot endpoints for detect nat");
        return false;
    }

    for (auto& kv : boot_endpoints) {
        SendNatDetectRequest(kv.first, kv.second);
    }

    TOP_INFO_NAME("nat detect start and wait ...");
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cond_.wait_for(lock, std::chrono::seconds(3), [this](){
                return this->nat_type_ != kNatTypeUnknown; })) {
            TOP_INFO_NAME("nat detect failed: cond_var timeout");
            return false;
        }
    }

    TOP_INFO_NAME("nat detect success: nat_type(%d)", (int)nat_type_);

    // remove detection
    for (auto& kv : boot_endpoints) {
        SendNatDetectFinish(kv.first, kv.second);
        nat_handshake_manager_->RemoveDetection(kv.first, kv.second);
    }

    return true;
}

NatManager::NatManager() {
    TOP_DEBUG_NAME("NatManager");
}

NatManager::~NatManager() {
    TOP_DEBUG_NAME("~NatManager ...");
    Stop();
    TOP_DEBUG_NAME("~NatManager");
}

void NatManager::HandleNatDetectRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    const auto peer_ip = packet.get_from_ip_addr();
    const auto peer_port = packet.get_from_ip_port();
    TOP_INFO_NAME("get kKadNatDetectRequest from node(%s:%d)",
        peer_ip.c_str(), peer_port);

    protobuf::NatDetectRequest detect_req;
    if (!detect_req.ParseFromString(message.data())) {
        TOP_INFO_NAME("NatDetectRequest ParseFromString from string failed!");
        return;
    }

    TOP_INFO_NAME("detect peer info: public(%s:%d), local(%s:%d)",
        peer_ip.c_str(), peer_port,
        detect_req.local_ip().c_str(), detect_req.local_port());

    transport::protobuf::RoutingMessage res_message;
    // res_message.set_des_service_type(message.src_service_type());
    // res_message.set_src_service_type(message.des_service_type());
    res_message.set_hop_num(0);
    res_message.set_src_node_id("");
    res_message.set_des_node_id(message.src_node_id());
    res_message.set_type(kKadNatDetectResponse);
    res_message.set_id(message.id());
    res_message.set_is_root(message.is_root());

    protobuf::NatDetectResponse detect_res;
    if (detect_req.local_ip() == peer_ip && detect_req.local_port() == peer_port) {
        detect_res.set_nat_type(kNatTypePublic);
    }
    detect_res.set_detect_port(nat_transport_->local_port());

    std::string data;
    if (!detect_res.SerializeToString(&data)) {
        assert(0);
    }
    res_message.set_data(data);

    xbyte_buffer_t xdata = SerializeMessage(res_message);
    if (xdata.empty()) {
        return;
    }
    transport_->SendPing(xdata, peer_ip, peer_port);

    if (detect_res.nat_type() == kNatTypePublic) {
        TOP_INFO_NAME("peer nat_type is public");
        return;
    }

    TOP_INFO_NAME("peer nat_type is unknown, and start detecting");

    nat_handshake_manager_->AddDetection(
            peer_ip,
            peer_port,
            nat_transport_,
            peer_port,
            kKadNatDetectHandshake2Node,
            0);  // boot detect first
}

void NatManager::OnTimerNatDetectTimeout() {
    TOP_DEBUG_NAME("timer proc is called");  // TODO(blueshi):
    if (SetNatTypeAndNotify(kNatTypeConeAbnormal)) {
        TOP_INFO_NAME("nat detect finished: timer timeout");
    }
}

void NatManager::HandleNatDetectResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    const auto peer_ip = packet.get_from_ip_addr();
    const auto peer_port = packet.get_from_ip_port();
    TOP_INFO_NAME("get kKadNatDetectResponse from node(%s:%d)",
        peer_ip.c_str(), peer_port);

    protobuf::NatDetectResponse detect_res;
    if (!detect_res.ParseFromString(message.data())) {
        TOP_INFO_NAME("NatDetectResponse ParseFromString from string failed!");
        return;
    }

    if (detect_res.nat_type() == kNatTypePublic) {
        if (SetNatTypeAndNotify(kNatTypePublic)) {
            TOP_INFO_NAME("nat detect finished: get public from boot");
        }
        return;
    }

    TOP_INFO_NAME("local nat_type is unknown, and start detecting");
    nat_handshake_manager_->AddDetection(
            peer_ip,
            peer_port,
            nat_transport_,
            detect_res.detect_port(),
            kKadNatDetectHandshake2Boot,
            500);  // node wait 500ms for boot detect first

    TOP_INFO_NAME("start timer to detect nat_type");
    timer_ = std::make_shared<base::Timer>(timer_manager_, "NatManager");
    timer_->CallAfter(
        1500 * 1000,  // TODO(blueshi): why 1 second between req/res?
        std::bind(&NatManager::OnTimerNatDetectTimeout, this));
}

void NatManager::HandleNatDetectHandshake2Node(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_INFO_NAME("get kKadNatDetectHandshake2Node from boot(%s:%d)",
        packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());

    if (SetNatTypeAndNotify(kNatTypeConeNormal)) {
        TOP_INFO_NAME("nat detect finished: receive handshake from boot");
    }
}

void NatManager::HandleNatDetectHandshake2Boot(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_DEBUG_NAME("get kKadNatDetectHandshake2Boot from node(%s:%d)",
        packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
}

void NatManager::HandleNatDetectFinish(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    const auto peer_ip = packet.get_from_ip_addr();
    const auto peer_port = packet.get_from_ip_port();
    TOP_INFO_NAME("get HandleNatDetectFinish from node(%s:%d)",
        peer_ip.c_str(), peer_port);

    nat_handshake_manager_->RemoveDetection(peer_ip, peer_port);
}

bool NatManager::SetNatTypeAndNotify(NatType nat_type) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (nat_type_ != kNatTypeUnknown) {
        return false;
    }

    nat_type_ = nat_type;
    cond_.notify_one();
    return true;
}

void NatManager::SendNatDetectRequest(
        const std::string& peer_ip,
        uint16_t peer_port) {
    TOP_INFO_NAME("SendNatDetectRequest(%s:%d)",
        peer_ip.c_str(), (int)peer_port);
    transport::protobuf::RoutingMessage message;
    // message.set_src_service_type(local_node_ptr_->service_type());
    // message.set_des_service_type(kRoot);
    message.set_hop_num(0);
    message.set_src_node_id("");
    message.set_des_node_id("");
    message.set_type(kKadNatDetectRequest);
    message.set_id(CallbackManager::MessageId());
    message.set_is_root(true);

    protobuf::NatDetectRequest detect_req;
    detect_req.set_local_ip(nat_transport_->local_ip());
    detect_req.set_local_port(nat_transport_->local_port());

    std::string data;
    if (!detect_req.SerializeToString(&data)) {
        assert(0);
    }
    message.set_data(data);

    xbyte_buffer_t xdata = SerializeMessage(message);
    if (xdata.empty()) {
        return;
    }
    // TOP_FATAL("SendNatDetectRequest(size=%d)", (int)xdata.size());
    if (nat_transport_->SendPing(xdata, peer_ip, peer_port) != kKadSuccess) {
        TOP_INFO_NAME("send nat detect(%s:%d) failed", peer_ip.c_str(), (int)peer_port);
    }
}

void NatManager::SendNatDetectFinish(
        const std::string& peer_ip,
        uint16_t peer_port) {
    TOP_INFO_NAME("SendNatDetectFinish(%s:%d)",
        peer_ip.c_str(), (int)peer_port);
    transport::protobuf::RoutingMessage message;
    // message.set_src_service_type(local_node_ptr_->service_type());
    // message.set_des_service_type(kRoot);
    message.set_hop_num(0);
    message.set_src_node_id("");
    message.set_des_node_id("");
    message.set_type(kKadNatDetectFinish);
    message.set_id(CallbackManager::MessageId());
    message.set_is_root(true);

    protobuf::NatDetectFinish detect_finish;
    detect_finish.set_resv(0);

    std::string data;
    if (!detect_finish.SerializeToString(&data)) {
        assert(0);
    }
    message.set_data(data);

    xbyte_buffer_t xdata = SerializeMessage(message);
    if (xdata.empty()) {
        return;
    }
    if (nat_transport_->SendPing(xdata, peer_ip, peer_port) != kKadSuccess) {
        TOP_INFO_NAME("send nat detect(%s:%d) failed", peer_ip.c_str(), (int)peer_port);
    }
}

bool NatManager::GetLocalNatType(int32_t& nat_type) {
    nat_type = nat_type_;
    return true;
}

bool NatManager::Start(
        bool first_node,
        const std::set<std::pair<std::string, uint16_t>>& boot_endpoints,
        transport::MultiThreadHandler*,
        transport::Transport* transport,
        transport::Transport* nat_transport) {
    TOP_INFO_NAME("starting ...");
    assert(transport);
    assert(nat_transport);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (started_) {
            TOP_INFO_NAME("start before");
            return true;
        }

        transport_ = transport;
        nat_type_ = kNatTypeUnknown;
        message_queue_.Clear();
        message_queue_.SetName(name_);
        nat_handshake_manager_ = std::make_shared<NatHandshakeManager>(timer_manager_);
        nat_handshake_manager_->SetName(name_);
        if (!nat_handshake_manager_->Start()) {
            TOP_ERROR_NAME("handshake manager start failed");
            return false;
        }

        nat_transport_ = nat_transport;
        if (first_node) {
            nat_type_ = kNatTypePublic;
            TOP_INFO_NAME("first_node: set nat_type public");
        }

        TOP_INFO("starting thread(NatManager) ...");
        thread_destroy_ = false;
        thread_ = std::make_shared<std::thread>(&NatManager::ThreadProc, this);

        TOP_INFO_NAME("start success");
        started_ = true;
    }

    if (!MultiNatDetect(boot_endpoints)) {
        TOP_WARN_NAME("multi nat detect failed");
    }

    return true;
}

void NatManager::Stop() {
    TOP_INFO_NAME("stopping ...");
    std::unique_lock<std::mutex> lock(mutex_);
    if (!started_) {
        TOP_INFO_NAME("haven't started");
        return;
    }

    if (stopped_) {
        TOP_INFO_NAME("stopped before");
    }

    ThreadJoin();
    nat_handshake_manager_->Stop();
    if (nat_transport_) {
        //nat_transport_->Stop();
        nat_transport_ = nullptr;
    }
    
    stopped_ = true;
    TOP_INFO_NAME("stop success");
}

void NatManager::ThreadJoin() {
    timer_ = nullptr;

    if (thread_) {
        thread_destroy_ = true;
        thread_->join();
        thread_ = nullptr;
        TOP_INFO("thread(NatManager) stopped");
    }
}

void NatManager::ThreadProc() {
    TOP_INFO_NAME("ThreadProc started");
    while (!thread_destroy_) {
        auto message_ptr = std::make_shared<transport::protobuf::RoutingMessage>();
        auto packet_ptr = std::make_shared<base::xpacket_t>();
        if (!message_queue_.GetMessage(message_ptr, packet_ptr)) {
            continue;
        }

        TOP_DEBUG_NAME("get message(type=%d)", message_ptr->type());
        switch (message_ptr->type()) {
        case kKadNatDetectRequest:
            HandleNatDetectRequest(*message_ptr, *packet_ptr);
            break;
        case kKadNatDetectResponse:
            HandleNatDetectResponse(*message_ptr, *packet_ptr);
            break;
        case kKadNatDetectHandshake2Node:
            HandleNatDetectHandshake2Node(*message_ptr, *packet_ptr);
            break;
        case kKadNatDetectHandshake2Boot:
            HandleNatDetectHandshake2Boot(*message_ptr, *packet_ptr);
            break;
        case kKadNatDetectFinish:
            HandleNatDetectFinish(*message_ptr, *packet_ptr);
            break;
        }
    }
    TOP_INFO_NAME("ThreadProc stopped");
}

xbyte_buffer_t NatManager::SerializeMessage(const transport::protobuf::RoutingMessage& message) {
    std::string msg;
    if (!message.SerializeToString(&msg)) {
        TOP_INFO("RoutingMessage SerializeToString failed!");
        return xbyte_buffer_t{};
    }

    xbyte_buffer_t xdata{msg.begin(), msg.end()};
    return xdata;
}

void NatManager::SetNatType(NatType nat_type) {
    TOP_FATAL("set nat_type(%d) to (%d)", nat_type_.load(), nat_type);
    nat_type_ = nat_type;
}

}  // namespace kadmlia
}  // namespace top
