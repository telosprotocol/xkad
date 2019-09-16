// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/node_info.h"
#define private public
#define protected public
#include "xkad/routing_table/node_detection_manager.h"

namespace top {

namespace kadmlia {

namespace test {

class TestNodeDetectionManager : public testing::Test {
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

TEST_F(TestNodeDetectionManager, All) {
	auto sptr_node_info = std::make_shared<NodeInfo>("qwertgfdsazxcvbnhcdswq34321edswq");
    sptr_node_info->public_ip = "10.18.0.100";
    sptr_node_info->public_port = 5000;
    std::shared_ptr<LocalNodeInfo> local_node = std::make_shared<LocalNodeInfo>();
    RoutingTable routing_table(nullptr, kNodeIdSize, local_node);
    NodeDetectionManager node_detection_mgr(base::TimerManager::Instance(), routing_table);
    node_detection_mgr.AddDetectionNode(sptr_node_info);
    node_detection_mgr.RemoveDetection("10.18.0.100", 5000);

	{
		std::string id = "1234";
		ASSERT_FALSE(node_detection_mgr.Detected(id));
	}
}


}  // namespace test

}  // namespace kadmlia

}  // namespace top
