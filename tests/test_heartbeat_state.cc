// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// #include <string.h>

// #include <string>

// #include <gtest/gtest.h>
// #include <unistd.h>

// #include "xkad/routing_table/heartbeat_state.h"
// #include "xkad/routing_table/routing_utils.h"
// #include "xtransport/proto/transport.pb.h"
// #include "xwrouter/wrouter_utils/wrouter_utils.h"

// namespace top {

// namespace kadmlia {

// namespace test {

// class TestHeartbeatState : public testing::Test {
// public:
//     static void SetUpTestCase() {

//     }

//     static void TearDownTestCase() {
//     }

//     virtual void SetUp() {

//     }

//     virtual void TearDown() {
//     }
// };

// TEST_F(TestHeartbeatState, HeartbeatState) {
//     transport::protobuf::RoutingMessage message;
//     message.set_type(kKadHeartbeatRequest);
//     HeartbeatState::Instance().Send(message, "1", 2);
//     HeartbeatStateRecv::Instance().Recv(message);
//     ::sleep(2);
// }

// }  // namespace test

// }  // namespace kadmlia

// }  // namespace top
