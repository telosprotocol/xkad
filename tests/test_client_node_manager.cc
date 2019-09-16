// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#define private public
#include "xkad/routing_table/client_node_manager.h"

namespace top {

namespace kadmlia {

namespace test {

class TestClientNodeManager : public testing::Test {
public:
	static void SetUpTestCase() {

	}

	static void TearDownTestCase() {
	}

	virtual void SetUp() {

	}

	virtual void TearDown() {
	}
};

TEST_F(TestClientNodeManager, All) {
	ASSERT_NE(nullptr, ClientNodeManager::Instance());

    ClientNodeManager client_node_mgr;
    ClientNodeInfoPtr node_info;
    node_info.reset(new ClientNodeInfo("id"));
    ASSERT_EQ(client_node_mgr.AddClientNode(node_info), kKadSuccess);

	{
		ASSERT_NE(nullptr, client_node_mgr.FindClientNode("id"));
		ASSERT_EQ(nullptr, client_node_mgr.FindClientNode("id2"));
	}

    client_node_mgr.RemoveClientNode("id");
    ASSERT_TRUE(client_node_mgr.client_nodes_map_.empty());

}

}  // namespace test

}  // namespace kadmlia

}  // namespace top
