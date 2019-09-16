// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#include "xpbase/base/top_log.h"
#include "xtransport/udp_transport/udp_transport.h"
#include "xtransport/message_manager/multi_message_handler.h"
#include "xpbase/base/top_utils.h"
#define private public
#define protected public
#include "xkad/src/nat_manager.h"

namespace top {

namespace kadmlia {

namespace test {

class TestNatManager : public testing::Test {
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

TEST_F(TestNatManager, NatManagerIntf) {
	NatManagerIntf* p = new NatManager();
	delete p;
	p = nullptr;
}

TEST_F(TestNatManager, All) {
	ASSERT_NE(nullptr, NatManagerIntf::Instance());

	auto manager = std::make_shared<NatManager>();
	auto message_handler = std::make_shared<transport::MultiThreadHandler>();
	message_handler->Init();
	auto transport = std::make_shared<transport::UdpTransport>();
	transport->Start("127.0.0.1", 0, message_handler.get());
	auto nat_transport = std::make_shared<transport::UdpTransport>();
	nat_transport->Start("127.0.0.1", 0, message_handler.get());

	// TODO: need first node
	{
		bool first_node = true;
        const std::set<std::pair<std::string, uint16_t>> boot_endpoints{{"127.0.0.1", 9000}};
		ASSERT_TRUE(manager->Start(
				first_node,
				boot_endpoints,
				message_handler.get(),
				transport.get(),
				nat_transport.get()));
		
		
	}

	{
		int32_t nat_type;
		ASSERT_TRUE(manager->GetLocalNatType(nat_type));
		// ASSERT_EQ(kNatTypeUnknown, nat_type);  // TODO:
	}

	{
		manager->SetNatTypeAndNotify(kNatTypePublic);
	}

	{
		transport::protobuf::RoutingMessage message;
		base::xpacket_t packet;
		manager->HandleNatDetectRequest(message, packet);
		manager->HandleNatDetectResponse(message, packet);
		manager->HandleNatDetectHandshake2Node(message, packet);
		manager->HandleNatDetectHandshake2Boot(message, packet);
		manager->HandleNatDetectFinish(message, packet);
	}

	{
		auto message = std::make_shared<transport::protobuf::RoutingMessage>();
		auto packet = std::make_shared<base::xpacket_t>();
		manager->PushMessage(message, packet);
		manager->PushMessage(*message, *packet);
		// manager->ThreadProc();
	}

	{
		manager->SendNatDetectRequest("1234", 1234);
		manager->SendNatDetectFinish("1234", 1234);
	}

	manager->OnTimerNatDetectTimeout();
	manager->ThreadJoin();
	manager->ThreadProc();
	manager->Stop();

	nat_transport->Stop();
	transport->Stop();
	TOP_DEBUG("stoping all objectes");

	manager = nullptr;
	nat_transport = nullptr;
	transport = nullptr;
	message_handler = nullptr;

	SleepUs(2 * 1000 * 1000);
	TOP_DEBUG("stopped all objectes");
}


}  // namespace test

}  // namespace kadmlia

}  // namespace top
