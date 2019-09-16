// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>
#include <string>
#include <gtest/gtest.h>
#include "xkad/routing_table/node_info.h"

namespace top {
namespace kadmlia {
namespace test {

class TestNodeInfo : public testing::Test {
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

TEST_F(TestNodeInfo, IsPublicNode) {
	NodeInfo node_info("node_id");
	node_info.public_ip = "10.88.0.100";
	node_info.public_port = 8080;
	node_info.local_ip = "10.88.0.100";
	node_info.local_port = 8080;
	ASSERT_TRUE(node_info.IsPublicNode());
    node_info.Heartbeat();
}

TEST_F(TestNodeInfo, copy) {
	NodeInfo node_info;
	NodeInfo node2(node_info);

	NodeInfo node3;
	node3 = node_info;
}

TEST_F(TestNodeInfo, cmp) {
	NodeInfo node;
	node.node_id = "1";

	NodeInfo node2;
	node2.node_id = "2";

	ASSERT_TRUE(node < node2);
	ASSERT_EQ("1", node.string());
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
