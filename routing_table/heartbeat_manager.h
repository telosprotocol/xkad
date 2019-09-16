// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <string>
#include <functional>
#include <mutex>
#include <vector>

namespace top {
namespace kadmlia {

using OfflineCallback = std::function<void(const std::string& ip, uint16_t port)>;

class HeartbeatManagerIntf {
public:
    static HeartbeatManagerIntf* Instance();
    static void OnHeartbeatCallback(const std::string& ip, uint16_t port);

    virtual ~HeartbeatManagerIntf() {}
    virtual void Register(const std::string& name, OfflineCallback cb) = 0;

private:
    virtual void OnHeartbeatFailed(const std::string& ip, uint16_t port) = 0;
};

class HeartbeatManager : public HeartbeatManagerIntf {
public:
    virtual void Register(const std::string& name, OfflineCallback cb) override;
    virtual void OnHeartbeatFailed(const std::string& ip, uint16_t port) override;

private:
    std::mutex mutex_;
    std::vector<OfflineCallback> vec_cb_;
};

}  // namespace kadmlia
}  // namespace top
