// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/nat_detect/nat_handshake_manager.h"

#include "xbase/xutl.h"

#include "xpbase/base/top_utils.h"
#include "xkad/routing_table/callback_manager.h"
#include "xtransport/proto/transport.pb.h"
#include "xkad/proto/kadmlia.pb.h"
// #include "xkad/nat_detect/nat_log.h"
#include "xpbase/base/top_log_name.h"

namespace top {
namespace kadmlia {

static const int32_t kDetectionPeriod = 50;  // 50ms
static const int32_t kDetectCount = 2000 / kDetectionPeriod;  // max: 2000ms

void NatHandshakeManager::SetName(const std::string& name) {
    name_ = name;
}

NatHandshakeManager::NatHandshakeManager(base::TimerManager* timer_manager) {
    TOP_WARN_NAME("NatHandshakeManager");
    timer_manager_ = timer_manager;
}

NatHandshakeManager::~NatHandshakeManager() {
    TOP_WARN_NAME("~NatHandshakeManager ...");
    Stop();
    TOP_WARN_NAME("~NatHandshakeManager");
}

void NatHandshakeManager::AddDetection(
        const std::string& ip,
        uint16_t port,
        transport::Transport* transport,
        uint16_t detect_port,
        int32_t message_type,
        int32_t delay_ms) {
    assert(transport);
    const auto delay_count = delay_ms / kDetectionPeriod;
    TOP_INFO_NAME("add nat detection(%s:%d, detect_port=%d, message_type=%d, delay_ms=%d(%d)",
        ip.c_str(), (int)port, (int)detect_port, message_type, delay_ms, delay_count);
    const std::string key = ip + "_" + std::to_string(port);
    auto ptr = std::make_shared<NatDetectStruct>();
    ptr->ip = ip;
    ptr->port = port;
    ptr->transport = transport;
    ptr->detect_port = detect_port;
    ptr->message_type = message_type;
    ptr->delay_count = delay_count;
    ptr->detect_count = kDetectCount;

    std::unique_lock<std::mutex> lock(mutex_);
    peers_[key] = ptr; // overlap old one
}

void NatHandshakeManager::RemoveDetection(const std::string& ip, uint16_t port) {
    TOP_INFO_NAME("remove nat detection(%s:%d)",
        ip.c_str(), (int)port);
    const std::string key = ip + "_" + std::to_string(port);
    std::unique_lock<std::mutex> lock(mutex_);
    peers_.erase(key);
}

bool NatHandshakeManager::Start() {
    TOP_INFO_NAME("starting ...");
    std::unique_lock<std::mutex> lock(mutex_);
    if (started_) {
        TOP_INFO_NAME("start before");
        return true;
    }

    peers_.clear();
    timer_ = std::make_shared<base::TimerRepeated>(timer_manager_, "NatHandshakeManager");
    timer_->Start(
            kDetectionPeriod * 1000,
            kDetectionPeriod * 1000,
            std::bind(&NatHandshakeManager::DetectProc, this));
    started_ = true;
    TOP_INFO_NAME("start success");
    return true;
}

void NatHandshakeManager::Stop() {
    TOP_INFO_NAME("stopping ...");
    std::unique_lock<std::mutex> lock(mutex_);
    if (!started_) {
        TOP_INFO_NAME("haven't started");
        return;
    }

    if (timer_) {
        timer_->Join();
    }

    started_ = false;
    TOP_INFO_NAME("stop success");
}

int NatHandshakeManager::DoHandshake(
        transport::Transport* transport,
        const std::string& peer_ip,
        uint16_t detect_port,
        int32_t message_type) {
    transport::protobuf::RoutingMessage message;
    // message.set_src_service_type(service_type_);
    // message.set_des_service_type(kRoot);
    message.set_hop_num(0);
    message.set_src_node_id("");
    message.set_des_node_id("");
    message.set_type(message_type);
    message.set_id(CallbackManager::MessageId());
    message.set_is_root(true);

    TOP_WARN_NAME("send handshake(%d) to peer(%s:%d)",
        message_type,
        peer_ip.c_str(), (int)detect_port);

    std::string msg;
    if (!message.SerializeToString(&msg)) {
        TOP_INFO("RoutingMessage SerializeToString failed!");
        return kKadFailed;
    }
    xbyte_buffer_t xdata{msg.begin(), msg.end()};
    return transport->SendPing(xdata, peer_ip, detect_port);
}

void NatHandshakeManager::DetectProc() {
    std::map<std::string, std::shared_ptr<NatDetectStruct>> peers;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        peers = peers_;  // yes, copy!
        if (peers_.size() > 1000) {
            TOP_WARN_NAME("detection peers_ size: %d", (int)peers_.size());
        }
    }

    for (auto& kv : peers) {
        auto& st = *kv.second;
        if (st.delay_count > 0) {
            st.delay_count -= 1;
            continue;
        }

        if (st.detect_count > 0) {
            DoHandshake(st.transport, st.ip, st.detect_port, st.message_type);
            st.detect_count -= 1;
        }

        if (st.detect_count <= 0) {
            TOP_INFO_NAME("stop handshake(%d) to peer(%s:%d)",
                st.message_type,
                st.ip.c_str(), (int)st.detect_port);
            std::unique_lock<std::mutex> lock(mutex_);
            peers_.erase(kv.first);
        }
    }
}

}  // namespace kadmlia
}  // namespace top
