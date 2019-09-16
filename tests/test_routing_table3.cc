// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdio.h>
#include <string.h>

#include <string>
#include <memory>
#include <fstream>
#include <bitset>

#include <gtest/gtest.h>

#define private public
#define protected public
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/local_node_info.h"
#include "xpbase/base/kad_key/kadmlia_key.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"

namespace top {
namespace kadmlia {
namespace test {

class TestRoutingTable3 : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {}

protected:
};

class MockKadKey : public base::PlatformKadmliaKey {
public:
    virtual std::string Get() override {
        return std::string(kNodeIdSize, '\x00');
    }
};

const int kBits = kNodeIdSize * 8;

static std::string FromBitset(const std::bitset<kBits>& bits) {
    std::string node_id;
    node_id.resize(kNodeIdSize);
    for (int i = 0; i < kNodeIdSize; ++i) {
        std::bitset<8> byte_bits;
        for (int k = 0; k < 8; ++k) {
            byte_bits[k] = bits[(kNodeIdSize - i - 1) * 8 + k];
        }
        node_id[i] = (char)byte_bits.to_ulong();
    }
    return node_id;
}

static void ToBitset(const std::string& node_id, std::bitset<kBits>& bits) {
    assert(node_id.size() == kNodeIdSize);
    for (int i = 0; i < kNodeIdSize; ++i) {
        std::bitset<8> byte_bits((uint8_t)node_id[i]);
        for (int k = 0; k < 8; ++k) {
            bits[(kNodeIdSize - i - 1) * 8 + k] = byte_bits[k];
        }
    }
}

TEST_F(TestRoutingTable3, SetNodeBucket_1) {
    auto rt = std::make_shared<RoutingTable>(nullptr, 0, nullptr);
    auto kad_key = std::make_shared<MockKadKey>();
    auto local_node = std::make_shared<LocalNodeInfo>();
    local_node->kadmlia_key_ = kad_key;
    rt->local_node_ptr_ = local_node;

    // same id
    {
        auto node = std::make_shared<NodeInfo>();
        node->node_id = local_node->id();
        ASSERT_EQ(kKadFailed, rt->SetNodeBucket(node));
        ASSERT_EQ(kSelfBucketIndex, node->bucket_index);
    }

    // for every bucket(from the last bit)
    for (int i = 0; i < kNodeIdSize; ++i) {
        for (int k = 0; k < 8; ++k) {
            auto node = std::make_shared<NodeInfo>();
            node->node_id = local_node->id();

            std::bitset<8> bits((uint8_t)node->node_id[kNodeIdSize - i - 1]);

            // bits[k].flip();
            // random the bits after k
            for (int p = 0; p <= k; ++p) {
                bits[p].flip();  // any bits after k, will not affect the bucket index!!
            }
            node->node_id[kNodeIdSize - i - 1] = (char)bits.to_ulong();
            // TOP_FATAL("i = %d, k = %d", i, k);
            // TOP_FATAL("local_id: %s", HexEncode(local_node->id()).c_str());
            // TOP_FATAL("node_id : %s", HexEncode(node->node_id).c_str());
            ASSERT_EQ(kKadSuccess, rt->SetNodeBucket(node));
            const int bucket_index = i * 8 + k + 1;
            // const int bucket_index = i * 8 + k;  // from 0 to 36 * 8 - 1?
            ASSERT_EQ(bucket_index, node->bucket_index);
        }
    }
}

// TEST_F(TestRoutingTable3, NewNodeReplaceOldNode_1) {
//     auto rt = std::make_shared<RoutingTable>(nullptr, 0, nullptr);
//     auto kad_key = std::make_shared<MockKadKey>();
//     auto local_node = std::make_shared<LocalNodeInfo>();
//     local_node->kadmlia_key_ = kad_key;
//     rt->local_node_ptr_ = local_node;

//     for (int i = 1; i <= 3; ++i) {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         if (i == 1) {
//             node->node_id[1] = 0x1;
//         } else {
//             node->node_id[kNodeIdSize-1] = (char)i;  // 0x02/0x03
//         }
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }

//     // fill routing table with the same k-bucket node
//     for (int i = 0; i < kRoutingMaxNodesSize - 3; ++i) {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.set(256);
//         node->node_id = FromBitset(bits);
//         node->node_id[kNodeIdSize-1] = (char)(i % 128);
//         node->node_id[kNodeIdSize-2] = (char)(i / 128);
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }

