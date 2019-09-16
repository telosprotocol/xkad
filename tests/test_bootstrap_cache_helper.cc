// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include <iostream>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/line_parser.h"
#include "xkad/routing_table/routing_utils.h"
#include "xpbase/base/kad_key/get_kadmlia_key.h"
#define private public
#include "xkad/routing_table/bootstrap_cache_helper.h"

namespace top {
namespace kadmlia {
namespace test {

class TestBootstrapCacheHelper : public testing::Test {
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

TEST_F(TestBootstrapCacheHelper, All) {
    auto helper = std::make_shared<BootstrapCacheHelper>(base::TimerManager::Instance());
    auto kad_key = base::GetKadmliaKey();
    kad_key->set_xnetwork_id(kRoot);
    auto get_public_nodes = [](std::vector<NodeInfoPtr>& vec){
        auto node_info = std::make_shared<NodeInfo>();
        node_info->public_ip = "1";
        node_info->public_port = 2;
        vec.push_back(node_info);
    };
    auto get_service_public_nodes = [](uint64_t service_type, std::vector<NodeInfoPtr>& vec){
        auto node_info = std::make_shared<NodeInfo>();
        node_info->public_ip = "3";
        node_info->public_port = 4;
        vec.push_back(node_info);
    };
    ASSERT_TRUE(helper->Start(kad_key, get_public_nodes, get_service_public_nodes));
    ASSERT_TRUE(helper->Start(kad_key, get_public_nodes, get_service_public_nodes));  // start again

    {
        std::vector<std::string> public_nodes;
        helper->GetPublicEndpoints(public_nodes);
        ASSERT_EQ(0u, public_nodes.size());
    }

    {
        std::set<std::pair<std::string, uint16_t>> boot_endpoints;
        helper->GetPublicEndpoints(boot_endpoints);
        ASSERT_EQ(0u, boot_endpoints.size());
    }
    
    {
        uint64_t service_type = kRoot;
        ASSERT_TRUE(helper->SetCacheServiceType(service_type));
        std::set<std::pair<std::string, uint16_t>> boot_endpoints;
        ASSERT_TRUE(helper->GetCacheServicePublicNodes(service_type, boot_endpoints));
        ASSERT_EQ(1u, boot_endpoints.size());
    }

    {
        helper->DumpPublicEndpoints();
    }

    {
        auto ret = helper->GetRandomCacheServiceType();
        ASSERT_EQ(top::kRoot, ret);
    }

    {
        helper->RepeatCacheServicePublicNodes();
    }

    helper->Stop();
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
