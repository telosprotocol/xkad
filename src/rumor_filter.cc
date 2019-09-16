// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/gossip/rumor_filter.h"

#include "xkad/gossip/rumor_def.h"
#include "xkad/routing_table/routing_utils.h"
#include "xpbase/base/top_log.h"

namespace top {
namespace gossip {

RumorFilter* RumorFilter::Instance() {
    static RumorFilter ins;
    return &ins;
}

bool RumorFilter::FiltMessage(
    transport::protobuf::RoutingMessage& message) {
    auto gossip = message.gossip();
    if (!gossip.has_msg_hash()) {
        TOP_WARN("filter failed, gossip msg should set msg_hash");
        return true;
    }
    if (FindData(gossip.msg_hash())) {
        TOP_DEBUG("RumorFilter FindData, filter msg");
        return true;
    }
    if (!AddData(gossip.msg_hash())) {
        TOP_WARN("RumorFilter::FiltMessage Success,%d Already Exist. filter msg",
                gossip.msg_hash());
        return true;
    }
    return false;
}

}
}