//     // so, routing table is temp full
//     ASSERT_EQ(kRoutingMaxNodesSize, rt->nodes_.size()); 

//     // for k-bucket 280
//     // still can add node which in empty k-bucket
//     {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.set(280);
//         node->node_id = FromBitset(bits);
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }

//     // can replace with far node which is k-bucket is empty
//     {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.set(280);
//         node->node_id = FromBitset(bits);
//         node->node_id[kNodeIdSize-1] = 0x1;
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }

//     // for k-bucket 240
//     // still can add node which in empty k-bucket
//     {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.set(240);
//         node->node_id = FromBitset(bits);
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }

//     // can replace with closer node
//     {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.set(240);
//         node->node_id = FromBitset(bits);
//         node->node_id[kNodeIdSize-1] = 0x1;
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }
// }

// TEST_F(TestRoutingTable3, NewNodeReplaceOldNode_2) {
//     std::string node_id_0;
//     {
//         std::bitset<kBits> bits;
//         bits.set(0);
//         node_id_0 = FromBitset(bits);
//         TOP_FATAL("node_id_0: %s", HexEncode(node_id_0).c_str());
//     }
//     std::string node_id_287;
//     {
//         std::bitset<kBits> bits;
//         bits.set(287);
//         node_id_287 = FromBitset(bits);
//         TOP_FATAL("node_id_287: %s", HexEncode(node_id_287).c_str());
//     }

//     {
//         std::string node_id(kNodeIdSize, '\xf0');
//         std::bitset<kBits> bits;
//         ToBitset(node_id, bits);
//         auto node_id_2 = FromBitset(bits);
//         TOP_FATAL("node_id1: %s", HexEncode(node_id).c_str());
//         TOP_FATAL("node_id2: %s", HexEncode(node_id_2).c_str());
//     }
    
// }

// // add node for all bucket
// TEST_F(TestRoutingTable3, NewNodeReplaceOldNode_4) {
//     auto rt = std::make_shared<RoutingTable>(nullptr, 0, nullptr);
//     auto kad_key = std::make_shared<MockKadKey>();
//     auto local_node = std::make_shared<LocalNodeInfo>();
//     local_node->kadmlia_key_ = kad_key;
//     rt->local_node_ptr_ = local_node;

//     for (int i = 0; i < kRoutingMaxNodesSize; ++i) {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.flip(i);  // bucket(kBits - 1)
//         node->node_id = FromBitset(bits);
//         // TOP_FATAL("node_id[%3d] = %s", i, HexEncode(node->node_id).c_str());
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_TRUE(rt->CanAddNode(node));
//         ASSERT_EQ(kKadSuccess, rt->AddNode(node));
//     }

//     TOP_FATAL("---------------- checking node_id ---------------- ");
//     for (int i = 1; i < kRoutingMaxNodesSize; ++i) {
//         auto node = std::make_shared<NodeInfo>();
//         node->node_id = local_node->id();
//         std::bitset<kBits> bits;
//         ToBitset(node->node_id, bits);
//         bits.flip(i);  // bucket(kBits - 1)
//         bits.flip(0);  // make sure the id is not in the routing table
//         node->node_id = FromBitset(bits);
//         // TOP_FATAL("node_id[%3d] = %s", i, HexEncode(node->node_id).c_str());
//         node->nat_type = kNatTypePublic;
//         node->public_ip = "abcd";
//         node->public_port = 1234;
//         ASSERT_FALSE(rt->CanAddNode(node));
//     }
// }

}  // namespace test
}  // namespace kadmlia
}  // namespace top
