// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include <iostream>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/line_parser.h"
#include "xkad/routing_table/routing_utils.h"
#define private public
#include "xkad/routing_table/bootstrap_cache.h"

namespace top {
namespace kadmlia {
namespace test {

class TestBootstrapCache : public testing::Test {
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

TEST_F(TestBootstrapCache, GetCache) {
    BootstrapCacheManager manager;
    ASSERT_TRUE(manager.Init());
    ASSERT_TRUE(manager.Init());  // init again
    auto cache = manager.GetBootStrapCache(1);
    ASSERT_NE(nullptr, cache);
    VecBootstrapEndpoint vec_bootstrap_endpoint = {
        {"1.1.1.1", 11111},
        {"2.2.2.2", 22222}
    };
    ASSERT_TRUE(cache->SetCache(vec_bootstrap_endpoint));
    VecBootstrapEndpoint vec2;
    ASSERT_TRUE(cache->GetCache(vec2));
//     ASSERT_EQ(vec_bootstrap_endpoint, vec2);
}

TEST_F(TestBootstrapCache, GetBootstrapCache) {
    const uint64_t service_type = kRoot;
    auto ptr = GetBootstrapCache(service_type);
    ASSERT_NE(nullptr, ptr);
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
