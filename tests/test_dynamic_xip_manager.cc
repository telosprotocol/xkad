// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#include "xkad/routing_table/client_node_manager.h"
#define private public
#include "xkad/routing_table/dynamic_xip_manager.h"

namespace top {

namespace kadmlia {

namespace test {

class TestDynamicXipManager : public testing::Test {
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

TEST_F(TestDynamicXipManager, All) {
	auto manager = std::make_shared<DynamicXipManager>();
	ClientNodeInfoPtr node_info;
    node_info.reset(new ClientNodeInfo("id"));
	std::string dyxip = "1234";
    ASSERT_EQ(kKadSuccess, manager->AddClientNode(dyxip, node_info));

	{
		ASSERT_NE(nullptr, manager->FindClientNode(dyxip));
		ASSERT_EQ(nullptr, manager->FindClientNode("abcd"));
	}

    manager->RemoveClientNode(dyxip);
    

	{
		const base::XipParser local_xip;
		auto ret = manager->DispatchDynamicXip(local_xip);
		ASSERT_EQ(16u, ret.size());
	}
}

}  // namespace test

}  // namespace kadmlia

}  // namespace top
