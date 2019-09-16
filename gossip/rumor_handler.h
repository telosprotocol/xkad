// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <mutex>
#include <vector>

#include "xkad/proto/kadmlia.pb.h"
#include "xkad/gossip/rumor_message_manager.h"
#include "xkad/gossip/rumor_def.h"
#include "xkad/routing_table/node_info.h"
#include "xkad/routing_table/local_node_info.h"

namespace top {

namespace kadmlia {
    class RoutingTable;
    typedef std::shared_ptr<RoutingTable> RoutingTablePtr;
}

namespace gossip {

class RumorHandler {
public:
    RumorHandler()
        :inited_(false),
        just_root_(false),
        kad_routing_table_(),
        message_manager_ptr_(std::make_shared<RumorMessageManager>()) {}

    RumorHandler(bool just_root)
        :inited_(false),
         just_root_(just_root),
         kad_routing_table_(),
         message_manager_ptr_(std::make_shared<RumorMessageManager>()) {}
    ~RumorHandler() {}
    bool Init(
        kadmlia::RoutingTablePtr);
    bool UnInit();
    bool AddRumorMessage(
        const transport::protobuf::RoutingMessage&);
    void RemoveRumorMessage(
        int32_t);
    bool FindRumorMessage(
        int32_t,
        transport::protobuf::RoutingMessage&);
    void GetAllRumorMessage(
        std::vector<transport::protobuf::RoutingMessage>&);
    bool SendToClosestNode(
        const transport::protobuf::RoutingMessage&);
    std::shared_ptr<kadmlia::LocalNodeInfo>  LocalNodeInfo() const;
    void SpreadNeighbors(
        transport::protobuf::RoutingMessage&,
        const std::vector<kadmlia::NodeInfoPtr>&);
    void SpreadNeighborsRapid(
        transport::protobuf::RoutingMessage&);
    void SpreadNeighborsSelfMessage();
    void GetAllNeighborNodes(
        std::vector<kadmlia::NodeInfoPtr>&);
private:
    bool inited_;
    bool just_root_;
    kadmlia::RoutingTablePtr kad_routing_table_;
    RumorMessageManagerSptr message_manager_ptr_;

    DISALLOW_COPY_AND_ASSIGN(RumorHandler);
};

using RumorHandlerSptr = std::shared_ptr<RumorHandler>;

}
}
