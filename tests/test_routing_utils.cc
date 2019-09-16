// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <string.h>

#include <string>

#include <gtest/gtest.h>

#include "xkad/routing_table/routing_utils.h"
#include "xpbase/base/xid/xid_def.h"

namespace top {

namespace kadmlia {

namespace test {

class TestRoutingUtil : public testing::Test {
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

TEST_F(TestRoutingUtil, TestAll) {
	random_number_generator_mutex();
	std::string str(RandomString(256));
    std::string hex_enc = HexEncode(str);
    std::string hex_dec = HexDecode(hex_enc);
    ASSERT_EQ(hex_dec, str);
    HexSubstr(str);
	
    std::string base_enc = Base64Encode(str);
    std::string base_dec = Base64Decode(base_enc);
    ASSERT_EQ(base_dec, str);
    Base64Substr(str);
}

TEST_F(TestRoutingUtil, RandomInt32) {
    RandomInt32();
    RandomUint32();
}

TEST_F(TestRoutingUtil, toupper) {
    std::string test_str = "sdfasdfASFasdfsdfa0.HFSDFDS>%$#@*&<]}~`0sdf90";
    toupper(test_str);
    ASSERT_EQ(test_str, "SDFASDFASFASDFSDFA0.HFSDFDS>%$#@*&<]}~`0SDF90");
}

TEST_F(TestRoutingUtil, tolower) {
    std::string test_str = "sdfasdfASFasdfsdfa0.HFSDFDS>%$#@*&<]}~`0sdf90";
    tolower(test_str);
    ASSERT_EQ(test_str, "sdfasdfasfasdfsdfa0.hfsdfds>%$#@*&<]}~`0sdf90");
}

TEST_F(TestRoutingUtil, GenRandomID_bussiness) {
    std::string conuntry = "CN";
    std::string business = "XVPN";
    std::string id = GenRandomID(conuntry, business);
    ASSERT_FALSE(id.empty());
}

TEST_F(TestRoutingUtil, GenNodeIdType_service_type1) {
    uint8_t country_code = 1;
    uint8_t service_type = 2;
    auto ret = GenNodeIdType(country_code, service_type);
    ASSERT_EQ(4u, ret.size());
}

TEST_F(TestRoutingUtil, GenNodeIdType_service_type2) {
    uint8_t country_code = 1;
    uint32_t service_type = 2;
    auto ret = GenNodeIdType(country_code, service_type);
    ASSERT_EQ(4u, ret.size());
}

TEST_F(TestRoutingUtil, GenNodeIdType_bussiness) {
    {
        std::string country_code;
        std::string bussiness;
        auto ret = GenNodeIdType(country_code, bussiness);
        ASSERT_EQ(0u, ret.size());
    }

    {
        std::string country_code = "INVALID";
        std::string bussiness;
        auto ret = GenNodeIdType(country_code, bussiness);
        ASSERT_EQ(0u, ret.size());
    }

    {
        std::string country_code = "CN";
        std::string bussiness;
        auto ret = GenNodeIdType(country_code, bussiness);
        ASSERT_EQ(0u, ret.size());
    }

    {
        std::string country_code = "CN";
        std::string bussiness = "ROOT";
        auto ret = GenNodeIdType(country_code, bussiness);
        ASSERT_EQ(4u, ret.size());
    }
}

TEST_F(TestRoutingUtil, GenRandomID_service) {
    uint8_t country_code = 1;
    uint8_t service_type = top::kInvalidType;
    auto ret = GenRandomID(country_code, service_type);
    ASSERT_EQ(kNodeIdSize, ret.size());  // regard country_code as zone_id, service_type as network_id
}

TEST_F(TestRoutingUtil, GetNetworkId) {
    {
        std::string id;
        uint32_t network_id = 0;
        ASSERT_FALSE(GetNetworkId(id, network_id));
    }

    {
        std::string id;
        id.resize(kNodeIdSize);
        id[1] = 0xf0;
        uint32_t network_id = 0;
        ASSERT_TRUE(GetNetworkId(id, network_id));
        ASSERT_EQ(0xf0, network_id);
    }
}

TEST_F(TestRoutingUtil, GetPublicEndpointsConfig) {
    {
        TOP_INFO("GetPublicEndpointsConfig_1");
        base::Config config;
        std::set<std::pair<std::string, uint16_t>> boot_endpoints;
        GetPublicEndpointsConfig(config, boot_endpoints);
        ASSERT_EQ(0u, boot_endpoints.size());
    }

    {
        TOP_INFO("GetPublicEndpointsConfig_2");
        base::Config config;
        config.Set("node", "public_endpoints", "1:2, 3:4");
        std::set<std::pair<std::string, uint16_t>> boot_endpoints;
        GetPublicEndpointsConfig(config, boot_endpoints);
        ASSERT_EQ(2u, boot_endpoints.size());
    }
}

TEST_F(TestRoutingUtil, GetPublicServiceEndpointsConfig) {
    {
        TOP_INFO("GetPublicServiceEndpointsConfig_1");
        base::Config config;
        std::string service_name;
        std::set<std::pair<std::string, uint16_t>> boot_endpoints;
        GetPublicServiceEndpointsConfig(config, service_name, boot_endpoints);
        ASSERT_EQ(0u, boot_endpoints.size());
    }

    {
        TOP_INFO("GetPublicServiceEndpointsConfig_2");
        base::Config config;
        std::string service_name = "vpn";
        config.Set(service_name, "bootstrap", "1:2,3:4");
        std::set<std::pair<std::string, uint16_t>> boot_endpoints;
        GetPublicServiceEndpointsConfig(config, service_name, boot_endpoints);
        ASSERT_EQ(2u, boot_endpoints.size());
    }
}

TEST_F(TestRoutingUtil, GetAllPublicServiceEndpointsConfig) {
    base::Config config;
    std::string service_list = "vpn,storage";
    config.Set("vpn", "bootstrap", "1:2,3:4");
    config.Set("storage", "bootstrap", "5:6,7:8");
    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    GetAllPublicServiceEndpointsConfig(config, service_list, boot_endpoints);
    ASSERT_EQ(4u, boot_endpoints.size());
}

TEST_F(TestRoutingUtil, GetXNetworkID_GetZoneID) {
    const uint32_t NET_ID = 1;
    const uint8_t ZONE_ID = 2;
    base::XID xid(NET_ID, ZONE_ID, "pubkey", "prikey");
    auto str_id = xid.ToString();
    auto net_id = GetXNetworkID(str_id);
    ASSERT_EQ(NET_ID, net_id);

    auto zone_id = GetZoneID(str_id);
    ASSERT_EQ(ZONE_ID, zone_id);
}


}  // namespace test

}  // namespace kadmlia

}  // namespace top
