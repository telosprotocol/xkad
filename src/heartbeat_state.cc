// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


// #include "xkad/routing_table/heartbeat_state.h"

// #include <assert.h>
// #include <time.h>

// #include <utility>
// #include <string>
// #include <map>
// #include <mutex>
// #include <thread>
// #include <chrono>
// #include <iostream>
// #include <functional>

// #include "xpbase/base/top_log.h"
// #include "xpbase/base/top_timer.h"
// #include "xkad/routing_table/routing_utils.h"
// #include "xtransport/proto/transport.pb.h"
// #include "xkad/proto/kadmlia.pb.h"

// namespace top {
// namespace kadmlia {

// typedef std::chrono::system_clock Clock;
// typedef std::mutex Mutex;
// typedef std::lock_guard<Mutex> Guard;
// typedef std::function<void ()> Callback;

// class HeartbeatStateImpl : public HeartbeatState {
// public:
//     explicit HeartbeatStateImpl(const std::string &type) {
//         Init();
//         type_ = type;
//     }

//     // void Join() override {
//     //     if (timer_ != nullptr) {
//     //         timer_->close();
//     //         timer_->release_ref();
//     //         timer_ = nullptr;
//     //     }

//     //     if (iothread_ != nullptr) {
//     //         iothread_->close();
//     //         iothread_->release_ref();
//     //         iothread_ = nullptr;
//     //     }
//     // }

//     // req
//     void Send(const transport::protobuf::RoutingMessage& message, const std::string& peer_ip,
//             uint16_t peer_port) override {
//         if (message.type() !=  kKadHeartbeatRequest &&
//                 message.type() !=  kKadHeartbeatResponse)
//             return;

//         static bool flag_message_size = false;
//         if (!flag_message_size) {
//             flag_message_size = true;
//             TOP_INFO("[ht_state] heartbeat request message size: %d bytes",
//                 static_cast<int>(message.ByteSizeLong()));
//         }

//         Guard guard(mutex_);
//         ++sum_1_;
//     }

//     void Recv(const transport::protobuf::RoutingMessage& message) {
//         if (message.type() !=  kKadHeartbeatRequest &&
//                 message.type() !=  kKadHeartbeatResponse)
//             return;

//         static bool flag_message_size = false;
//         if (!flag_message_size) {
//             flag_message_size = true;
//             TOP_INFO("[ht_state] heartbeat response message size: %d bytes",
//                 static_cast<int>(message.ByteSizeLong()));
//         }

//         Guard guard(mutex_);
//         ++sum_1_;
//     }

//     void DumpAndReset_1() {
//         TOP_DEBUG("[ht_state] %s sum_1: %d", type_.c_str(), sum_1_);
//         sum_1_ = 0;
//     }

//     void DumpAndReset_5() {
//         if (n_calls_ > 0 && n_calls_ % 5 == 0) {
//             TOP_DEBUG("[ht_state] %s sum_5: %d", type_.c_str(), sum_5_);
//             sum_5_ = 0;
//         }
//     }

//     void DumpAndReset_30() {
//         if (n_calls_ > 0 && n_calls_ % 30 == 0) {
//             TOP_DEBUG("[ht_state] %s sum_30: %d", type_.c_str(), sum_30_);
//             sum_30_ = 0;
//         }
//     }

// private:
//     void TimerProc() {
//         Guard guard(mutex_);
//         // sum_1_
//         sum_5_ += sum_1_;
//         sum_30_ += sum_1_;
//         ++n_calls_;

//         // TOP_INFO("[ht_state] ========== begin ========== ");
//         DumpAndReset_1();
//         DumpAndReset_5();
//         DumpAndReset_30();
//         // TOP_INFO("[ht_state] ========== end ========== ");
//     }

//     // init
//     void Init() {
//         TOP_INFO("[ht_state] initing ...");

//         timer_->Start(
//                 10 * 1000, 1000 * 1000,
//                 std::bind(&HeartbeatStateImpl::TimerProc, this));
//         TOP_INFO("[ht_state] inited");
//     }

// private:
//     Mutex mutex_;
//     std::string type_;
//     int n_calls_{0};
//     int sum_1_{0};  // 1 unit
//     int sum_5_{0};  // 5 units
//     int sum_30_{0};  // 30 units

//     std::shared_ptr<base::TimerRepeated> timer_{std::make_shared<base::TimerRepeated>()};
// };

// HeartbeatState& HeartbeatState::Instance() {
//     static HeartbeatStateImpl ins("send");
//     return ins;
// }

// class HeartbeatStateRecvImpl : public HeartbeatStateRecv {
// public:
//     void Recv(const transport::protobuf::RoutingMessage& message) override {
//         recv_state_.Recv(message);
//     }
//     // void Join() override {
//     //     recv_state_.Join();
//     // }

// private:
//     HeartbeatStateImpl recv_state_{"recv"};
// };

// HeartbeatStateRecv& HeartbeatStateRecv::Instance() {
//     static HeartbeatStateRecvImpl ins;
//     return ins;
// }

// }  // namespace kadmlia
// }  // namespace top
