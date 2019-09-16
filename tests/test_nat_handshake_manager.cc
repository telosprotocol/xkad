// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#include "xtransport/udp_transport/udp_transport.h"
#define private public
#define protected public
#include "xkad/nat_detect/nat_handshake_manager.h"

namespace top {

namespace kadmlia {

namespace test {

class TestNatHandshakeManager : public testing::Test {
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

TEST_F(TestNatHandshakeManager, All) {
	NatHandshakeManager manager(base::TimerManager::Instance());

	ASSERT_TRUE(manager.Start());

	{
		std::string ip = "1234";
		uint16_t port = 12345;
		auto transport = std::make_shared<transport::UdpTransport>();
		uint16_t detect_port = 5555;
		int32_t message_type = kKadNatDetectHandshake2Boot;
		int32_t delay_ms = 0;
		manager.AddDetection(ip, port, transport.get(), detect_port, message_type, delay_ms);

		manager.DetectProc();

		manager.RemoveDetection(ip, port);
	}

	manager.Stop();
}


}  // namespace test

}  // namespace kadmlia

}  // namespace top
