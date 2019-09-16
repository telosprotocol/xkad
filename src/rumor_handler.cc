// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/gossip/rumor_handler.h"

#include "xpbase/base/uint64_bloomfilter.h"
#include "xkad/gossip/rumor_def.h"
#include "xkad/routing_table/local_node_info.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/node_info.h"
#include "xpbase/base/kad_key/get_kadmlia_key.h"

namespace top {
namespace gossip {

bool RumorHandler::Init(
    kadmlia::RoutingTablePtr kad_routing_table) {
    if (inited_) {
        TOP_WARN("RumorHandler::Init Already Inited.");
        return true;
    }
    if (!kad_routing_table) {
        TOP_WARN("kadmlia::RoutingTable Invalid.");
        return false;
    }
    kad_routing_table_ = kad_routing_table;
    inited_ = true;
    TOP_DEBUG("RumorHandler::Init Success.");
    return true;
}

bool RumorHandler::UnInit() {
    kad_routing_table_.reset();
    inited_ = false;
    return true;
}

bool RumorHandler::AddRumorMessage(
    const transport::protobuf::RoutingMessage& message) {
    if (!message_manager_ptr_) {
        TOP_WARN("RumorMessageManagerSptr Is Empty.", message.type());
        return false;
    }
    if(!message_manager_ptr_->AddMessage(message)) {
        TOP_WARN("RumorHandler::AddRumorMessage Failed.Message Type:%d", message.type());
        return false;
    }
    return true;
}

void RumorHandler::RemoveRumorMessage(int32_t message_type) {
    return message_manager_ptr_->RemoveMessage(message_type);
}

bool RumorHandler::FindRumorMessage(
        int32_t message_type, 
        transport::protobuf::RoutingMessage& message) {
    if(!message_manager_ptr_->SearchMessage(message_type, message)) {
        TOP_WARN("RumorHandler::FindRumorMessage Failed.Message Type:%d", message_type);
        return false;
    }
    return true;
}

void RumorHandler::GetAllRumorMessage(
    std::vector<transport::protobuf::RoutingMessage>& all_message) {
    return message_manager_ptr_->GetAllMessages(all_message);
}

void RumorHandler::GetAllNeighborNodes(
    std::vector<kadmlia::NodeInfoPtr>& all_neighbors) {
    auto local_node_info_ptr = kad_routing_table_->get_local_node_info();
    if (!local_node_info_ptr) {
        TOP_WARN("kadmlia::LocalNodeInfo is invalid");
        return;
    }
    std::string node_id = local_node_info_ptr->id();
    auto tmp_all_neighbors = kad_routing_table_->nodes();
    if (!just_root_) {
        all_neighbors.swap(tmp_all_neighbors);
        return;
    }

    std::set<std::string> getted_nodes;
    for (auto iter = tmp_all_neighbors.begin(); iter != tmp_all_neighbors.end(); ++iter) {
        if ((*iter)->xid.empty()) {
            continue;
        }

        base::KadmliaKeyPtr xid = base::GetKadmliaKey((*iter)->xid);
        if (xid->xnetwork_id() != kRoot) {
            continue;
        }

        auto getted_iter = getted_nodes.find((*iter)->xid);
        if (getted_iter != getted_nodes.end()) {
            continue;
        }
        getted_nodes.insert((*iter)->xid);
        all_neighbors.push_back(*iter);
    }
}

void RumorHandler::SpreadNeighbors(
        transport::protobuf::RoutingMessage& message,
        const std::vector<kadmlia::NodeInfoPtr>& neighbors) {
    assert(false);
    message.set_broadcast(true);
    if (neighbors.empty()) {
        TOP_WARN("No neighbors exists.Message Type:%d", 
            message.type());
        return;
    }
    if (message.src_node_id().empty()) {
        TOP_WARN("src node id is not given.Message Type:%d",
            message.type());
        return;
    }
    auto local_node_info = kad_routing_table_->get_local_node_info();
    if (!local_node_info) {
        TOP_WARN("LocalNodeInfo Is Invalid.");
        return;
    }
    std::vector<uint64_t> new_bloomfilter_vec;
    for (auto i = 0; i < message.bloomfilter_size(); ++i) {
        new_bloomfilter_vec.push_back(message.bloomfilter(i));
    }
    std::shared_ptr<base::Uint64BloomFilter> new_bloomfilter;
    if (new_bloomfilter_vec.empty()) {
        new_bloomfilter = std::make_shared<base::Uint64BloomFilter>(
                kadmlia::kFindNodesBloomfilterBitSize,
                kadmlia::kFindNodesBloomfilterHashNum);
    } else {
        new_bloomfilter = std::make_shared<base::Uint64BloomFilter>(
                new_bloomfilter_vec,
                kadmlia::kFindNodesBloomfilterHashNum);
    }
    base::Uint64BloomFilter old_bloomfilter = *new_bloomfilter;
    // (Charlie): filter just by xid
    std::string node_id = global_xid->Get();  // local_node_info->id();
    new_bloomfilter->Add(gossip::RumorIdentity(message.id(), node_id).ToString());
    transport::protobuf::RoutingMessage send_message = message;
    std::for_each(neighbors.begin(), neighbors.end(),
        [&](kadmlia::NodeInfoPtr node_info_ptr) {
        if (!node_info_ptr) {
            TOP_WARN("kadmlia::NodeInfoPtr Is Empty.");
            return false;
        }
        gossip::RumorIdentity rumor_identity(message.id(), node_info_ptr->xid);
        std::string rumor_key = rumor_identity.ToString();
        new_bloomfilter->Add(rumor_key);
        return true;
    });
    const std::vector<uint64_t>& bloomfilter_vec = new_bloomfilter->Uint64Vector();
    send_message.clear_bloomfilter();
    for (uint32_t i = 0; i < bloomfilter_vec.size(); ++i) {
        send_message.add_bloomfilter(bloomfilter_vec[i]);
    }
    std::for_each(neighbors.begin(), neighbors.end(),
            [&](kadmlia::NodeInfoPtr node_info_ptr) {
        if (!node_info_ptr) {
            TOP_WARN("kadmlia::NodeInfoPtr is invalid");
            return false;
        }
        std::string peer_ip = node_info_ptr->public_ip;
        uint16_t peer_port = node_info_ptr->public_port;
        if (send_message.src_node_id() == node_info_ptr->xid ||
                send_message.src_node_id() == node_info_ptr->node_id) {
            TOP_DEBUG("filter the id[%s] whitch is same as src node[%s].",
                node_info_ptr->node_id.c_str(),
                send_message.src_node_id().c_str());
            return true;
        }
        std::string dest_node_id = node_info_ptr->xid;
        if (dest_node_id.empty()) {
            return false;
        }
        gossip::RumorIdentity rumor_identity(send_message.id(), dest_node_id);
        std::string rumor_key = rumor_identity.ToString();
        if (old_bloomfilter.Contain(rumor_key)) {
            return false;
        }
        TOP_DEBUG("gossip message ip: %s, port: %d, [%d]",
                peer_ip.c_str(), peer_port, message.type());
        if (kadmlia::kKadSuccess != kad_routing_table_->SendData(send_message, peer_ip, peer_port)) {
            TOP_WARN("SendData Failed,ip:%s,port:%d", peer_ip.c_str(), peer_port);
            return false;
        }
        return true;
    });
}

void RumorHandler::SpreadNeighborsRapid(
    transport::protobuf::RoutingMessage& message) {
    std::vector<kadmlia::NodeInfoPtr> all_neighbors;
    GetAllNeighborNodes(all_neighbors);
    return SpreadNeighbors(message, all_neighbors);
}

void RumorHandler::SpreadNeighborsSelfMessage() {
    if (0 == message_manager_ptr_->GetMessageCount()) {
        TOP_DEBUG("kadmlia::Rumor Message Is Empty");
        return;
    }
    std::vector<transport::protobuf::RoutingMessage> all_messages;
    message_manager_ptr_->GetAllMessages(all_messages);
    std::vector<kadmlia::NodeInfoPtr> all_neighbors;
    GetAllNeighborNodes(all_neighbors);
    for (auto& message : all_messages) {
        SpreadNeighbors(message, all_neighbors);
    }
}

bool RumorHandler::SendToClosestNode(
    const transport::protobuf::RoutingMessage& in_message) {
    if (!kad_routing_table_) {
        TOP_WARN("kadmlia::RoutingTable is invalid");
        return false;
    }
    if (in_message.src_node_id().empty()) {
        TOP_WARN("src node id is not given.");
        return false;
    }
    transport::protobuf::RoutingMessage message = in_message;
    kad_routing_table_->SendToClosestNode(message);
    return true;
}

std::shared_ptr<kadmlia::LocalNodeInfo> RumorHandler::LocalNodeInfo() const {
    if (!kad_routing_table_) {
        TOP_WARN("kadmlia::RoutingTable is invalid");
        return {};
    }
    return kad_routing_table_->get_local_node_info();
}
}
}
