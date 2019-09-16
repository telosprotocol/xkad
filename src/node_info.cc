// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/node_info.h"
#include "xtransport/udp_transport/xudp_socket.h"

namespace top {
namespace kadmlia {

static const int kHeartbeatFirstTimeout = 8;  // seconds
static const int kHeartbeatSecondTimeout = 2;  // seconds
static const int kHeartbeatErrorMaxCount = 12;

NodeInfo::NodeInfo() {
    ResetHeartbeat();
}

NodeInfo::NodeInfo(const NodeInfo& other)
        : node_id(other.node_id),
            bucket_index(other.bucket_index),
            public_ip(other.public_ip),
            public_port(other.public_port),
            local_ip(other.local_ip),
            local_port(other.local_port),
            connection_id(other.connection_id),
            detection_count(other.detection_count),
            nat_type(other.nat_type),
            detection_delay_count(other.detection_delay_count),
            heartbeat_count(other.heartbeat_count),
            service_type(other.service_type),
            is_client(other.is_client),
            same_vlan(other.same_vlan),
            xid(other.xid),
            xip(other.xip),
            score(other.score) {
    hash64 = base::xhash64_t::digest(xid);
    ResetHeartbeat();
	udp_property.reset(new top::transport::UdpProperty());	
}

NodeInfo::NodeInfo(const std::string& id) : node_id(id) {
    ResetHeartbeat();
	udp_property.reset(new top::transport::UdpProperty());	
}

NodeInfo::~NodeInfo() {
	udp_property = nullptr;
}

NodeInfo& NodeInfo::operator=(const NodeInfo& other) {
    if (this == &other) {
        return *this;
    }
    node_id = other.node_id;
    bucket_index = other.bucket_index;
    public_ip = other.public_ip;
    public_port = other.public_port;
    local_ip = other.local_ip;
    local_port =  other.local_port;
    connection_id = other.connection_id;
    detection_count = other.detection_count;
    nat_type = other.nat_type;
    detection_delay_count = other.detection_delay_count;
    heartbeat_count = other.heartbeat_count;  // count > 3
    service_type = other.service_type;
    is_client = other.is_client;
    tp_next_time_to_heartbeat = other.tp_next_time_to_heartbeat;
    same_vlan = other.same_vlan;
    xid = other.xid;
    xip = other.xip;
    score = other.score;
    hash64 = base::xhash64_t::digest(xid);
    return *this;
}

bool NodeInfo::operator < (const NodeInfo& other) const {
    return node_id < other.node_id;
}

bool NodeInfo::IsPublicNode() {
    return public_ip == local_ip && public_port == local_port;
}

std::string NodeInfo::string() {
    return node_id;
}

bool NodeInfo::IsTimeout(std::chrono::steady_clock::time_point tp_now) {
    return heartbeat_count >= kHeartbeatErrorMaxCount;
}

bool NodeInfo::IsTimeToHeartbeat(std::chrono::steady_clock::time_point tp_now) {
    return tp_now > tp_next_time_to_heartbeat;
}

void NodeInfo::Heartbeat() {
    ++heartbeat_count;
    tp_next_time_to_heartbeat = std::chrono::steady_clock::now() +
        std::chrono::seconds(kHeartbeatSecondTimeout);
}

void NodeInfo::ResetHeartbeat() {
    heartbeat_count = 0;
    tp_next_time_to_heartbeat = std::chrono::steady_clock::now() +
        std::chrono::seconds(kHeartbeatFirstTimeout);
}

}  // namespace kadmlia
}  // namespace top
