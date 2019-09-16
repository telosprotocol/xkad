// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <string>

#include "xpbase/base/top_string_util.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/routing_table/routing_utils.h"

namespace top {

namespace gossip {

static const int32_t kDefautHopCount = 10;
static const int32_t kDefaultSpreadPeriod = 3 * 1000 * 1000;
static const int32_t kDefaultFilterSize = 3024000u;
static const int32_t kRepeatedValueNum = 1;

struct RumorIdentity final : public kadmlia::MessageIdentity {
    RumorIdentity(
        const uint32_t in_message_id,
        const std::string& in_node_id) 
        :kadmlia::MessageIdentity(in_message_id,in_node_id) {
    }
    ~RumorIdentity() {}
};

}
}
