// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>

#include "xtransport/proto/transport.pb.h"
#include "xkad/proto/kadmlia.pb.h"
#include "xpbase/base/manager_template.h"
#include "xkad/routing_table/node_info.h"
#include "xkad/routing_table/routing_utils.h"

namespace top {
namespace gossip {

class RumorMessageManager : public ManagerTemplate<int32_t, transport::protobuf::RoutingMessage> {
public:
    RumorMessageManager() {}
    ~RumorMessageManager() {}
    bool AddMessage(
        const transport::protobuf::RoutingMessage&);
    void RemoveMessage(
        int32_t);
    bool SearchMessage(
        int32_t, 
        transport::protobuf::RoutingMessage&) const;
    void GetAllMessages(
        std::vector<transport::protobuf::RoutingMessage>&);
    uint32_t GetMessageCount() const;
};

typedef std::shared_ptr<RumorMessageManager> RumorMessageManagerSptr;

}
}
