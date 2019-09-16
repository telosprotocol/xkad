// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// #pragma once

// #include <assert.h>
// #include <stdint.h>
// #include <string>
// #include "xtransport/proto/transport.pb.h"
// #include "xkad/proto/kadmlia.pb.h"

// namespace top {
// namespace kadmlia {

// class HeartbeatState {
// public:
//     static HeartbeatState& Instance();
//     virtual void Send(const transport::protobuf::RoutingMessage& message,
//         const std::string& peer_ip, uint16_t peer_port) = 0;
//     // virtual void Join() = 0;
// };

// class HeartbeatStateRecv {
// public:
//     static HeartbeatStateRecv& Instance();
//     virtual void Recv(const transport::protobuf::RoutingMessage& message) = 0;
//     // virtual void Join() = 0;
// };

// }  // namespace kadmlia
// }  // namespace top
