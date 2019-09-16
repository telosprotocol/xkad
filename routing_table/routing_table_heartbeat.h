// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// #pragma once

// #include <assert.h>
// #include <thread>
// #include <chrono>
// #include <functional>
// #include <mutex>

// #include "xpbase/base/top_log.h"
// #include "xpbase/base/top_timer.h"

// namespace top {
// namespace kadmlia {

// typedef std::function<void ()> Callback;
// typedef std::mutex Mutex;
// typedef std::lock_guard<Mutex> Guard;

// class RoutingTableHeartbeat {
// public:
//     void Start(Callback heartbeat_proc, Callback heartbeat_check_proc) {
//         TOP_INFO("[heartbeat] starting ...");
//         assert(heartbeat_proc);
//         assert(heartbeat_check_proc);
//         Guard guard(mutex_);

//         timer_.Start(10 * 1000, 1000 * 1000, heartbeat_proc);
//         timer_check_.Start(10 * 1000, 1000 * 1000, heartbeat_check_proc);
//         TOP_INFO("[heartbeat] started");
//     }

//     void Stop() {
//         TOP_INFO("[heartbeat] stopping ...");
//         Guard guard(mutex_);
//         timer_.Join();
//         timer_check_.Join();
//         TOP_INFO("[heartbeat] stopped");
//     }

// private:
//     Mutex mutex_;
//     Callback callback_;
//     base::TimerRepeated timer_;
//     base::TimerRepeated timer_check_;
// };

// }  // namespace kadmlia
// }  // namespace top
