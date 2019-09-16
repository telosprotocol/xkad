// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>
#include <thread>

#include <gtest/gtest.h>

#define private public
#include "xkad/routing_table/local_node_info.h"
#include "xpbase/base/xid/xid_generator.h"
#include "xkad/nat_detect/nat_manager_intf.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"

namespace top {

namespace kadmlia {

namespace test {

class TestLocalNodeInfo : public testing::Test {
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

TEST_F(TestLocalNodeInfo, All) {
    std::string idtype(top::kadmlia::GenNodeIdType("CN", "VPN"));
    LocalNodeInfo local_node_info;
    base::XIDGenerator xid_generator;
    base::XIDType xid_type;
    xid_type.xnetwork_id_ = 19;
    xid_type.zone_id_ = 250;
    ASSERT_TRUE(xid_generator.CreateXID(xid_type));
    base::XIDSptr xid = nullptr;
    xid.reset(new base::XID);
    ASSERT_TRUE(xid_generator.GetXID(*xid));
    NatManagerIntf::Instance()->SetNatType(kNatTypePublic);
    local_node_info.nat_type_ = kNatTypePublic;
    auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
    kad_key->set_xnetwork_id(kEdgeXVPN);
    const bool first_node = true;
    ASSERT_TRUE(local_node_info.Init(
            "0.0.0.0", 15942, first_node, false, idtype, kad_key, kEdgeXVPN, kRoleEdge));

    {
        base::XipParser xip_parser;
        auto str_xip = xip_parser.xip();
        local_node_info.set_xip(str_xip);
    }

    {
        const std::string node_id = "1234";
        const std::string dxip = "abcd";
        local_node_info.AddDxip(node_id, dxip);
        ASSERT_TRUE(local_node_info.HasDynamicXip(dxip));
        local_node_info.DropDxip(node_id);
        ASSERT_FALSE(local_node_info.HasDynamicXip(dxip));
    }

    local_node_info.Reset();
}

TEST_F(TestLocalNodeInfo, set_xip) {
    
}

}  // namespace test

}  // namespace kadmlia

}  // namespace top
