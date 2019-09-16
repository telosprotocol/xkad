// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/gossip/rumor_message_manager.h"

#include "xpbase/base/top_log.h"

namespace top {
namespace gossip {

bool RumorMessageManager::AddMessage(
    const transport::protobuf::RoutingMessage& message) {
    if (!AddData(message.type(),message)) {
        TOP_WARN("RumorMessageManager::AddMessage Failed,message tpye[%d]",message.type());
        return false;
    }
    return true;
}

void RumorMessageManager::RemoveMessage(int32_t message_type) {
    DeleteKey(message_type);
}

bool RumorMessageManager::SearchMessage(
        int32_t message_type,
        transport::protobuf::RoutingMessage& message) const {
    if (!FindData(message_type, message)) {
        TOP_WARN("RumorMessageManager::SearchMessage Failed,message type[%d]",
            message_type);
        return false;
    }
    return true;
}

void RumorMessageManager::GetAllMessages(
    std::vector<transport::protobuf::RoutingMessage>& all_messages) {
    GetAllData(all_messages);
}

uint32_t RumorMessageManager::GetMessageCount() const {
    return Count();
}

}
}
