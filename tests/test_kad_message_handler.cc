// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// #include <string.h>

// #include <string>

// #include <gtest/gtest.h>

// #include "xkad/routing_table/routing_table.h"
// #include "xtransport/udp_transport/udp_transport.h"
// #include "xpbase/base/kad_key/get_kadmlia_key.h"
// #include "xkad/routing_table/local_node_info.h"
// #define private public
// #define protected public
// #include "xkad/routing_table/kad_message_handler.h"
// #include "xtransport/message_manager/message_manager_intf.h"

// namespace top {

// namespace kadmlia {

// namespace test {

// class TestKadMessageHandler : public testing::Test {
// public:
// 	static void SetUpTestCase() {

// 	}

// 	static void TearDownTestCase() {
// 	}

// 	virtual void SetUp() {

// 	}

// 	virtual void TearDown() {
// 	}
// };

// TEST_F(TestKadMessageHandler, All) {
// 	std::vector<int> vec_message{
// 		kKadConnectRequest,
// 		kKadHandshake,
// 		kKadBootstrapJoinRequest,
// 		kKadBootstrapJoinResponse,
// 		kKadFindNodesRequest,
// 		kKadFindNodesResponse,
// 		kKadHeartbeatRequest,
// 		kKadHeartbeatResponse,
// 		kKadAck,
// 		kKadNatDetectRequest,
// 		kKadNatDetectResponse,
// 		kKadNatDetectHandshake2Node,
// 		kKadNatDetectHandshake2Boot,
// 		kKadNatDetectFinish,
// 	};

// 	for (auto msg_type : vec_message) {
// 		transport::MessageManagerIntf::Instance()->UnRegisterMessageProcessor(msg_type);
// 	}

// 	auto local_node = std::make_shared<LocalNodeInfo>();
// 	// const base::XipParser xip;
// 	// local_node->set_kadmlia_key(base::GetKadmliaKey(xip));
// 	std::string xid;
// 	xid.resize(kNodeIdSize);
// 	local_node->set_kadmlia_key(base::GetKadmliaKey(xid));
// 	local_node->set_first_node(true);
// 	uint32_t kadmlia_key_len = kNodeIdSize;
// 	auto transport = std::make_shared<transport::UdpTransport>();
// 	auto routing_table = std::make_shared<RoutingTable>(transport, kadmlia_key_len, local_node);
// 	ASSERT_TRUE(routing_table->Init());
// 	KadMessageHandler message_handler;
// 	message_handler.set_routing_ptr(routing_table);
// 	for (auto msg_type : vec_message) {
// 		transport::protobuf::RoutingMessage message;
// 		{
// 			std::string des_node_id;
// 			des_node_id.resize(36);
// 			message.set_des_node_id(des_node_id);
// 		}
// 		base::xpacket_t packet;
// 		message.set_type(msg_type);
// 		transport::MessageManagerIntf::Instance()->HandleMessage(message, packet);
// 	}
// }

// }  // namespace test

// }  // namespace kadmlia

// }  // namespace top
