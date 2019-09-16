// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdio.h>
#include <string.h>

#include <string>
#include <memory>
#include <fstream>

#include <gtest/gtest.h>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_timer.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"
#include "xtransport/udp_transport/udp_transport.h"
#include "xtransport/src/message_manager.h"
#include "node_mgr.h"

namespace top {
namespace kadmlia {
namespace test {

class TestRoutingTable2 : public testing::Test {
public:
    static void SetUpTestCase() {
        mgr1_ = std::make_shared<NodeMgr>();
        ASSERT_TRUE(mgr1_->Init(true, "1"));
    }

    static void TearDownTestCase() {
        mgr1_ = nullptr;
    }

    virtual void SetUp() {}

    virtual void TearDown() {}

protected:
    static std::shared_ptr<NodeMgr> mgr1_;
};

std::shared_ptr<NodeMgr> TestRoutingTable2::mgr1_;

TEST_F(TestRoutingTable2, NatDetect) {
    auto mgr2 = std::make_shared<NodeMgr>();
    ASSERT_TRUE(mgr2->Init(false, "2"));
    ASSERT_TRUE(mgr2->NatDetect(mgr1_->LocalIp(), mgr1_->RealLocalPort()));
    mgr2 = nullptr;
}

TEST_F(TestRoutingTable2, Join) {
    auto mgr2 = std::make_shared<NodeMgr>();
    ASSERT_TRUE(mgr2->Init(false, "3"));
    ASSERT_TRUE(mgr2->JoinRt(mgr1_->LocalIp(), mgr1_->RealLocalPort()));
    mgr2 = nullptr;
}

// handle message
TEST_F(TestRoutingTable2, kKadConnectRequest) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadConnectRequest);
    base::xpacket_t packet;
    mgr1_->HandleMessage(message, packet);
}

TEST_F(TestRoutingTable2, kKadNatDetectRequest) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadNatDetectRequest);
    base::xpacket_t packet;
    mgr1_->HandleMessage(message, packet);
}

TEST_F(TestRoutingTable2, kKadNatDetectResponse) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadNatDetectResponse);
    base::xpacket_t packet;
    mgr1_->HandleMessage(message, packet);
}

TEST_F(TestRoutingTable2, kKadNatDetectHandshake2Node) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadNatDetectHandshake2Node);
    base::xpacket_t packet;
    mgr1_->HandleMessage(message, packet);
}

TEST_F(TestRoutingTable2, kKadNatDetectHandshake2Boot) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadNatDetectHandshake2Boot);
    base::xpacket_t packet;
    mgr1_->HandleMessage(message, packet);
}

TEST_F(TestRoutingTable2, kKadNatDetectFinish) {
    transport::protobuf::RoutingMessage message;
    message.set_type(kKadNatDetectFinish);
    base::xpacket_t packet;
    mgr1_->HandleMessage(message, packet);
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
