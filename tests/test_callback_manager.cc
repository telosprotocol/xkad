// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#include "xbase/xpacket.h"
#define private public
#include "xkad/routing_table/callback_manager.h"

namespace top {

namespace kadmlia {

namespace test {

class TestCallbackManager : public testing::Test {
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

TEST_F(TestCallbackManager, Init) {
    CallbackManager callback_mgr;
}

TEST_F(TestCallbackManager, Add1) {
    CallbackManager callback_mgr;
    callback_mgr.Add(1, 5, nullptr, 1);
    auto iter = callback_mgr.callback_map_.find(1);
    ASSERT_FALSE(iter == callback_mgr.callback_map_.end());
}

TEST_F(TestCallbackManager, Add2) {
    CallbackManager callback_mgr;
    CallbackItemPtr item_ptr;
    item_ptr.reset(new CallbackItem{ 2, NULL, nullptr, 4, 1, nullptr, nullptr });
    callback_mgr.Add(item_ptr);
    auto iter = callback_mgr.callback_map_.find(2);
    ASSERT_FALSE(iter == callback_mgr.callback_map_.end());
}

TEST_F(TestCallbackManager, Callback1) {
    CallbackManager callback_mgr;

    std::mutex mutex;
    std::condition_variable cond_var;
    int called_times = 0;
    auto callback = [&called_times](
            int status, transport::protobuf::RoutingMessage & tmp_message, base::xpacket_t& packet) {
        ++called_times;
    };
    callback_mgr.Add(1, 5, callback, 5);
    auto iter = callback_mgr.callback_map_.find(1);
    ASSERT_FALSE(iter == callback_mgr.callback_map_.end());
    top::transport::protobuf::RoutingMessage message;
    base::xpacket_t packet;
    callback_mgr.Callback(1, message, packet);
    callback_mgr.Callback(1, message, packet);
    callback_mgr.Callback(1, message, packet);
    callback_mgr.Callback(1, message, packet);
    callback_mgr.Callback(1, message, packet);
    callback_mgr.Callback(1, message, packet);
    callback_mgr.Callback(1, message, packet);
    ASSERT_EQ(called_times, 5);
}

TEST_F(TestCallbackManager, Callback2) {
    CallbackManager callback_mgr;
    std::shared_ptr<std::mutex> pmutex = std::make_shared<std::mutex>();
    std::shared_ptr<std::condition_variable> cond_var = std::make_shared<std::condition_variable>();
    int called_times = 0;
    auto callback = [&called_times](
            int status,
            transport::protobuf::RoutingMessage& tmp_message,
            base::xpacket_t& packet,
            std::shared_ptr<std::mutex> pmutex,
            std::shared_ptr<std::condition_variable> cond_var) {
        ++called_times;
    };
    CallbackItemPtr item_ptr;
    item_ptr.reset(new CallbackItem{ 2, NULL, callback, 4, 5, pmutex, cond_var });
    callback_mgr.Add(item_ptr);
    auto iter = callback_mgr.callback_map_.find(2);
    ASSERT_FALSE(iter == callback_mgr.callback_map_.end());
    top::transport::protobuf::RoutingMessage message;
    base::xpacket_t packet;
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    callback_mgr.Callback(2, message, packet);
    ASSERT_EQ(called_times, 5);
}

TEST_F(TestCallbackManager, Timeout1) {
    CallbackManager callback_mgr;

    std::mutex mutex;
    std::condition_variable cond_var;
    auto callback = [](
        int status, transport::protobuf::RoutingMessage & tmp_message, base::xpacket_t& packet) {};
    callback_mgr.Add(1, 5, callback, 1);
    auto iter = callback_mgr.callback_map_.find(1);
    ASSERT_FALSE(iter == callback_mgr.callback_map_.end());
    top::transport::protobuf::RoutingMessage message;
    base::xpacket_t packet;
    callback_mgr.Timeout(1);
}

TEST_F(TestCallbackManager, Timeout2) {
    CallbackManager callback_mgr;
    std::shared_ptr<std::mutex> pmutex = std::make_shared<std::mutex>();
    std::shared_ptr<std::condition_variable> cond_var = std::make_shared<std::condition_variable>();
    // int res = kKadFailed;
    auto callback = [](
        int status,
        transport::protobuf::RoutingMessage& tmp_message,
        base::xpacket_t& packet,
        std::shared_ptr<std::mutex> pmutex,
        std::shared_ptr<std::condition_variable> cond_var) {};
    CallbackItemPtr item_ptr;
    item_ptr.reset(new CallbackItem{ 2, NULL, callback, 4, 1, pmutex, cond_var });
    callback_mgr.Add(item_ptr);
    auto iter = callback_mgr.callback_map_.find(2);
    ASSERT_FALSE(iter == callback_mgr.callback_map_.end());
    top::transport::protobuf::RoutingMessage message;
    base::xpacket_t packet;
    callback_mgr.Timeout(2);
}

TEST_F(TestCallbackManager, Instance) {
    ASSERT_NE(nullptr, CallbackManager::Instance());
}

TEST_F(TestCallbackManager, TimeoutCheck) {
    CallbackManager callback_mgr;
    callback_mgr.TimeoutCheck();
}


}  // namespace test

}  // namespace kadmlia

}  // namespace top
