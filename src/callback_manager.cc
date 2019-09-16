// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/callback_manager.h"

#include <vector>
#include <chrono>

#include "xbase/xpacket.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_log.h"
#include "xkad/routing_table/routing_table.h"

namespace top {

namespace kadmlia {

static const int32_t kTimeCheckoutPeriod = 1000 * 1000;  // 1s

CallbackItem::~CallbackItem() {}

std::atomic<uint32_t> CallbackManager::msg_id_(time(0));

uint32_t CallbackManager::MessageId() {
    return ++msg_id_;
}

CallbackManager::CallbackManager()
        : callback_map_(),
          callback_map_mutex_() {
    timer_.CallAfter(kTimeCheckoutPeriod, std::bind(&CallbackManager::TimeoutCheck, this));
}

CallbackManager::~CallbackManager() {
    Join();
    TOP_INFO("CallbackManager thread joined!");
}

CallbackManager* CallbackManager::Instance() {
    static CallbackManager ins;
    return &ins;
}

void CallbackManager::Join() {
    timer_.Join();
    {
        std::unique_lock<std::mutex> lock(callback_map_mutex_);
        callback_map_.clear();
    }
}

void CallbackManager::Add(
        uint32_t message_id,
        int32_t timeout_sec,
        ResponseFunctor callback,
        int32_t expect_count) {
    CallbackItemPtr item_ptr;
    item_ptr.reset(new CallbackItem{
        message_id, callback, nullptr,
        timeout_sec, expect_count, nullptr, nullptr });
    std::unique_lock<std::mutex> lock(callback_map_mutex_);
    callback_map_.insert(std::make_pair(message_id, item_ptr));
}

void CallbackManager::Add(CallbackItemPtr callback_ptr) {
    if (!callback_ptr) {
        return;
    }

    std::unique_lock<std::mutex> lock(callback_map_mutex_);
    callback_map_.insert(std::make_pair(callback_ptr->message_id, callback_ptr));
}

void CallbackManager::Callback(
        uint32_t message_id,
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    CallbackItemPtr item_ptr;
    {
        std::unique_lock<std::mutex> lock(callback_map_mutex_);
        auto iter = callback_map_.find(message_id);
        if (iter == callback_map_.end()) {
            return;
        }

        item_ptr = iter->second;
        iter->second->expect_count--;
        if (iter->second->expect_count <= 0) {
            callback_map_.erase(iter);
        }
    }

    if (item_ptr) {
        if (item_ptr->callback) {
            item_ptr->callback(kKadSuccess, message, packet);
        }

        if (item_ptr->mutex_callback) {
            item_ptr->mutex_callback(
                kKadSuccess,
                message,
                packet,
                item_ptr->wait_mutex,
                item_ptr->wait_condition);
        }
    }
}

void CallbackManager::Timeout(uint32_t message_id) {
    Cancel(message_id, 0);
}

// if no_callback is 0, call callback. if no_callback is 1, do not call callback
void CallbackManager::Cancel(uint32_t message_id, uint32_t no_callback) {
    ResponseFunctor callback;
    CallbackItemPtr mutex_callback_item;
    int32_t expect_count = 0;
    {
        std::unique_lock<std::mutex> lock(callback_map_mutex_);
        auto iter = callback_map_.find(message_id);
        if (iter == callback_map_.end()) {
            return;
        }

        callback = iter->second->callback;
        mutex_callback_item = iter->second;
        expect_count = iter->second->expect_count;
        callback_map_.erase(iter);
    }

    if (no_callback != 0)
        return;
    if (callback) {
        transport::protobuf::RoutingMessage message;
		message.set_id(message_id);
        base::xpacket_t packet;
        for (int i = 0; i < expect_count; ++i) {
            callback(kKadTimeout, message, packet);
        }
    }

    if (mutex_callback_item->mutex_callback) {
        transport::protobuf::RoutingMessage message;
        base::xpacket_t packet;
        for (int i = 0; i < expect_count; ++i) {
            mutex_callback_item->mutex_callback(
                    kKadTimeout,
                    message,
                    packet,
                    mutex_callback_item->wait_mutex,
                    mutex_callback_item->wait_condition);
        }
    }
}

void CallbackManager::TimeoutCheck() {
    std::vector<uint32_t> message_vec;
    {
        std::unique_lock<std::mutex> lock(callback_map_mutex_);
        for (auto iter = callback_map_.begin(); iter != callback_map_.end(); ++iter) {
            iter->second->timeout_sec--;
            if (iter->second->timeout_sec <= 0) {
                message_vec.push_back(iter->first);
            }
        }
    }

    for (uint32_t i = 0; i < message_vec.size(); ++i) {
        Timeout(message_vec[i]);
    }
    timer_.CallAfter(kTimeCheckoutPeriod, std::bind(&CallbackManager::TimeoutCheck, this));
}

}  // namespace kadmlia

}  // namespace top
