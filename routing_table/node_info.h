// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <memory>
#include <chrono>

#include "xbasic/xhash.hpp"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/nat_detect/nat_defines.h"
#include "xtransport/transport_fwd.h"

namespace top {
namespace kadmlia {

const int kInvalidBucketIndex = -1;
const int kSelfBucketIndex = 0;

struct NodeInfo {
public:
    NodeInfo();
    NodeInfo(const NodeInfo& other);
    NodeInfo(const std::string& id);
    ~NodeInfo();
    NodeInfo& operator=(const NodeInfo& other);
    bool operator < (const NodeInfo& other) const;
    bool IsPublicNode();
    std::string string();
    bool IsTimeout(std::chrono::steady_clock::time_point tp_now);
    bool IsTimeToHeartbeat(std::chrono::steady_clock::time_point tp_now);
    void Heartbeat();
    void ResetHeartbeat();

public:
    std::string node_id;
    int bucket_index{ kInvalidBucketIndex };
    std::string public_ip;
    uint16_t public_port{ 0 };
    std::string local_ip;
    uint16_t local_port{ 0 };
    int32_t connection_id{ 0 };
    int32_t detection_count{ 0 };
    int32_t nat_type{ 0 };
    int32_t detection_delay_count{ 0 };
    int32_t heartbeat_count{ 0 };  // count > 3
    uint64_t service_type{ 0 };
    bool is_client{ false };
    std::chrono::steady_clock::time_point tp_next_time_to_heartbeat;
    bool same_vlan{ false };
    std::string xid;
    std::string xip;
    uint32_t score{ 0 };
    uint64_t hash64{ 0 };
	transport::UdpPropertyPtr udp_property;
};

typedef std::shared_ptr<NodeInfo> NodeInfoPtr;

}  // namespace kadmlia
}  // namespace top
