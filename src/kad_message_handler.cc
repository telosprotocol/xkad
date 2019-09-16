// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/kad_message_handler.h"

#include <vector>
#include <string>
#include <utility>
#include <map>

#include "xpbase/base/line_parser.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/multirelay_log.h"
#include "xtransport/transport_message_register.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/routing_table/callback_manager.h"
#include "xkad/routing_table/node_info.h"
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/node_detection_manager.h"
#include "xkad/routing_table/client_node_manager.h"
#include "xkad/routing_table/local_node_info.h"

namespace top {

namespace kadmlia {

KadMessageHandler::KadMessageHandler() {}

KadMessageHandler::~KadMessageHandler() {}

void KadMessageHandler::Init() {
    AddBaseHandlers();
}

void KadMessageHandler::set_routing_ptr(std::shared_ptr<RoutingTable> routing_ptr) {
    assert(routing_ptr);
    routing_ptr_ = routing_ptr;
}

void KadMessageHandler::AddBaseHandlers() {
    message_manager_->RegisterMessageProcessor(kKadConnectRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleConnectRequest(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadHandshake, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleHandshake(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadBootstrapJoinRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleBootstrapJoinRequest(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadBootstrapJoinResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleBootstrapJoinResponse(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadFindNodesRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleFindNodesRequest(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadFindNodesResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleFindNodesResponse(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadHeartbeatRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleHeartbeatRequest(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadHeartbeatResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        HandleHeartbeatResponse(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadAck, [](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
    });
    message_manager_->RegisterMessageProcessor(kKadNatDetectRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        nat_manager_->PushMessage(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadNatDetectResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        nat_manager_->PushMessage(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadNatDetectHandshake2Node, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        nat_manager_->PushMessage(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadNatDetectHandshake2Boot, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        nat_manager_->PushMessage(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kKadNatDetectFinish, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        nat_manager_->PushMessage(message, packet);
    });
}

int KadMessageHandler::HandleClientMessage(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!message.has_client_id()) {
        return kContinue;
    }

    LocalNodeInfoPtr local_node = routing_ptr_->get_local_node_info();
    if (!local_node) {
        TOP_ERROR("get routing table by next service type[%llu] failed!",
            message.des_service_type());
        return kKadFailed;
    }

    if (!message.relay_flag()) {
        if (message.client_id() == local_node->id()) {
            return kContinue;
        }

        ClientNodeInfoPtr client_node_ptr;
        client_node_ptr.reset(new ClientNodeInfo(message.client_id()));
        client_node_ptr->public_ip = packet.get_from_ip_addr();
        client_node_ptr->public_port = packet.get_from_ip_port();
        ClientNodeManager::Instance()->AddClientNode(client_node_ptr);  // just cover
        if (message.has_xid() && !message.xid().empty()) {
            ClientNodeInfoPtr client_node_ptr;
            client_node_ptr.reset(new ClientNodeInfo(message.xid()));
            client_node_ptr->public_ip = packet.get_from_ip_addr();
            client_node_ptr->public_port = packet.get_from_ip_port();
            ClientNodeManager::Instance()->AddClientNode(client_node_ptr);  // just cover
        }
        message.set_relay_flag(true);
        message.set_src_node_id(local_node->id());
        return kContinue;
    }

    if (message.des_node_id() != local_node->id()) {
        return kContinue;
    }

    // request message arrive des node or  response message arrive the first relay node
    ClientNodeInfoPtr client_node_ptr = ClientNodeManager::Instance()->FindClientNode(
            message.client_id());
    if (!client_node_ptr) {
        TOP_DEBUG("client[%s] request message arrive this dest node[%s]",
                HexEncode(message.client_id()).c_str(),
                HexEncode(local_node->id()).c_str());
        return kContinue;
    }
    TOP_DEBUG("response message of client[%s] arrive this first relay node[%s]",
            HexEncode(message.client_id()).c_str(),
            HexEncode(local_node->id()).c_str());

    std::string client_pub_ip = client_node_ptr->public_ip;
    uint16_t client_pub_port = client_node_ptr->public_port;
    message.set_relay_flag(false);
    message.set_des_node_id(client_node_ptr->node_id);
    return routing_ptr_->SendData(message, client_pub_ip, client_pub_port);
}

void KadMessageHandler::HandleHeartbeatRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleHeartbeatRequest(message, packet);
}

void KadMessageHandler::HandleHeartbeatResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleHeartbeatResponse(message, packet);
}

void KadMessageHandler::SendAck(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    transport::protobuf::RoutingMessage res_message;
    routing_ptr_->SetFreqMessage(res_message);
    LocalNodeInfoPtr local_node = routing_ptr_->get_local_node_info();
    if (!local_node) {
        return;
    }

    res_message.set_src_service_type(message.des_service_type());
    res_message.set_des_service_type(message.src_service_type());
    res_message.set_des_node_id(message.src_node_id());
    res_message.set_type(kKadAck);
    res_message.set_id(0);
    res_message.set_ack_id(message.ack_id());
    routing_ptr_->SendData(res_message, packet.get_from_ip_addr(), packet.get_from_ip_port());
}

void KadMessageHandler::HandleFindNodesRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleFindNodesRequest(message, packet);
}

void KadMessageHandler::HandleFindNodesResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleFindNodesResponse(message, packet);
}

void KadMessageHandler::HandleBootstrapJoinRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleBootstrapJoinRequest(message, packet);
}

void KadMessageHandler::HandleBootstrapJoinResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleBootstrapJoinResponse(message, packet);
}

void KadMessageHandler::HandleHandshake(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleHandshake(message, packet);
}

void KadMessageHandler::HandleConnectRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    routing_ptr_->HandleConnectRequest(message, packet);
}

}  // namespace kadmlia

}  // namespace top
