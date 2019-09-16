// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/node_detection_manager.h"

#include "xbase/xutl.h"

#include "xtransport/transport.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/top_utils.h"
#include "xkad/routing_table/callback_manager.h"
#include "xkad/routing_table/node_info.h"
#include "xtransport/proto/transport.pb.h"
#include "xkad/proto/kadmlia.pb.h"
#include "xkad/routing_table/local_node_info.h"
#include "xkad/routing_table/routing_table.h"

namespace top {

namespace kadmlia {

// static const int kDetectedMapClearCount = 100 * 1024;
static const int32_t kDoDetectionPeriod = 600 * 1000;  // 600ms

NodeDetectionManager::NodeDetectionManager(base::TimerManager* timer_manager, RoutingTable& routing_table)
        : detection_nodes_map_(),
          detection_nodes_map_mutex_(),
          detected_nodes_map_(),
          detected_nodes_map_mutex_(),
          routing_table_(routing_table) {
    timer_manager_ = timer_manager;
    timer_ = std::make_shared<base::TimerRepeated>(timer_manager_, "NodeDetectionManager");
    timer_->Start(
            kDoDetectionPeriod,
            kDoDetectionPeriod,
            std::bind(&NodeDetectionManager::DoTetection, this));
}

NodeDetectionManager::~NodeDetectionManager() {
    Join();
    TOP_INFO("NodeDetectionManager thread joined!");
}

void NodeDetectionManager::Join() {
    destroy_ = true;
    timer_ = nullptr;
    {
        std::unique_lock<std::mutex> lock(detection_nodes_map_mutex_);
        detection_nodes_map_.clear();
    }
    {
        std::unique_lock<std::mutex> lock(detected_nodes_map_mutex_);
        detected_nodes_map_.clear();
    }
}

bool NodeDetectionManager::Detected(const std::string& id) {
    std::unique_lock<std::mutex> lock(detected_nodes_map_mutex_);
    auto no_iter = detected_nodes_map_.find(id);
    if (no_iter != detected_nodes_map_.end()) {
        if (no_iter->second->detection_count >= 3) {
            return true;
        }
    }

    return false;
}

int NodeDetectionManager::AddDetectionNode(std::shared_ptr<NodeInfo> node_ptr) {
    if (destroy_) {
        return kKadSuccess;
    }

    if (node_ptr->nat_type == kNatTypeConeAbnormal) {
        node_ptr->detection_delay_count = 3;
    } else {
        node_ptr->detection_delay_count = 0;
    }

    std::unique_lock<std::mutex> lock(detection_nodes_map_mutex_);
    std::string key = (node_ptr->public_ip + "_" +
            base::xstring_utl::tostring(node_ptr->public_port));
    auto ins_iter = detection_nodes_map_.insert(std::make_pair(key, node_ptr));
    if (ins_iter.second) {
        return kKadSuccess;
    }

    return kKadFailed;
}

void NodeDetectionManager::RemoveDetection(const std::string& ip, uint16_t port) {
    std::unique_lock<std::mutex> lock(detection_nodes_map_mutex_);
    std::string key = ip + "_" + base::xstring_utl::tostring(port);
    auto iter = detection_nodes_map_.find(key);
    if (iter != detection_nodes_map_.end()) {
        detection_nodes_map_.erase(iter);
    }
}

int NodeDetectionManager::Handshake(std::shared_ptr<NodeInfo> node_ptr) {
    if (destroy_) {
        return kKadSuccess;
    }

    transport::protobuf::RoutingMessage message;
    routing_table_.SetFreqMessage(message);
    LocalNodeInfoPtr local_node = routing_table_.get_local_node_info();
    if (!local_node) {
        return kKadFailed;
    }
    message.set_des_service_type(node_ptr->service_type);
    message.set_des_node_id(node_ptr->node_id);
    message.set_type(kKadHandshake);
    if (local_node->client_mode()) {
        message.set_client_msg(true);
    }

    std::shared_ptr<transport::Transport> transport_ptr = routing_table_.get_transport();
    if (!transport_ptr) {
        TOP_ERROR("service type[%llu] has not register transport.", message.des_service_type());
        return kKadFailed;
    }

    protobuf::Handshake handshake;
    handshake.set_type(kHandshakeRequest);
    handshake.set_local_ip(local_node->local_ip());
    handshake.set_local_port(local_node->local_port());
    handshake.set_public_ip(local_node->public_ip());
    handshake.set_public_port(local_node->public_port());
    handshake.set_nat_type(local_node->nat_type());
    handshake.set_xid(global_xid->Get());
    std::string data;
    if (!handshake.SerializeToString(&data)) {
        TOP_INFO("ConnectReq SerializeToString failed!");
        return kKadFailed;
    }

    message.set_data(data);
    std::string msg;
    if (!message.SerializeToString(&msg)) {
        TOP_INFO("RoutingMessage SerializeToString failed!");
        return kKadFailed;
    }
    xbyte_buffer_t xdata{msg.begin(), msg.end()};

    // try vlan connect 
    //transport_ptr->SendPing(xdata, node_ptr->local_ip, node_ptr->local_port);
    // try public connect 
    transport_ptr->SendPing(xdata, node_ptr->public_ip, node_ptr->public_port);
    TOP_DEBUG("sendping sendhandshake from:%s:%d to %s:%d size:%d",
            local_node->public_ip().c_str(),
            local_node->public_port(),
            (node_ptr->public_ip).c_str(),
            node_ptr->public_port,
            xdata.size());
    return kKadSuccess;
}

void NodeDetectionManager::DoTetection() {
    if (destroy_) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock(detection_nodes_map_mutex_);
        if (detection_nodes_map_.size() > 1000) {
            TOP_WARN("detection_nodes_map_ size: %d", detection_nodes_map_.size());
        }
        auto iter = detection_nodes_map_.begin();
        while (iter != detection_nodes_map_.end()) {
            if (iter->second->detection_delay_count > 0) {
                iter->second->detection_delay_count--;
                ++iter;
                continue;
            }

            if (iter->second->detection_count >= kDetectionTimes) {
                detection_nodes_map_.erase(iter++);
                continue;
            }

            Handshake(iter->second);
            iter->second->detection_count++;
            ++iter;
        }
    }
}

}  // namespace kadmlia

}  // namespace top
