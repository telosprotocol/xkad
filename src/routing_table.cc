// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/routing_table.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <future>
#include <chrono>
#include <bitset>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <fstream>
#include <limits>
#include <sstream>

#include "xbase/xpacket.h"
#include "xbase/xutl.h"

#include "xpbase/base/top_log.h"
#include "xpbase/base/multirelay_log.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/rand_util.h"
#include "xpbase/base/uint64_bloomfilter.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/check_cast.h"
#include "xpbase/base/endpoint_util.h"
#include "xpbase/base/kad_key/get_kadmlia_key.h"
#include "xgossip/include/gossip_utils.h"
#include "xkad/routing_table/node_detection_manager.h"
#include "xkad/routing_table/client_node_manager.h"
#include "xkad/routing_table/callback_manager.h"
#include "xkad/routing_table/nodeid_utils.h"
#include "xkad/routing_table/local_node_info.h"
#include "xpbase/base/top_string_util.h"
//#include "xkad/top_main/top_commands.h"
#include "xpbase/base/kad_key/chain_kadmlia_key.h"
#include "xpbase/base/top_log_name.h"

namespace top {

namespace kadmlia {

static const int32_t kHeartbeatPeriod = 1 * 1000 * 1000;  // 2s
static const int32_t kHeartbeatCheckProcPeriod = 1 * 1000 * 1000;  // 2s
static const int32_t kRejoinPeriod = 3 * 1000 * 1000;  // 3s
static const int32_t kFindNeighboursPeriod = 3 * 1000 * 1000;  // 3s
static const int32_t kDumpRoutingTablePeriod = 1 * 60 * 1000 * 1000; // 5min

RoutingTable::RoutingTable(
        std::shared_ptr<transport::Transport> transport_ptr,
        uint32_t kadmlia_key_len,
        std::shared_ptr<LocalNodeInfo> local_node_ptr)
        : RoutingMaxNodesSize_(kRoutingMaxNodesSize), 
          transport_ptr_(transport_ptr),
          local_node_ptr_(local_node_ptr),
          nodes_(),
          nodes_mutex_(),
          node_id_map_(),
          node_id_map_mutex_(),
          node_hash_map_(std::make_shared<std::map<uint64_t, NodeInfoPtr>>()),
          node_hash_map_mutex_(),
          bootstrap_mutex_(),
          bootstrap_cond_(),
          joined_(false),
          bootstrap_id_(),
          bootstrap_ip_(),
          bootstrap_port_(0),
          bootstrap_nodes_mutex_(),
          find_neighbour_num_(1),
          node_detection_ptr_(nullptr),
          destroy_(false),
          find_nodes_period_(0),
          set_endpoints_mutex_(),
          after_join_(false),
          kadmlia_key_len_(kadmlia_key_len),
        //   support_rumor_(false),
        //   rumor_handler_(nullptr),
          dy_manager_(nullptr),
          heart_beat_info_map_(),
          heart_beat_info_map_mutex_(),
          heart_beat_callback_(nullptr),
          heart_beat_callback_mutex_(),
          security_join_ptr_() {
    // TOP_FATAL_NAME("new RoutingTable(%p)", this);
}

RoutingTable::~RoutingTable() {
    // TOP_FATAL_NAME("delete RoutingTable(%p)", this);
    // TOP_FATAL_NAME("~~~~~~~~~~~~~~~~~RoutingTable()");
}

bool RoutingTable::Init() {
    bootstrap_cache_helper_ = std::make_shared<BootstrapCacheHelper>(timer_manager_);

    if (!transport_ptr_) {
        TOP_ERROR_NAME("udp transport invalid!");
        return false;
    }

    if (!local_node_ptr_) {
        TOP_ERROR_NAME("local_node_ptr invalid!");
        return false;
    }

    name_ = base::StringUtil::str_fmt("<bluert %s>", HexSubstr(local_node_ptr_->id()).c_str());

    uint16_t local_port = transport_ptr_->local_port();
    local_node_ptr_->set_local_port(local_port);

    if (local_node_ptr_->first_node()) {
        local_node_ptr_->set_public_ip(local_node_ptr_->local_ip());
        local_node_ptr_->set_public_port(local_node_ptr_->local_port());
    }

    {
        std::unique_lock<std::mutex> lock_hash(node_hash_map_mutex_);
        NodeInfoPtr node_ptr;
        node_ptr.reset(new NodeInfo(local_node_ptr_->id()));
        node_ptr->local_ip = local_node_ptr_->local_ip();
        node_ptr->local_port = local_node_ptr_->local_port();
        node_ptr->public_ip = local_node_ptr_->public_ip();
        node_ptr->public_port = local_node_ptr_->public_port();
        node_ptr->xid = local_node_ptr_->xid();
        node_ptr->hash64 = local_node_ptr_->hash64();
        node_hash_map_->insert(std::make_pair(node_ptr->hash64, node_ptr));
    }

    node_detection_ptr_.reset(new NodeDetectionManager(timer_manager_, *this));
    dy_manager_.reset(new DynamicXipManager);
//     SupportSecurityJoin();

    // attention: hearbeat timer does not do hearbeating really(using xudp do)
    timer_heartbeat_ = std::make_shared<base::TimerRepeated>(timer_manager_, "RoutingTable::HeartbeatProc");
    timer_heartbeat_->Start(
            kHeartbeatPeriod,
            kHeartbeatPeriod,
            std::bind(&RoutingTable::HeartbeatProc, shared_from_this()));

    // timer_heartbeat_check_ = std::make_shared<base::TimerRepeated>(timer_manager_, "RoutingTable::HeartbeatCheckProc");
    // timer_heartbeat_check_->Start(
    //         kHeartbeatCheckProcPeriod,
    //         kHeartbeatCheckProcPeriod,
    //         std::bind(&RoutingTable::HeartbeatCheckProc, shared_from_this()));
    using namespace std::placeholders;
    HeartbeatManagerIntf::Instance()->Register(std::to_string((long)this), std::bind(&RoutingTable::OnHeartbeatFailed, shared_from_this(), _1, _2));
    if (!local_node_ptr_->first_node()) {
        TOP_INFO_NAME("RoutingTable Init start Rejoin Timer");
        timer_rejoin_ = std::make_shared<base::TimerRepeated>(timer_manager_, "RoutingTable::Rejoin");
        timer_rejoin_->Start(
                kRejoinPeriod,
                kRejoinPeriod,
                std::bind(&RoutingTable::Rejoin, shared_from_this()));
    }
    timer_find_neighbours_ = std::make_shared<base::TimerRepeated>(timer_manager_, "RoutingTable::FindNeighbours");
    timer_find_neighbours_->Start(
            kFindNeighboursPeriod,
            kFindNeighboursPeriod,
            std::bind(&RoutingTable::FindNeighbours, shared_from_this()));

    /*
    timer_prt_ = std::make_shared<base::TimerRepeated>(timer_manager_, "RoutingTable::PrintRoutingTable");
    timer_prt_->Start(
            kDumpRoutingTablePeriod,
            kDumpRoutingTablePeriod,
            std::bind(&RoutingTable::PrintRoutingTable, shared_from_this()));
            */
    return true;
}

void RoutingTable::PrintRoutingTable() {
    if (destroy_) {
        return;
    }
    static std::atomic<uint32_t> index(0);

    uint32_t size = 0;
    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        uint32_t tmp_index = ++index;
        std::string file_name = "/tmp/all_node_id_" + HexSubstr(local_node_ptr_->id()) + std::to_string(tmp_index);
        FILE* fd = fopen(file_name.c_str(), "w");
        if (fd == NULL) {
            return;
        }

        SortNodesByTargetXid(local_node_ptr_->id(), nodes_.size());
        size = nodes_.size();
        fprintf(fd, "local: %s\t%s:%d\n", HexEncode(local_node_ptr_->id()).c_str(),local_node_ptr_->public_ip().c_str(), local_node_ptr_->public_port());
        uint32_t size = 0;
        std::map<uint32_t, unsigned int> bucket_rank_map;
        for (auto& node_ptr : nodes_) {
            bucket_rank_map[node_ptr->bucket_index] += 1;
            fprintf(fd, "node: %s\t%s:%d\n", HexEncode(node_ptr->node_id).c_str(),
                    node_ptr->public_ip.c_str(),
                    node_ptr->public_port);
        }

        fprintf(fd, "\n");
        for (auto& item : bucket_rank_map) {
            fprintf(fd, "bucket: %d:%d\n", item.first, item.second);
        }
        fclose(fd);
    }

    TOP_DEBUG_NAME("RoutingTable dump for xnetwork_id(%d) service_type(%llu), dump_size(%d)",
            local_node_ptr_->kadmlia_key()->xnetwork_id(),
            local_node_ptr_->kadmlia_key()->GetServiceType(),
            size);
}

bool RoutingTable::UnInit() {
    TellNeighborsDropAllNode();
    destroy_ = true;
    // if (rumor_handler_) {
    //     if (!rumor_handler_->UnInit()) {
    //         TOP_ERROR_NAME("RoutingTable::UnInit Failed.RumorHandler::UnInit");
    //     }
    // } else {
    //     TOP_ERROR_NAME("RoutingTable::UnInit Failed,RumorHandlerSptr Is Invalid.");
    // }

    if (node_detection_ptr_) {
        node_detection_ptr_->Join();
    }

    timer_rejoin_ = nullptr;
    timer_find_neighbours_ = nullptr;
    timer_heartbeat_ = nullptr;
    timer_heartbeat_check_ = nullptr;
    timer_prt_ = nullptr;

    if (bootstrap_cache_helper_) {
        bootstrap_cache_helper_->Stop();
    }

    return true;
}

// bool RoutingTable::SupportRumor(bool just_root) {
//     if (support_rumor_) {
//         TOP_ERROR_NAME("RoutingTable::SupportRumor Already Supported.");
//         return true;
//     }
//     rumor_handler_.reset(new gossip::RumorHandler(just_root));
//     if(!rumor_handler_->Init(shared_from_this())) {
//         TOP_ERROR_NAME("RumorHandler::Init Failed.");
//         return false;
//     }
//     support_rumor_ = true;
//     return true;
// }

// void RoutingTable::SpreadAllNeighborsRapid(transport::protobuf::RoutingMessage& message) {
//     if (!CheckRumorLicense()) {
//         TOP_ERROR_NAME("RoutingTable::SpreadAllNeighbors Failed,CheckRumorLicense Failed.");
//         return;
//     }
//     rumor_handler_->SpreadNeighborsRapid(message);
// }

// bool RoutingTable::CheckRumorLicense() const {
//     if (!support_rumor_) {
//         TOP_ERROR_NAME("RoutingTable::CheckRumorLicense Faild,Rumor Is Not Supported.");
//         return false;
//     }
//     if (!rumor_handler_) {
//         TOP_ERROR_NAME("RoutingTable::CheckRumorLicense Faild,RumorHandlerSptr Is Invalid.");
//         return false;
//     }
//     return true;
// }

int RoutingTable::MultiJoin(const std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    TOP_INFO_NAME("MultiJoin(%d) ...", (int)boot_endpoints.size());  // NOLINT
    // std::string boot_nodes;
    // for (const auto& kv : boot_endpoints) boot_nodes += kv.first + ":" + std::to_string(kv.second) + ", ";
    // TOP_INFO_NAME("MultiJoin(%s) ...", boot_nodes.c_str());  // NOLINT

    if (joined_) {
        return kKadFailed;
    }

    int retried_times = 0;
    uint32_t wait_time = 4;
    //while (retried_times < kJoinRetryTimes) {
    while (1) {
        for (auto& kv : boot_endpoints) {
            const auto peer_ip = kv.first;
            const auto peer_port = kv.second;
            Bootstrap(peer_ip, peer_port, local_node_ptr_->service_type());
            TOP_INFO_NAME("  -> Bootstrap(%s:%d) ...", peer_ip.c_str(), peer_port);
        }

        std::unique_lock<std::mutex> lock(bootstrap_mutex_);
        if (bootstrap_cond_.wait_for(lock, std::chrono::seconds(wait_time), [this] () -> bool {
                    return this->joined_; })) {
            if (joined_) {
                TOP_INFO_NAME("  node join(%s:%d) success",
                    bootstrap_ip_.c_str(), (int)bootstrap_port_);  // NOLINT

                return kKadSuccess;
            }
        }

        retried_times += 1;
        if (retried_times > kJoinRetryTimes) {
            wait_time *= 2;
            if (wait_time > 128) {
                wait_time = 128;
            }
        }

        TOP_INFO_NAME("%s [%llu] has nodes_ size: %d, set_size: %d, ip: %s, port: %d",
            HexSubstr(local_node_ptr_->id()).c_str(),
            local_node_ptr_->service_type(),
            node_id_map_.size(),
            nodes_size(), local_node_ptr_->public_ip().c_str(),
            local_node_ptr_->public_port());
    }

    TOP_ERROR_NAME("node join failed after retried: %d times!", retried_times);
    return kKadFailed;
}

void RoutingTable::MultiJoinAsync(const std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    TOP_INFO_NAME("MultiJoinAsync(%d) ...", (int)boot_endpoints.size());  // NOLINT
    if (joined_) {
        TOP_INFO_NAME("joined before");
        return;
    }

    int retried_times = 0;
    while (retried_times < kJoinRetryTimes) {
        for (auto& kv : boot_endpoints) {
            const auto peer_ip = kv.first;
            const auto peer_port = kv.second;
            Bootstrap(peer_ip, peer_port, local_node_ptr_->service_type());
            TOP_INFO_NAME("  -> Bootstrap(%s:%d) ...", peer_ip.c_str(), peer_port);
        }

        ++retried_times;
    }
}

bool RoutingTable::IsJoined() {
    return joined_;
}

bool RoutingTable::SetJoin(const std::string& boot_id, const std::string& boot_ip,
        int boot_port) {
    std::unique_lock<std::mutex> lock(joined_mutex_);
    if (joined_) {
        TOP_INFO_NAME("SetJoin(%s:%d-%s) ignore",
            boot_ip.c_str(), boot_port, HexEncode(boot_id).c_str());
        return false;
    }

    joined_ = true;
    after_join_ = true;
    set_bootstrap_id(boot_id);
    set_bootstrap_ip(boot_ip);
    set_bootstrap_port(boot_port);
    TOP_INFO_NAME("SetJoin(%s:%d-%s) success",
        boot_ip.c_str(), boot_port, HexEncode(boot_id).c_str());
    return true;
}

void RoutingTable::SetUnJoin() {
    std::unique_lock<std::mutex> lock(joined_mutex_);
    joined_ = false;
}

void RoutingTable::WakeBootstrap() {
    std::lock_guard<std::mutex> lock(bootstrap_mutex_);
    bootstrap_cond_.notify_all();
}

void RoutingTable::SendToClosestNode(transport::protobuf::RoutingMessage& message, bool add_hop) {
    if (message.des_node_id() == local_node_ptr_->id()) {
        TOP_INFO_NAME("send to self wrong!");
        return;
    }

    if (!local_node_ptr_->first_node() && !joined_) {
        return;
    }

    if (add_hop) {
        transport::protobuf::HopInfo* hop_info = message.add_hop_nodes();
        hop_info->set_node_id(local_node_ptr_->id());
    }
    RecursiveSend(message, 0);
}

void RoutingTable::SendToClosestNode(transport::protobuf::RoutingMessage& message) {
    SendToClosestNode(message, true);
}

void RoutingTable::RecursiveSend(transport::protobuf::RoutingMessage& message, int retry_times) {
    std::set<std::string> exclude;
    for (int i = 0; i < message.hop_nodes_size(); ++i) {
        auto iter = exclude.find(message.hop_nodes(i).node_id());
        if (iter != exclude.end()) {
            return;
        }
        exclude.insert(message.hop_nodes(i).node_id());
    }

    std::vector<NodeInfoPtr> ready_nodes;
    std::vector<NodeInfoPtr> next_nodes_vec = GetClosestNodes(message.des_node_id(), RoutingMaxNodesSize_, false);
    for (auto& nptr : next_nodes_vec) {
        if (nptr->node_id == local_node_ptr_->id()) {
            continue;
        }
        if (nptr->public_ip == local_node_ptr_->public_ip() && nptr->public_port == local_node_ptr_->public_port()) {
            continue;
        }
        auto iter = exclude.find(nptr->node_id);
        if (iter != exclude.end()) {
            continue;
        }
        ready_nodes.push_back(nptr);
    }
    if (ready_nodes.empty()) {
        TOP_WARN_NAME("SendToClosestNode get empty nodes, send failed");
        return;
    }

    NodeInfoPtr node = ready_nodes[0];
    if (!node) {
        return;
    }
    SendData(message, node);
}

// more pure send api
int RoutingTable::SendData(
        const xbyte_buffer_t& data,
        const std::string& peer_ip,
        uint16_t peer_port,
        uint16_t priority) {
    uint8_t local_buf[kUdpPacketBufferSize];
    base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0,0, false);
    _xip2_header header;
    memset(&header, 0, sizeof(header));
    header.flags |= priority;
    packet.get_body().push_back((uint8_t*)&header, enum_xip2_header_len);
    packet.get_body().push_back((uint8_t*)data.data(), data.size());  // NOLINT
    packet.set_to_ip_addr(peer_ip);
    packet.set_to_ip_port(peer_port);
    return transport_ptr_->SendData(packet);
}

int RoutingTable::SendData(
        transport::protobuf::RoutingMessage& message,
        const std::string& peer_ip,
        uint16_t peer_port) {
    // TOP_FATAL_NAME("to %s:%d, \n ----%s", peer_ip.c_str(), (int)peer_port, message.DebugString().c_str());
    SetTestTraceInfo(message);
    SetVersion(message);

    std::string msg;
    if (!message.SerializeToString(&msg)) {
        TOP_INFO_NAME("RoutingMessage SerializeToString failed!");
        return kKadFailed;
    }
    xbyte_buffer_t data{msg.begin(), msg.end()};
    return SendData(data, peer_ip, peer_port, message.priority());
}

int RoutingTable::SendData(transport::protobuf::RoutingMessage& message, NodeInfoPtr node) {
    if (node->same_vlan) {
        return SendData(message, node->local_ip, node->local_port);
    }

//    return SendData(message, node->public_ip, node->public_port);
	SetTestTraceInfo(message);
	SetVersion(message);

	std::string msg;
	if (!message.SerializeToString(&msg)) {
		TOP_INFO_NAME("RoutingMessage SerializeToString failed!");
		return kKadFailed;
	}
	xbyte_buffer_t data{msg.begin(), msg.end()};
//	return SendData(data, peer_ip, peer_port, message.priority());

	uint8_t local_buf[kUdpPacketBufferSize];
	base::xpacket_t packet(base::xcontext_t::instance(), local_buf, sizeof(local_buf), 0,0, false);
	_xip2_header header;
	memset(&header, 0, sizeof(header));
	header.flags |= message.priority();
	packet.get_body().push_back((uint8_t*)&header, enum_xip2_header_len);
	packet.get_body().push_back((uint8_t*)data.data(), data.size());  // NOLINT
	packet.set_to_ip_addr(node->public_ip);
	packet.set_to_ip_port(node->public_port);
    TOP_DEBUG_NAME("xkad send message.type:%d size:%d", message.type(), packet.get_size());
	return transport_ptr_->SendDataWithProp(packet, node->udp_property);
}

void RoutingTable::SetTestTraceInfo(transport::protobuf::RoutingMessage& message) {
    int request_type = kadmlia::kNone;  // GetRequestType(message.type());
    if (request_type == kadmlia::kNone) {
        return;
    }
    if (!message.has_multi_relay() || !message.multi_relay()) {
        return;
    }

    std::string prefix;
    request_type == kadmlia::kRequestMsg? prefix = "QQQ": prefix = "AAA";
    if (!message.has_xrequest_id()) {
        std::string random_xrequest_id = RandomAscString(16);
        message.set_xrequest_id(prefix + random_xrequest_id);
    }

    if (!message.has_seq()) {
        message.set_seq(0);
        SMDEBUG(message.xrequest_id().c_str(), message.seq(),
                "%s relay message begin type(%d) msgid(%d) from this node(%s)",
                prefix.c_str(),
                message.type(),
                message.id(),
                HexEncode(local_node_ptr_->id()).c_str());
    }

    // next seq = seq + 1
    int old_seq = message.seq();
    message.set_seq(++old_seq);
    message.add_trace_route(HexEncode(local_node_ptr_->id()));
    return;
}

void RoutingTable::ResetNodeHeartbeat(const std::string& id) {
    std::unique_lock<std::mutex> set_lock(node_id_map_mutex_);
    auto iter = node_id_map_.find(id);
    if (iter != node_id_map_.end()) {
        iter->second->ResetHeartbeat();
    }
}

void RoutingTable::HeartbeatProc() {
    if (destroy_) {
        return;
    }

    // find all nodes need to heartbeat(sort is not nessesary)
    std::vector<NodeInfoPtr> tmp_vec;
    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        int sort_num = SortNodesByTargetXid(local_node_ptr_->id(), RoutingMaxNodesSize_);
        for (int i = 0; i < sort_num; ++i) {
            tmp_vec.push_back(nodes_[i]);
        }
    }
    // do heartbeat for every neighbour nodes
    std::string all_ips;
    const auto tp_now = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < tmp_vec.size(); ++i) {
        all_ips += tmp_vec[i]->public_ip + ", ";
        if (tmp_vec[i]->public_ip == local_node_ptr_->public_ip() &&
                tmp_vec[i]->public_port == local_node_ptr_->public_port()) {
            continue;
        }

        /* // hearbeat timer does not do hearbeating really(using xudp do)
        if (tmp_vec[i]->IsTimeToHeartbeat(tp_now)) {
            int send_ret = SendHeartbeat(tmp_vec[i], local_node_ptr_->service_type());
            if (send_ret != kadmlia::kKadSuccess) {
                // send failed, local socket maybe not connected
                continue;
            }
            {
                std::unique_lock<std::mutex> lock(node_id_map_mutex_);
                tmp_vec[i]->Heartbeat();
            }
        }
        */
    }
    TOP_INFO_NAME("[%s][first: %d][%llu][%d][%d][%d][%d][%d][%d][%d] has nodes_ size(nodes size): %d,"
        "set_size: %d, ip: %s, port: %d, heart_size: %d, all ips:[%s]",
        HexEncode(local_node_ptr_->id()).c_str(),
        local_node_ptr_->first_node(),
        local_node_ptr_->kadmlia_key()->GetServiceType(),
        local_node_ptr_->kadmlia_key()->xnetwork_id(),
        local_node_ptr_->kadmlia_key()->zone_id(),
        local_node_ptr_->kadmlia_key()->cluster_id(),
        local_node_ptr_->kadmlia_key()->group_id(),
        local_node_ptr_->kadmlia_key()->node_id(),
        local_node_ptr_->kadmlia_key()->network_type(),
        local_node_ptr_->kadmlia_key()->xip_type(),
        node_id_map_.size(),
        nodes_size(), local_node_ptr_->public_ip().c_str(),
        local_node_ptr_->public_port(), tmp_vec.size(), all_ips.c_str());
}

void RoutingTable::HeartbeatCheckProc() {
    if (destroy_) {
        return;
    }

    std::vector<NodeInfoPtr> tmp_vec;
    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        int sort_num = SortNodesByTargetXid(local_node_ptr_->id(), RoutingMaxNodesSize_);
        for (int i = 0; i < sort_num; ++i) {
            tmp_vec.push_back(nodes_[i]);
        }
    }

    const auto tp_now = std::chrono::steady_clock::now();
    for (uint32_t i = 0; i < tmp_vec.size(); ++i) {
        if (tmp_vec[i]->IsTimeout(tp_now)) {
            DropNode(tmp_vec[i]);
            TOP_WARN_NAME("node heartbeat error after tried: %d times.ID:[%s],"
                "IP:[%s],Port[%d] to ID:[%s],IP[%s],Port[%d] drop it.",
                tmp_vec[i]->heartbeat_count,
                HexSubstr(local_node_ptr_->id()).c_str(),
                local_node_ptr_->local_ip().c_str(),
                local_node_ptr_->local_port(),
                HexSubstr(tmp_vec[i]->node_id).c_str(),
                tmp_vec[i]->local_ip.c_str(),
                tmp_vec[i]->local_port);
        }
    }
}

void RoutingTable::Rejoin() {
    if (destroy_) {
        return;
    }

    // the first start-time  make sure go-to this if after calling Join
    if (!local_node_ptr_ || !transport_ptr_) {
        TOP_ERROR_NAME("local_node_ptr_ or transport_ptr null");
        return;
    }
    if (local_node_ptr_->first_node()) {
        TOP_ERROR_NAME("this is the first node,doesn't need rejoin");
        return;
    }

    // make sure run rejoin after joined once
    if (!after_join_) {
        return;
    }

    do  {
            {
                std::unique_lock<std::mutex> vec_lock(nodes_mutex_);
                // hearbeat thread may be drop node, finnally nodes_ size become 0
                if (nodes_.empty()) {
                    // this is really import,than join will work
                    SetUnJoin();
                } else {
                    // usually one real node will not create virtual-nodes beyond 5
                    if (nodes_.size() <= 5) {
                        bool offline = true;
                        for (auto& item : nodes_) {
                            if (item->public_ip != local_node_ptr_->public_ip()
                                    || item->public_port != local_node_ptr_->public_port()
                                    || item->local_ip != local_node_ptr_->local_ip()
                                    || item->local_port != local_node_ptr_->local_port()) {
                                offline = false;
                            }
                        }
                        // all nodes in routing-table is virtual-nodes of local real node
                        if (offline) {
                            SetUnJoin();
                        }
                    } // end if (nodes_...
                } // end else
            }

            TOP_INFO_NAME("Rejoin alive for self_service_type(%llu), now size(%d)",
                    local_node_ptr_->kadmlia_key()->GetServiceType(),
                    nodes_size());

            // do Join when haven't any neighbours(not Joined)
            if (!joined_) {
                TOP_INFO_NAME("there is no node ,will rejoin.");
                std::set<std::pair<std::string, uint16_t>> cache_bootstrap_set;
                GetBootstrapCache(cache_bootstrap_set);
                {
                    std::unique_lock<std::mutex> bootstrap_lock(bootstrap_nodes_mutex_);
                    for (auto& node_ptr : bootstrap_nodes_) {
                        cache_bootstrap_set.insert(std::make_pair(node_ptr->public_ip, node_ptr->public_port));
                    }
                }
                if (MultiJoin(cache_bootstrap_set) != kKadSuccess) {
                    TOP_ERROR_NAME("Rejoin MultiJoin failed");
                } else {
                    TOP_INFO_NAME("Rejoin MultiJoin success");
                }
            } // end for if(!joined_...
    } while (0);
}

uint32_t RoutingTable::GetFindNodesMaxSize() {
    return RoutingMaxNodesSize_;
}

void RoutingTable::FindNeighbours() {
    if (destroy_) {
        return;
    }

    // the first start-time  make sure go-to this if after calling Join
    if (local_node_ptr_ && transport_ptr_) {
        std::vector<NodeInfoPtr> tmp_vec;
        {
            std::unique_lock<std::mutex> lock(nodes_mutex_);
            int sort_num = SortNodesByTargetXid(local_node_ptr_->id(), RoutingMaxNodesSize_);
            for (int i = 0; i < sort_num; ++i) {
                tmp_vec.push_back(nodes_[i]);
            }
        }

        TOP_INFO_NAME("FindNeighbours alive for self_service_type(%llu), now size(%d)",
                local_node_ptr_->kadmlia_key()->GetServiceType(),
                nodes_size());

        FindClosestNodes(1, GetFindNodesMaxSize(), tmp_vec);
        // if (find_neighbour_num_ >= 12) {
        //     TOP_DEBUG_NAME("<bluefind> find_neighbour_num_(%d)", find_neighbour_num_);
        //     if (tmp_vec.size() < GetFindNodesMaxSize()) {
        //         FindClosestNodes(1, GetFindNodesMaxSize(), tmp_vec);
        //     } else {
        //         ++find_nodes_period_;
        //         if (find_nodes_period_ > 3) {
        //             FindClosestNodes(1, GetFindNodesMaxSize(), tmp_vec);
        //             find_nodes_period_ = 0;
        //         }
        //     }
        // } else if (find_neighbour_num_ == 1) {
        //     FindClosestNodes(0, GetFindNodesMaxSize(), tmp_vec);
        // } else {
        //     TOP_DEBUG_NAME("<bluefind> find_neighbour_num_(%d)", find_neighbour_num_);
        //     int find_num = static_cast<int>(std::pow(1.8, find_neighbour_num_++));
        //     if (find_num > GetFindNodesMaxSize()) {
        //         find_num = GetFindNodesMaxSize();
        //     }
        //     TOP_DEBUG_NAME("<bluefind> find_num(%d)", find_num);
        //     FindClosestNodes(1, find_num % (GetFindNodesMaxSize() + 1), tmp_vec);
        // }
    }
}

int RoutingTable::AddNode(NodeInfoPtr node) {
    TOP_DEBUG_NAME("node_id(%s), pub(%s:%d)", HexSubstr(node->node_id).c_str(), node->public_ip.c_str(), node->public_port);
    if (node->nat_type == kNatTypeUnknown) {
        TOP_WARN_NAME("bluenat[%llu] add node(%s:%d-%d) failed: nat_type is unknown",
            local_node_ptr_->service_type(),
            node->public_ip.c_str(), (int)node->public_port, node->service_type);
        return kKadFailed;
    }

    if (node->node_id == local_node_ptr_->id()) {
        TOP_DEBUG_NAME("kHandshake: local_node_ptr_->id()[%s][%s][%s][%d][%s][%s]",
                HexEncode(node->node_id).c_str(),
                HexEncode(local_node_ptr_->id()).c_str(),
                node->public_ip.c_str(),
                node->public_port,
                HexEncode(node->xid).c_str(),
                HexEncode(global_xid->Get()).c_str());
        return kKadFailed;
    }

    if (!ValidNode(node)) {
        TOP_WARN_NAME("node invalid.");
        return kKadFailed;
    }

    if (HasNode(node)) {
        TOP_INFO_NAME("kHandshake: HasNode: %s", HexEncode(node->node_id).c_str());
        return kKadNodeHasAdded;
    }

    if (SetNodeBucket(node) != kKadSuccess) {
        TOP_WARN_NAME("set node bucket index failed![%s]", node->node_id.c_str());
        return kKadFailed;
    }

    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        SortNodesByTargetXid(local_node_ptr_->id(), nodes_.size());
        if (!NewNodeReplaceOldNode(node, true)) {
            TOP_WARN_NAME("newnodereplaceoldnode failed. node_id:%s, node_bucket:%d, local:%s",
                    HexEncode(node->node_id).c_str(),
                    node->bucket_index,
                    HexEncode(local_node_ptr_->id()).c_str());
            return kKadFailed;
        }

        TOP_WARN_NAME("newnodereplaceoldnode success. node_id:%s, node_bucket:%d, local:%s",
                HexEncode(node->node_id).c_str(),
                node->bucket_index,
                HexEncode(local_node_ptr_->id()).c_str());

        if (HasNode(node)) {
            TOP_INFO_NAME("kHandshake: HasNode");
            return kKadNodeHasAdded;
        }
        TOP_DEBUG_NAME("addnode:[%s] for local_node:[%s]", HexEncode(node->node_id).c_str(), HexEncode(local_node_ptr_->id()).c_str());
        nodes_.push_back(node);
        // DumpNodes();
    }

    {
        std::unique_lock<std::mutex> lock(node_id_map_mutex_);
        node_id_map_.insert(std::make_pair(node->node_id, node));
    }

    {
        std::unique_lock<std::mutex> lock_hash(node_hash_map_mutex_);
        node_hash_map_->insert(std::make_pair(node->hash64, node));
    }

    no_lock_for_use_nodes_.reset();
    no_lock_for_use_nodes_ = std::make_shared<std::vector<NodeInfoPtr>>(nodes());
    return kKadSuccess;
}

void RoutingTable::DumpNodes() {
    // dump all nodes
    {
        std::string fmt("all nodes:\n");
        TOP_DEBUG_NAME("%s", fmt.c_str());
        fmt = base::StringUtil::str_fmt("self]: %s, dis(0), pub(%s:%d)\n",
                    HexSubstr(local_node_ptr_->id()).c_str(),
                    local_node_ptr_->public_ip().c_str(), (int)local_node_ptr_->public_port());
        TOP_DEBUG_NAME("%s", fmt.c_str());
        for (int i = 0; i < (int)nodes_.size(); ++i) {
            // fmt += base::StringUtil::str_fmt("%d: count(%d)\n", kv.first, kv.second);
            fmt = base::StringUtil::str_fmt("%4d]: %s, dis(%d), pub(%s:%d)\n", (int)i,
                    HexSubstr(nodes_[i]->node_id).c_str(), nodes_[i]->bucket_index,
                    nodes_[i]->public_ip.c_str(), (int)nodes_[i]->public_port);
            TOP_DEBUG_NAME("%s", fmt.c_str());
        }
    }
}

bool RoutingTable::CanAddNode(NodeInfoPtr node) {
    TOP_DEBUG_NAME("node_id(%s), pub(%s:%d)", HexSubstr(node->node_id).c_str(), node->public_ip.c_str(), node->public_port);
    if (node->nat_type == kNatTypeUnknown) {
        TOP_WARN_NAME("bluenat[%llu] CanAddNode: node(%s:%d-%d) nat_type is unknown",
            local_node_ptr_->service_type(),
            node->public_ip.c_str(), (int)node->public_port, node->service_type);
        return false;
    }

    if (node->node_id == local_node_ptr_->id()) {
        TOP_DEBUG_NAME("local node");
        return false;
    }

    if (!ValidNode(node)) {
        TOP_WARN_NAME("node invalid.");
        return false;
    }

    if (HasNode(node)) {
        TOP_DEBUG_NAME("has node");
        return false;
    }

    if (SetNodeBucket(node) != kKadSuccess) {
        TOP_WARN_NAME("set node bucket index failed![%s]", HexSubstr(node->node_id).c_str());
        return false;
    }

    std::unique_lock<std::mutex> lock(nodes_mutex_);
    SortNodesByTargetXid(local_node_ptr_->id(), nodes_.size());
    NodeInfoPtr remove_node;
    if (NewNodeReplaceOldNode(node, false)) {
        return true;
    }

    TOP_DEBUG_NAME("replace fail, dis(%d)", node->bucket_index);
    return false;
}

int RoutingTable::DropNode(NodeInfoPtr node) {
    {
        std::unique_lock<std::mutex> vec_lock(nodes_mutex_);
        for (auto iter = nodes_.begin(); iter != nodes_.end(); ++iter) {
            if ((*iter)->node_id == node->node_id) {
                nodes_.erase(iter);
                break;
            }
        }
    }

    {
        std::unique_lock<std::mutex> set_lock(node_id_map_mutex_);
        auto iter = node_id_map_.find(node->node_id);
        if (iter != node_id_map_.end()) {
            node_id_map_.erase(iter);
        }
    }

    {
        std::unique_lock<std::mutex> lock_hash(node_hash_map_mutex_);
        auto iter = node_hash_map_->find(node->hash64);
        if (iter != node_hash_map_->end()) {
            node_hash_map_->erase(iter);
        }
    }

    // drop dynamic xip distribute by node
    TOP_DEBUG_NAME("update drop node[%s, %s:%d]",
            HexSubstr(node->node_id).c_str(),
            node->public_ip.c_str(), node->public_port);
    local_node_ptr_->DropDxip(node->node_id);
    if(security_join_ptr_) {
        security_join_ptr_->Frozen(node->xid);
    }
    no_lock_for_use_nodes_.reset();
    no_lock_for_use_nodes_ = std::make_shared<std::vector<NodeInfoPtr>>(nodes());
    return kKadSuccess;
}

NodeInfoPtr RoutingTable::GetRandomNode() {
    std::unique_lock<std::mutex> lock(nodes_mutex_);
    if (nodes_.empty()) {
        return nullptr;
    }
    return nodes_[RandomUint32() % nodes_.size()];
}

std::vector<NodeInfoPtr> RoutingTable::nodes() {
    std::unique_lock<std::mutex> lock(nodes_mutex_);
    return nodes_;
}

void RoutingTable::GetRangeNodes(
        const uint64_t& min,
        const uint64_t& max,
        std::vector<NodeInfoPtr>& vec) {
    if (min == 0 && max == std::numeric_limits<uint64_t>::max()) {
        vec = nodes();
        return;
    }

    std::unique_lock<std::mutex> lock(node_hash_map_mutex_);
    auto minit = node_hash_map_->lower_bound(min); // the first item not less than
    auto maxit = node_hash_map_->upper_bound(max); // the first item greater than
    for (auto it = minit; it != maxit && it != node_hash_map_->end(); ++it) {
        vec.push_back(it->second);
    }
    return;
}

// include min_index, include max_index. [,]
void RoutingTable::GetRangeNodes(
        uint32_t min_index,
        uint32_t max_index,
        std::vector<NodeInfoPtr>& vec) {
    if (min_index > max_index) {
        return;
    }
    if (min_index >= node_hash_map_->size() || max_index < 0) {
        return;
    }
    if (max_index >= node_hash_map_->size()) {
        max_index = node_hash_map_->size() - 1;
    }
    if (min_index == 0 && max_index == node_hash_map_->size() - 1) {
        vec = nodes();
        return;
    }

    std::unique_lock<std::mutex> lock(node_hash_map_mutex_);
    auto ibegin = node_hash_map_->begin();
    auto nxit_min = std::next(ibegin, min_index);
    auto nxit_max = std::next(ibegin, max_index + 1);

    for (; nxit_min != nxit_max; ++ nxit_min) {
        vec.push_back(nxit_min->second);
    }
    return;
}

int32_t RoutingTable::GetSelfIndex() {
    std::unique_lock<std::mutex> lock(node_hash_map_mutex_);
    auto ifind = node_hash_map_->find(local_node_ptr_->hash64());
    if (ifind == node_hash_map_->end()) {
        //std::cout << "not found" << std::endl;
        return -1;
    }
    auto  index = std::distance(node_hash_map_->begin(), ifind);
    //std::cout << "index:" << index << std::endl;
    return index;
}

uint32_t RoutingTable::nodes_size() {
    std::unique_lock<std::mutex> lock(nodes_mutex_);
    return nodes_.size();
}

NodeInfoPtr RoutingTable::GetNode(const std::string& id) {
    std::unique_lock<std::mutex> set_lock(node_id_map_mutex_);
    auto iter = node_id_map_.find(id);
    if (iter != node_id_map_.end()) {
        return iter->second;
    }

    return nullptr;
}

int RoutingTable::ClosestToTarget(
        const std::string& target,
        const std::set<std::string>& exclude,
        bool& closest) {
    if (target == local_node_ptr_->id()) {
        return kKadFailed;
    }

    if (nodes_size() == 0) {
        return kKadFailed;
    }

    if (target.size() != kNodeIdSize) {
        TOP_INFO_NAME("Invalid target_id passed. node id size[%d]", target.size());
        return kKadFailed;
    }

    NodeInfoPtr closest_node(GetClosestNode(target, true, exclude));
    if (!closest_node) {
        closest = true;
        return kKadSuccess;
    }

    closest = (closest_node->bucket_index == kSelfBucketIndex) ||
            CloserToTarget(local_node_ptr_->id(), closest_node->node_id, target);
    return kKadSuccess;
}

int RoutingTable::ClosestToTarget(const std::string& target, bool& closest) {
    if (target == local_node_ptr_->id()) {
        TOP_ERROR_NAME("target equal local nodeid, CloserToTarget goes wrong");
        return kKadFailed;
    }

    if (nodes_size() == 0) {
        closest = true;
        return kKadSuccess;
    }

    if (target.size() != kNodeIdSize) {
        TOP_INFO_NAME("Invalid target_id passed. node id size[%d]", target.size());
        return kKadFailed;
    }

    std::set<std::string> exclude;
    NodeInfoPtr closest_node(GetClosestNode(target, true, exclude));
    if (!closest_node) {
        closest = true;
        return kKadSuccess;
    }
    closest = (closest_node->bucket_index == kSelfBucketIndex) ||
              CloserToTarget(local_node_ptr_->id(), closest_node->node_id, target);
    return kKadSuccess;
}

NodeInfoPtr RoutingTable::GetClosestNode(
        const std::string& target_id,
        bool not_self,
        const std::set<std::string>& exclude,
        bool base_xip) {
    auto closest_nodes(GetClosestNodes(target_id, RoutingMaxNodesSize_, base_xip));
    for (const auto& node_info : closest_nodes) {
        if (not_self) {
            if (node_info->node_id == local_node_ptr_->id()) {
                continue;
            }
        }

        auto iter = exclude.find(node_info->node_id);
        if (iter != exclude.end()) {
            continue;
        }

        return node_info;
    }
    return nullptr;
}

std::vector<NodeInfoPtr> RoutingTable::GetClosestNodes(
    const std::string& target_id,
    uint32_t number_to_get,
    bool base_xip) {
    std::unique_lock<std::mutex> lock(nodes_mutex_);
    if (number_to_get == 0) {
        return std::vector<NodeInfoPtr>();
    }

    int sorted_count = 0;
    if (base_xip) {
        sorted_count = SortNodesByTargetXip(target_id, number_to_get);
    } else {
        sorted_count = SortNodesByTargetXid(target_id, number_to_get);
    }

    if (sorted_count == 0) {
        return std::vector<NodeInfoPtr>();
    }

    return std::vector<NodeInfoPtr>(
               std::begin(nodes_),
               std::begin(nodes_) + static_cast<size_t>(sorted_count));
}

bool RoutingTable::HasNode(NodeInfoPtr node) {
    std::unique_lock<std::mutex> lock(node_id_map_mutex_);
    auto iter = node_id_map_.find(node->node_id);
    return iter != node_id_map_.end();
}

NodeInfoPtr RoutingTable::FindLocalNode(const std::string node_id) {
    std::unique_lock<std::mutex> lock(node_id_map_mutex_);
    auto iter = node_id_map_.find(node_id);
    if (iter != node_id_map_.end()) {
        return iter->second;
    }
    return nullptr;
}

bool RoutingTable::ValidNode(NodeInfoPtr node) {
    if (node->node_id.size() != kNodeIdSize) {
        TOP_ERROR_NAME("node id size is invalid![%d] should[%d]", node->node_id.size(), kNodeIdSize);
        return false;
    }

    if (node->public_ip.empty() || node->public_port <= 0) {
        TOP_WARN_NAME("node[%s] public ip or public port invalid!", HexEncode(node->node_id).c_str());
        return false;
    }
    return true;
}

int RoutingTable::SetNodeBucket(NodeInfoPtr node) {
    int id_bit_index(0);
    while (id_bit_index != kNodeIdSize) {
        if (local_node_ptr_->id()[id_bit_index] != node->node_id[id_bit_index]) {
            std::bitset<8> holder_byte(static_cast<int>(
                        local_node_ptr_->id()[id_bit_index]));
            std::bitset<8> node_byte(static_cast<int>(node->node_id[id_bit_index]));
            int bit_index(0);
            while (bit_index != 8U) {
                if (holder_byte[7U - bit_index] != node_byte[7U - bit_index]) {
                    break;
                }
                ++bit_index;
            }

            node->bucket_index = (8 * (kNodeIdSize - id_bit_index)) - bit_index;
            return kKadSuccess;
        }
        ++id_bit_index;
    }
    node->bucket_index = kSelfBucketIndex;
    return kKadFailed;
}

void RoutingTable::SortNodesByTargetXid(
        const std::string& target_xid,
        std::vector<NodeInfoPtr>& nodes) {
    std::sort(
            nodes.begin(),
            nodes.end(),
            [target_xid, this](const NodeInfoPtr & lhs, const NodeInfoPtr & rhs) {
        return CloserToTarget(lhs->node_id, rhs->node_id, target_xid);
    });
}

int RoutingTable::SortNodesByTargetXid(const std::string& target_xid, int number) {
    int count = std::min(number, static_cast<int>(nodes_.size()));
    if (count <= 0) {
        return 0;
    }

    std::partial_sort(
        nodes_.begin(),
        nodes_.begin() + count,
        nodes_.end(),
    [target_xid, this](const NodeInfoPtr & lhs, const NodeInfoPtr & rhs) {
        return CloserToTarget(lhs->node_id, rhs->node_id, target_xid);
    });
    return count;
}

int RoutingTable::SortNodesByTargetXip(const std::string& target_xip, int number) {
    int count = std::min(number, static_cast<int>(nodes_.size()));
    if (count <= 0) {
        return 0;
    }

    // TODO(smaug) xip of node
    std::partial_sort(
        nodes_.begin(),
        nodes_.begin() + count,
        nodes_.end(),
    [target_xip, this](const NodeInfoPtr & lhs, const NodeInfoPtr & rhs) {
        return CloserToTarget(lhs->xip, rhs->xip, target_xip);
    });
    return count;
}
bool RoutingTable::CloserToTarget(
    const std::string& id1,
    const std::string& id2,
    const std::string& target_id) {
    for (int i = 0; i < kNodeIdSize; ++i) {
        unsigned char result1 = id1[i] ^ target_id[i];
        unsigned char result2 = id2[i] ^ target_id[i];
        if (result1 != result2) {
            return result1 < result2;
        }
    }
    return false;
}

bool RoutingTable::NewNodeReplaceOldNode(NodeInfoPtr node, bool remove) {
    int sum = 0;
    for (auto& n : nodes_) {
        if (n->bucket_index == node->bucket_index) {
            sum += 1;
        }
    }

    // the k-bucket is full
    if (sum >= kKadParamK) {
        TOP_DEBUG_NAME("k-bucket(%d) is full", node->bucket_index);
        return false;
    }

    return true;
}

void RoutingTable::GetRandomAlphaNodes(std::map<std::string, std::string>& query_nodes) {
    query_nodes.clear();
    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        if (nodes_.size() == 0) {
            return;
        }
        const auto count = std::min((int)nodes_.size(), kKadParamAlphaRandom);
        while ((int)query_nodes.size() < count) {
            uint32_t rand_index = RandomUint32() % nodes_.size();
            auto node = nodes_[rand_index];
            if (query_nodes.find(node->node_id) != query_nodes.end()) {
                continue;  // random again
            }
            query_nodes[node->node_id] = "";
        }
    }
}

void RoutingTable::GetClosestAlphaNodes(std::map<std::string, std::string>& query_nodes) {
    query_nodes.clear();
    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        // when nodes_.size is not enough
        if (nodes_.size() <= kKadParamAlpha + kKadParamAlphaRandom) {
            for (auto& node : nodes_) {
                query_nodes[node->node_id] = "";
            }
            return;
        }

        // add alpha closest nodes
        const auto count = SortNodesByTargetXid(local_node_ptr_->id(), kKadParamAlpha);
        for (int i = 0; i < count; ++i) {
            query_nodes[nodes_[i]->node_id] = "";
        }

        // add alpha random nodes
        while ((int)query_nodes.size() < kKadParamAlpha + kKadParamAlphaRandom) {
            uint32_t rand_index = RandomUint32() % (nodes_.size() - kKadParamAlpha);
            auto node = nodes_[rand_index + kKadParamAlpha];  // without first alpha closest nodes
            if (query_nodes.find(node->node_id) != query_nodes.end()) {
                continue;  // random again
            }
            query_nodes[node->node_id] = "";
        }
    }
}

void RoutingTable::FindClosestNodes(
        int attempts,
        int count,
        const std::vector<NodeInfoPtr>& nodes) {
    TOP_DEBUG_NAME("<bluefind> FindClosestNodes(count=%d, nodes.size=%d)", count, (int)nodes.size());
    if (!local_node_ptr_->first_node() && !joined_ && nodes_size() <= 0) {
        TOP_INFO_NAME("this node has not joind!");
        return;
    }

    if (attempts == 0) {
        SendFindClosestNodes(bootstrap_id_, count, nodes, local_node_ptr_->service_type());
        return;
    }

    // NodeInfoPtr closest_node = nullptr;
    std::map<std::string, std::string> query_nodes;
    // GetRandomAlphaNodes(query_nodes);
    GetClosestAlphaNodes(query_nodes);

    for (auto& kv : query_nodes) {
        SendFindClosestNodes(kv.first, count, nodes, local_node_ptr_->service_type());
    }
}

int RoutingTable::Bootstrap(
        const std::string& peer_ip,
        uint16_t peer_port,
        uint64_t des_service_type) {
    TOP_DEBUG_NAME("Bootstrap to (%s:%d_%ld)",
        peer_ip.c_str(), (int)peer_port, (long)des_service_type);
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(des_service_type);
    message.set_priority(enum_xpacket_priority_type_flash);
    TOP_INFO_NAME("join with service type[%llu] ,src[%llu], des[%llu]",
            local_node_ptr_->service_type(),
            local_node_ptr_->service_type(),
            des_service_type);
    message.set_des_node_id("");
    message.set_type(kKadBootstrapJoinRequest);
    if (local_node_ptr_->client_mode()) {
        // usually for bootstrapjoin type,set cleint_msg is enough
        message.set_client_id(local_node_ptr_->id());
        message.set_relay_flag(false);
    }

    protobuf::BootstrapJoinRequest join_req;
    join_req.set_local_ip(local_node_ptr_->local_ip());
    join_req.set_local_port(local_node_ptr_->local_port());
    join_req.set_nat_type(local_node_ptr_->nat_type());
    join_req.set_xid(global_xid->Get());
    if (!local_node_ptr_->client_mode()) {
        join_req.set_xip(local_node_ptr_->xip());
    }
    std::string data;
    if (!join_req.SerializeToString(&data)) {
        TOP_INFO_NAME("ConnectReq SerializeToString failed!");
        return kKadFailed;
    }

    message.set_data(data);
    SetSpeClientMessage(message);
    return SendData(message, peer_ip, peer_port);
}

void RoutingTable::FindCloseNodesWithEndpoint(
        const std::string& des_node_id,
        const std::pair<std::string, uint16_t>& boot_endpoints) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id(des_node_id);
    message.set_type(kKadFindNodesRequest);
    message.set_priority(enum_xpacket_priority_type_flash);

    protobuf::FindClosestNodesRequest find_nodes_req;
    find_nodes_req.set_count(GetFindNodesMaxSize());
    find_nodes_req.set_target_id(local_node_ptr_->id());
    std::string data;
    if (!find_nodes_req.SerializeToString(&data)) {
        TOP_INFO_NAME("ConnectReq SerializeToString failed!");
        return;
    }

    message.set_data(data);
    SetSpeClientMessage(message);
    SendData(message, boot_endpoints.first, boot_endpoints.second);
}

int RoutingTable::SendFindClosestNodes(
        const std::string& node_id,
        int count,
        const std::vector<NodeInfoPtr>& nodes,
        uint64_t des_service_type) {
    TOP_INFO_NAME("sendfind to %s", HexSubstr(node_id).c_str());
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(des_service_type);
    message.set_des_node_id(node_id);
    message.set_type(kKadFindNodesRequest);
    message.set_priority(enum_xpacket_priority_type_flash);

    protobuf::FindClosestNodesRequest find_nodes_req;
    find_nodes_req.set_count(count);
    find_nodes_req.set_target_id(local_node_ptr_->id());
    std::vector<uint64_t> bloomfilter_vec;
    GetExistsNodesBloomfilter(nodes, bloomfilter_vec);
    for (uint32_t i = 0; i < bloomfilter_vec.size(); ++i) {
        find_nodes_req.add_bloomfilter(bloomfilter_vec[i]);
    }

    auto src_nodeinfo_ptr = find_nodes_req.mutable_src_nodeinfo();
    src_nodeinfo_ptr->set_id(local_node_ptr_->id());
    src_nodeinfo_ptr->set_public_ip(local_node_ptr_->public_ip());
    src_nodeinfo_ptr->set_public_port(local_node_ptr_->public_port());
    src_nodeinfo_ptr->set_local_ip(local_node_ptr_->local_ip());
    src_nodeinfo_ptr->set_local_port(local_node_ptr_->local_port());
    src_nodeinfo_ptr->set_nat_type(local_node_ptr_->nat_type());
    src_nodeinfo_ptr->set_xip(local_node_ptr_->xip());
    src_nodeinfo_ptr->set_xid(global_xid->Get());

    std::string data;
    if (!find_nodes_req.SerializeToString(&data)) {
        TOP_INFO_NAME("ConnectReq SerializeToString failed!");
        return kKadFailed;
    }

    message.set_data(data);
    SetSpeClientMessage(message);
    NodeInfoPtr node_ptr = GetNode(node_id);
    if (!node_ptr) {
        return kKadNodeNotExists;
    }
    TOP_DEBUG_NAME("sendfindclosestnodes: message.is_root(%d),"
            "message.des_service_type:(%llu), local_service_type:(%llu)",
            message.is_root(),
            message.des_service_type(),
            local_node_ptr_->kadmlia_key()->GetServiceType());
    TOP_DEBUG_NAME("bluefind send_find to node: %s", HexSubstr(node_ptr->node_id).c_str());
    SendData(message, node_ptr);
    return kKadSuccess;
}

int RoutingTable::SendHeartbeat(NodeInfoPtr node_ptr, uint64_t des_service_type) {
    TOP_WARN_NAME("SendHeartbeat to %s:%d", node_ptr->public_ip.c_str(), node_ptr->public_port);
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(des_service_type);
    message.set_des_node_id(node_ptr->node_id);
    message.set_type(kKadHeartbeatRequest);
    message.set_priority(enum_xpacket_priority_type_flash);
    protobuf::Heartbeat heart_beat_info;
    ::google::protobuf::Map< ::std::string, ::std::string >* extinfo_map
        = heart_beat_info.mutable_extinfo_map();

    {
        std::unique_lock<std::mutex> lock(heart_beat_info_map_mutex_);
        for (auto& item : heart_beat_info_map_) {
            (*extinfo_map)[item.first] = item.second;
        }
    }

    std::string data;
    if (!heart_beat_info.SerializeToString(&data)) {
        TOP_INFO_NAME("Heartbeat SerializeToString failed!");
        return kKadFailed;
    }

    if (!data.empty()) {
        message.set_data(data);
    }
    return SendData(message, node_ptr);
}

void RoutingTable::GetExistsNodesBloomfilter(
        const std::vector<NodeInfoPtr>& nodes,
        std::vector<uint64_t>& bloomfilter_vec) {
    base::Uint64BloomFilter bloomfilter{ kFindNodesBloomfilterBitSize, kFindNodesBloomfilterHashNum };
    for (uint32_t i = 0; i < nodes.size(); ++i) {
        bloomfilter.Add(nodes[i]->node_id);
    }
    bloomfilter_vec = bloomfilter.Uint64Vector();
}

void RoutingTable::HandleMessage(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet) {}

bool RoutingTable::IsDestination(const std::string& des_node_id, bool check_closest) {
    if (des_node_id == local_node_ptr_->id()) {
        return true;
    }

    if (!check_closest) {
        return false;
    }

    bool closest = false;
    if (ClosestToTarget(des_node_id, closest) != kKadSuccess) {
        TOP_WARN_NAME("this message must drop! this node is not des "
            "but nearest node is this node![%s] to [%s]",
            HexSubstr(local_node_ptr_->id()).c_str(),
            HexSubstr(des_node_id).c_str());
        return false;
    }

    return closest;
}

void RoutingTable::HandleFindNodesRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (message.des_node_id() != local_node_ptr_->id()) {
        TOP_ERROR_NAME("find nodes must direct![des: %s] [local: %s] [msg.src: %s] [msg.is_root: %d]",
                HexEncode(message.des_node_id()).c_str(),
                HexEncode(local_node_ptr_->id()).c_str(),
                HexEncode(message.src_node_id()).c_str(),
                message.is_root());
        return;
    }

    if (!message.has_data() || message.data().empty()) {
        TOP_INFO_NAME("HandleFindNodesRequest request in data is empty.");
        return;
    }

    protobuf::FindClosestNodesRequest find_nodes_req;
    if (!find_nodes_req.ParseFromString(message.data())) {
        TOP_INFO_NAME("FindClosestNodesRequest ParseFromString from string failed!");
        return;
    }

    // asker node canadd to local routingtable?
    auto src_nodeinfo = find_nodes_req.src_nodeinfo();
    NodeInfoPtr req_src_node_ptr;
    req_src_node_ptr.reset(new NodeInfo(src_nodeinfo.id()));
    req_src_node_ptr->local_ip = src_nodeinfo.local_ip();
    req_src_node_ptr->local_port = src_nodeinfo.local_port();
    req_src_node_ptr->public_ip = src_nodeinfo.public_ip();
    req_src_node_ptr->public_port = src_nodeinfo.public_port();
    req_src_node_ptr->nat_type = src_nodeinfo.nat_type();
    req_src_node_ptr->xip = src_nodeinfo.xip();
    req_src_node_ptr->xid = src_nodeinfo.xid();
    req_src_node_ptr->hash64 = base::xhash64_t::digest(req_src_node_ptr->xid);

    // just consider public ip here
    if (req_src_node_ptr->nat_type == kNatTypePublic){
        if (CanAddNode(req_src_node_ptr)) {
            AddNode(req_src_node_ptr);
        }
    }
 
    std::vector<uint64_t> bloomfilter_vec;
    for (auto i = 0; i < find_nodes_req.bloomfilter_size(); ++i) {
        bloomfilter_vec.push_back(find_nodes_req.bloomfilter(i));
    }

    std::shared_ptr<base::Uint64BloomFilter> new_bloomfilter;
    if (bloomfilter_vec.empty()) {
        new_bloomfilter = std::make_shared<base::Uint64BloomFilter>(
                kadmlia::kFindNodesBloomfilterBitSize,
                kadmlia::kFindNodesBloomfilterHashNum);
    } else {
        new_bloomfilter = std::make_shared<base::Uint64BloomFilter>(
                bloomfilter_vec,
                kadmlia::kFindNodesBloomfilterHashNum);
    }

    std::vector<NodeInfoPtr> closest_nodes = GetClosestNodes(
            find_nodes_req.target_id(),
            find_nodes_req.count() + 1);
    TOP_DEBUG_NAME("bluefind closest_nodes.size=%d", (int)closest_nodes.size());
    std::string find_nodes;
    protobuf::FindClosestNodesResponse find_nodes_res;
    // local_node
    if (local_node_ptr_->first_node()) {
        if (!new_bloomfilter->Contain(local_node_ptr_->id())) {
            protobuf::NodeInfo* node_ptr = find_nodes_res.add_nodes();
            node_ptr->set_id(local_node_ptr_->id());
            node_ptr->set_public_ip(local_node_ptr_->local_ip());
            node_ptr->set_public_port(local_node_ptr_->local_port());
            node_ptr->set_nat_type(local_node_ptr_->nat_type());
            node_ptr->set_xip(local_node_ptr_->xip());
            node_ptr->set_xid(global_xid->Get());
            find_nodes += local_node_ptr_->local_ip() + ", ";
        }
    } else {
        if (!local_node_ptr_->public_ip().empty() && local_node_ptr_->public_port() > 0) {  // public node?
            if (!new_bloomfilter->Contain(local_node_ptr_->id())) {
                protobuf::NodeInfo* node_ptr = find_nodes_res.add_nodes();
                node_ptr->set_id(local_node_ptr_->id());
                node_ptr->set_public_ip(local_node_ptr_->public_ip());
                node_ptr->set_public_port(local_node_ptr_->public_port());
                node_ptr->set_nat_type(local_node_ptr_->nat_type());
                node_ptr->set_xip(local_node_ptr_->xip());
                node_ptr->set_xid(global_xid->Get());
                find_nodes += local_node_ptr_->public_ip() + ", ";
            }
        }
    }

    for (uint32_t i = 0; i < closest_nodes.size(); ++i) {
        if (closest_nodes[i]->node_id == find_nodes_req.target_id()) {
            continue;
        }

        if (new_bloomfilter->Contain(closest_nodes[i]->node_id)) {
            continue;
        }

        // TODO(smaug) response differently base FindNeighbours-times
        if (find_nodes_res.nodes_size() >= 16) {
            break;
        }
        

        protobuf::NodeInfo* tmp_node = find_nodes_res.add_nodes();
        tmp_node->set_id(closest_nodes[i]->node_id);
        tmp_node->set_public_ip(closest_nodes[i]->public_ip);
        tmp_node->set_public_port(closest_nodes[i]->public_port);
        tmp_node->set_local_ip(closest_nodes[i]->local_ip);
        tmp_node->set_local_port(closest_nodes[i]->local_port);
        tmp_node->set_nat_type(closest_nodes[i]->nat_type);
        tmp_node->set_xip(closest_nodes[i]->xip);
        tmp_node->set_xid(closest_nodes[i]->xid);
        find_nodes += closest_nodes[i]->public_ip + ", ";
    }

    if (find_nodes_res.nodes_size() <= 0) {
        return;
    }
    TOP_DEBUG_NAME("HandleFindNodesRequest: get %d nodes", find_nodes_res.nodes_size());
    TOP_DEBUG_NAME("<bluefind> recv_find: %d nodes from node %s", find_nodes_res.nodes_size(), HexSubstr(message.src_node_id()).c_str());

    std::string data;
    if (!find_nodes_res.SerializeToString(&data)) {
        TOP_WARN_NAME("ConnectResponse SerializeToString failed!");
        return;
    }

    transport::protobuf::RoutingMessage res_message;
    SetFreqMessage(res_message);  // for RootRouting, this virtual func will set is_root true
    res_message.set_des_node_id(message.src_node_id());
    res_message.set_type(kKadFindNodesResponse);
    res_message.set_id(message.id());
    res_message.set_data(data);
    message.set_priority(enum_xpacket_priority_type_flash);
    SendData(
        res_message,
        packet.get_from_ip_addr(),
        packet.get_from_ip_port());
}

void RoutingTable::HandleFindNodesResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (message.des_node_id() != local_node_ptr_->id()) {
        TOP_ERROR_NAME("find nodes response error. must direct receive!");
        return;
    }

    if (!message.has_data() || message.data().empty()) {
        TOP_INFO_NAME("HandleFindNodesResponse data is empty.");
        return;
    }

    protobuf::FindClosestNodesResponse find_nodes_res;
    if (!find_nodes_res.ParseFromString(message.data())) {
        TOP_INFO_NAME("FindClosestNodesResponse ParseFromString from string failed!");
        return;
    }

    TOP_DEBUG_NAME("HandleFindNodesResponse get %d nodes", find_nodes_res.nodes_size());
    for (int i = 0; i < find_nodes_res.nodes_size(); ++i) {
        // TOP_FATAL_NAME("find nodes: %s(%s:%d)", HexEncode(find_nodes_res.nodes(i).id()).c_str(),
        //     find_nodes_res.nodes(i).public_ip().c_str(), (int)find_nodes_res.nodes(i).public_port());
        NodeInfoPtr node_ptr;
        node_ptr.reset(new NodeInfo(find_nodes_res.nodes(i).id()));
        node_ptr->local_ip = find_nodes_res.nodes(i).local_ip();
        node_ptr->local_port = find_nodes_res.nodes(i).local_port();
        node_ptr->public_ip = find_nodes_res.nodes(i).public_ip();
        node_ptr->public_port = find_nodes_res.nodes(i).public_port();
        node_ptr->service_type = message.src_service_type(); // for RootRouting, is always kRoot
        node_ptr->nat_type = find_nodes_res.nodes(i).nat_type();
        node_ptr->xip = find_nodes_res.nodes(i).xip();
        node_ptr->xid = find_nodes_res.nodes(i).xid();
        node_ptr->hash64 = base::xhash64_t::digest(node_ptr->xid);
        if (CanAddNode(node_ptr)) {
            if (node_ptr->public_ip == local_node_ptr_->public_ip() &&
                    node_ptr->public_port == local_node_ptr_->public_port()) {
                if (node_ptr->node_id != local_node_ptr_->id()) {
                    TOP_DEBUG_NAME("bluenat[%d] get nat_type(%d) of node(%s:%d-%d)",
                        local_node_ptr_->service_type(), node_ptr->nat_type,
                        node_ptr->public_ip.c_str(), node_ptr->public_port, node_ptr->service_type);
                    node_ptr->xid = global_xid->Get();
                    node_ptr->hash64 = base::xhash64_t::digest(node_ptr->xid);
                    if (AddNode(node_ptr) == kKadSuccess) {
                        TOP_DEBUG_NAME("update add_node(%s) from find node response(%s, %s:%d)",
                                HexSubstr(node_ptr->node_id).c_str(), HexSubstr(message.src_node_id()).c_str(),
                                packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
                    }
                }
                continue;
            }

            if (local_node_ptr_->nat_type() == kNatTypeConeAbnormal
                    && node_ptr->nat_type == kNatTypeConeAbnormal) {
                TOP_DEBUG_NAME("bluenat[%d] both node is abnormal, ignore connect",
                    local_node_ptr_->service_type());
                continue;
            }

            TOP_DEBUG("find node: %s:%d public:%d",
                    node_ptr->public_ip.c_str(),
                    node_ptr->public_port,
                    (node_ptr->nat_type == kNatTypePublic)?true:false);
            if (node_ptr->nat_type == kNatTypePublic
                    || (node_ptr->local_ip == node_ptr->public_ip && node_ptr->local_port == node_ptr->public_port)) {
                AddNode(node_ptr);
            } else {
                node_detection_ptr_->AddDetectionNode(node_ptr);
                //SendConnectRequest(find_nodes_res.nodes(i).id(), message.src_service_type());
                SendConnectRequest(
                        message.src_node_id(),
                        packet.get_from_ip_addr(),
                        packet.get_from_ip_port(),
                        find_nodes_res.nodes(i).id(),
                        message.src_service_type());
            }
        } // end if (CanAddNode ..
    }
}

void RoutingTable::SendConnectRequest(const std::string& id, uint64_t service_type) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(service_type);
    message.set_des_node_id(id);
    message.set_type(kKadConnectRequest);
    message.set_priority(enum_xpacket_priority_type_flash);
    if (local_node_ptr_->client_mode()) {
        message.set_client_msg(true);
        message.set_relay_flag(true);
        message.set_request_type(true);
        message.set_client_id(local_node_ptr_->id());
    }

    protobuf::ConnectReq conn_req;
    conn_req.set_local_ip(local_node_ptr_->local_ip());
    conn_req.set_local_port(local_node_ptr_->local_port());
    conn_req.set_public_ip(local_node_ptr_->public_ip());
    conn_req.set_public_port(local_node_ptr_->public_port());
    conn_req.set_nat_type(local_node_ptr_->nat_type());
    std::string data;
    if (!conn_req.SerializeToString(&data)) {
        TOP_INFO_NAME("ConnectReq SerializeToString failed!");
        return;
    }
    message.set_data(data);
    if (local_node_ptr_->public_ip().empty()) {
        TOP_ERROR_NAME("local node public ip is empty.");
        return;
    }
    // TOP_FATAL_NAME("'%s' [%lx] SendConnectRequest to '%s' [%lx]",
    //     HexSubstr(local_node_ptr_->id()).c_str(), (unsigned long)local_node_ptr_->service_type(),
    //     HexSubstr(message.des_node_id()).c_str(), (unsigned long)message.des_service_type());
    // TOP_FATAL_NAME("message: %s", message.DebugString().c_str());
    SendToClosestNode(message);
}

void RoutingTable::SendConnectRequest(
        const std::string& findnodes_routing_id,
        const std::string& ip,
        uint16_t port,
        const std::string& id,
        uint64_t service_type) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message); // for root-routing, will set is_root true
    message.set_des_service_type(service_type);
    message.set_des_node_id(id);
    message.set_type(kKadConnectRequest);
    message.set_priority(enum_xpacket_priority_type_flash);
    if (local_node_ptr_->client_mode()) {
        message.set_client_msg(true);
        message.set_relay_flag(true);
        message.set_request_type(true);
        message.set_client_id(local_node_ptr_->id());
    }

    protobuf::ConnectReq conn_req;
    conn_req.set_local_ip(local_node_ptr_->local_ip());
    conn_req.set_local_port(local_node_ptr_->local_port());
    conn_req.set_public_ip(local_node_ptr_->public_ip());
    conn_req.set_public_port(local_node_ptr_->public_port());
    conn_req.set_nat_type(local_node_ptr_->nat_type());
    conn_req.set_relay_routing_id(findnodes_routing_id); // send connect msg to the node which find node from, let it relay
    std::string data;
    if (!conn_req.SerializeToString(&data)) {
        TOP_INFO_NAME("ConnectReq SerializeToString failed!");
        return;
    }
    message.set_data(data);
    if (local_node_ptr_->public_ip().empty()) {
        TOP_ERROR_NAME("local node public ip is empty.");
        return;
    }
    // using findnodes_response node's ip and port
    SendData(message, ip, port);
}


void RoutingTable::TellNeighborsDropAllNode() {
    return;
    auto tmp_nodes = nodes();
    std::cout << "TellNeighborsDropAllNode: " << tmp_nodes.size() << std::endl;
    for (auto iter = tmp_nodes.begin(); iter != tmp_nodes.end(); ++iter) {
        SendDropNodeRequest((*iter)->node_id);
    }
}

void RoutingTable::SendDropNodeRequest(const std::string& id) {
    if (local_node_ptr_->client_mode()) {
        return;
    }
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_node_id(id);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_type(kKadDropNodeRequest);
    message.set_priority(enum_xpacket_priority_type_flash);
    SendToClosestNode(message);
}

void RoutingTable::OnHeartbeatFailed(const std::string& ip, uint16_t port) {
    std::vector<NodeInfoPtr> failed_nodes;
    {
        std::unique_lock<std::mutex> lock(nodes_mutex_);
        for (auto& node : nodes_) {
            if (node->public_ip == ip && node->public_port == port) {
                failed_nodes.push_back(node);
            }
        }
    }

    for (auto& node : failed_nodes) {
        DropNode(node);
        TOP_WARN_NAME("[%ld] node heartbeat error after tried: %d times.ID:[%s],"
                "IP:[%s],Port[%d] to ID:[%s],IP[%s],Port[%d] drop it.",
                (long)this,
                node->heartbeat_count,
                HexSubstr(local_node_ptr_->id()).c_str(),
                local_node_ptr_->local_ip().c_str(),
                local_node_ptr_->local_port(),
                HexSubstr(node->node_id).c_str(),
                node->local_ip.c_str(),
                node->local_port);
    }
}

void RoutingTable::HandleNodeQuit(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!IsDestination(message.des_node_id(), false)) {
        return;
    }
    NodeInfoPtr node_ptr;
    node_ptr.reset(new NodeInfo(message.src_node_id()));
    node_ptr->xid = message.xid();
    node_ptr->hash64 = base::xhash64_t::digest(node_ptr->xid);
    DropNode(node_ptr);
}

void RoutingTable::HandleConnectRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    protobuf::ConnectReq conn_req;
    if (!conn_req.ParseFromString(message.data())) {
        TOP_INFO_NAME("ConnectRequest ParseFromString from string failed!");
        return;
    }

    if (!IsDestination(message.des_node_id(), false)) {
        // mostly this is relay_routing, than send to the real node
        SendToClosestNode(message);
        return;
    }

    NodeInfoPtr node_ptr;
    node_ptr.reset(new NodeInfo(message.src_node_id()));
    node_ptr->local_ip = conn_req.local_ip();
    node_ptr->local_port = conn_req.local_port();
    node_ptr->public_ip = conn_req.public_ip();
    node_ptr->public_port = conn_req.public_port();
    node_ptr->service_type = message.src_service_type();
    node_ptr->nat_type = conn_req.nat_type();

    if (local_node_ptr_->nat_type() == kNatTypeConeAbnormal
            && node_ptr->nat_type == kNatTypeConeAbnormal) {
        TOP_INFO_NAME("bluenat[%d] both node is abnormal, ignore connect",
            local_node_ptr_->service_type());
        return;
    }
    TOP_DEBUG_NAME("connect comming: %s", node_ptr->public_ip.c_str());
    node_detection_ptr_->AddDetectionNode(node_ptr);
}

void RoutingTable::HandleHandshake(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    if (!IsDestination(message.des_node_id(), false)) {
        TOP_WARN_NAME("handshake message destination id error![%s][%s][%s]",
                HexSubstr(message.src_node_id()).c_str(),
                HexSubstr(message.des_node_id()).c_str(),
                HexSubstr(local_node_ptr_->id()).c_str());
        return;
    }

    if (!message.has_data() || message.data().empty()) {
        TOP_WARN_NAME("connect request in data is empty.");
        return;
    }

    protobuf::Handshake handshake;
    if (!handshake.ParseFromString(message.data())) {
        TOP_WARN_NAME("ConnectRequest ParseFromString from string failed!");
        return;
    }

    NodeInfoPtr node_ptr;
    node_ptr.reset(new NodeInfo(message.src_node_id()));
    std::string pub_ip = handshake.public_ip();
    uint16_t pub_port = handshake.public_port();
    if (pub_ip.empty()) {
        pub_ip = packet.get_from_ip_addr();
        pub_port = packet.get_from_ip_port();
    }
    assert(!pub_ip.empty());
    node_ptr->public_ip = pub_ip;
    node_ptr->public_port = pub_port;
    node_ptr->local_ip = handshake.local_ip();
    node_ptr->local_port = handshake.local_port();
    node_ptr->service_type = message.src_service_type();
    node_ptr->nat_type = handshake.nat_type();
    node_ptr->xid = handshake.xid();
    node_ptr->hash64 = base::xhash64_t::digest(node_ptr->xid);
    node_ptr->xip = handshake.xip();
    if (handshake.type() == kHandshakeResponse) {
        node_detection_ptr_->RemoveDetection(node_ptr->public_ip, node_ptr->public_port);
        if ((packet.get_from_ip_addr() == node_ptr->local_ip &&
                packet.get_from_ip_port() == node_ptr->local_port) &&
                node_ptr->public_ip != node_ptr->local_ip) {
            node_ptr->same_vlan = true;
        }

        if (!message.has_client_msg() || !message.client_msg()) {
            if (AddNode(node_ptr) == kKadSuccess) {
                TOP_DEBUG_NAME("update add_node(%s) from handshake(%s, %s:%d)",
                        HexSubstr(node_ptr->node_id).c_str(), HexSubstr(message.src_node_id()).c_str(),
                        packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
            }
        }

        if (!joined_) {
            if (!SetJoin(
                    message.src_node_id(),
                    handshake.public_ip(),
                    handshake.public_port())) {
                TOP_INFO_NAME("ignore BootstrapJoinResponse because this node already joined");
            }
        }
        return;
    }

    // kHandshakeRequest
    if (CanAddNode(node_ptr)) {
        node_detection_ptr_->AddDetectionNode(node_ptr);  // TODO(Charlie): nat detection will fail
    }
    handshake.set_type(kHandshakeResponse);
    handshake.set_local_ip(local_node_ptr_->local_ip());
    handshake.set_local_port(local_node_ptr_->local_port());
    handshake.set_public_ip(local_node_ptr_->public_ip());
    handshake.set_public_port(local_node_ptr_->public_port());
    handshake.set_nat_type(local_node_ptr_->nat_type());
    handshake.set_xid(global_xid->Get());
    handshake.set_xip(local_node_ptr_->xip());
    std::string data;
    if (!handshake.SerializeToString(&data)) {
        TOP_ERROR_NAME("ConnectResponse SerializeToString failed!");
        return;
    }

    transport::protobuf::RoutingMessage res_message;
    SetFreqMessage(res_message);
    res_message.set_src_service_type(message.des_service_type());
    res_message.set_des_service_type(message.src_service_type());
    res_message.set_src_node_id(message.des_node_id());
    res_message.set_des_node_id(message.src_node_id());
    res_message.set_type(kKadHandshake);
    res_message.set_id(message.id());
    message.set_priority(enum_xpacket_priority_type_flash);
    if (local_node_ptr_->client_mode()) {
        res_message.set_client_msg(true);
    }

    res_message.set_data(data);
    SendData(
        res_message,
        packet.get_from_ip_addr(),
        packet.get_from_ip_port());
}

void RoutingTable::HandleBootstrapJoinRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_DEBUG_NAME("HandleBootstrapJoinRequest from %s:%d",
        packet.get_from_ip_addr().c_str(), (int)packet.get_from_ip_port());
    bool allow_add = true;
    if (!message.has_data() || message.data().empty()) {
        TOP_INFO_NAME("HandleBootstrapJoinRequest request in data is empty.");
        return;
    }

    protobuf::BootstrapJoinRequest join_req;
    if (!join_req.ParseFromString(message.data())) {
        TOP_INFO_NAME("BootstrapJoinRequest ParseFromString from string failed!");
        return;
    }

    NodeInfoPtr node_ptr;
    node_ptr.reset(new NodeInfo(message.src_node_id()));
    node_ptr->local_ip = join_req.local_ip();
    node_ptr->local_port = join_req.local_port();
    node_ptr->public_ip = packet.get_from_ip_addr();
    node_ptr->public_port = packet.get_from_ip_port();
    node_ptr->service_type = message.src_service_type();
    node_ptr->nat_type = join_req.nat_type();
    node_ptr->xid = join_req.xid();
    node_ptr->hash64 = base::xhash64_t::digest(node_ptr->xid);
    node_ptr->xip = join_req.xip();
    SendBootstrapJoinResponse(message, packet);
    if(!allow_add) {
        TOP_DEBUG_NAME("HandleBootstrapJoinRequest not allow put it in routingtable");
    } else if(!message.has_client_msg() || !message.client_msg()) {
        TOP_DEBUG_NAME("[%llu] get nat_type(%d) of node(%s:%d-%llu)",
            local_node_ptr_->service_type(), node_ptr->nat_type,
            node_ptr->public_ip.c_str(), node_ptr->public_port, node_ptr->service_type);
        if (AddNode(node_ptr) == kKadSuccess) {
            TOP_DEBUG_NAME("update add_node(%s) from bootstrap request(%s, %s:%d)",
                    HexSubstr(node_ptr->node_id).c_str(), HexSubstr(message.src_node_id()).c_str(),
                    packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
        }
    } else {
        TOP_DEBUG_NAME("HandleBootstrapJoinRequest client node come,don't put it in routingtable");
    }
}

void RoutingTable::SendBootstrapJoinResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_DEBUG_NAME("SendBootstrapJoinResponse to (%s:%d)",
        packet.get_from_ip_addr().c_str(), (int)packet.get_from_ip_port());
    transport::protobuf::RoutingMessage res_message;
    // TODO(smaug) message.des_service_type maybe not equal the service_type of this routing table
    SetFreqMessage(res_message);
    res_message.set_src_service_type(message.des_service_type());
    res_message.set_des_service_type(message.src_service_type());
    res_message.set_des_node_id(message.src_node_id());
    res_message.set_type(kKadBootstrapJoinResponse);
    res_message.set_id(message.id());
    message.set_priority(enum_xpacket_priority_type_flash);
    res_message.set_debug("join res");
    res_message.set_status(message.status());
    if (message.has_client_msg() && message.client_msg()) {
        res_message.set_client_msg(true);
    }

    protobuf::BootstrapJoinResponse join_res;
    join_res.set_public_ip(packet.get_from_ip_addr());
    join_res.set_public_port(packet.get_from_ip_port());
    join_res.set_xid(global_xid->Get());
    join_res.set_xip(local_node_ptr_->xip());

    // dispatch dynamic xip for bootstrap node TODO(smaug) may be just for client
    if (message.has_client_msg() && message.client_msg()) {
        std::string dy_xip = dy_manager_->DispatchDynamicXip(local_node_ptr_->GetXipParser());
        join_res.set_dxip(dy_xip);
        ClientNodeInfoPtr client_ptr = std::make_shared<ClientNodeInfo>();
        client_ptr->public_ip = packet.get_from_ip_addr();
        client_ptr->public_port = packet.get_from_ip_port();
        dy_manager_->AddClientNode(dy_xip, client_ptr);
        TOP_INFO_NAME("dy_manager dispatch xip(%s) and add node[%s:%d]",
                HexEncode(dy_xip).c_str(),
                packet.get_from_ip_addr().c_str(),
                packet.get_from_ip_port());
    }

    if (join_res.public_ip().empty() || join_res.public_port() <= 0) {
        TOP_WARN_NAME("join node [%s] get public ip or public port failed!",
            HexSubstr(message.src_node_id()).c_str());
        return;
    }

    join_res.set_bootstrap_id(local_node_ptr_->id());
    join_res.set_nat_type(local_node_ptr_->nat_type());
    std::string data;
    if (!join_res.SerializeToString(&data)) {
        TOP_INFO_NAME("ConnectResponse SerializeToString failed!");
        return;
    }

    res_message.set_data(data);
    SendData(
        res_message,
        packet.get_from_ip_addr(),
        packet.get_from_ip_port());
}

void RoutingTable::HandleBootstrapJoinResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_DEBUG_NAME("HandleBootstrapJoinResponse from %s:%d",
        packet.get_from_ip_addr().c_str(), (int)packet.get_from_ip_port());
    if (!IsDestination(message.des_node_id(), false)) {
        TOP_WARN_NAME("HandleBootstrapJoinResponse message destination id error![%s][%s][%s]",
            HexSubstr(message.src_node_id()).c_str(),
            HexSubstr(message.des_node_id()).c_str(),
            HexSubstr(local_node_ptr_->id()).c_str());
        return;
    }

    if (!message.has_data() || message.data().empty()) {
        TOP_INFO_NAME("ConnectResponse data is empty.");
        return;
    }

    protobuf::BootstrapJoinResponse join_res;
    if (!join_res.ParseFromString(message.data())) {
        TOP_INFO_NAME("ConnectResponse ParseFromString failed!");
        return;
    }

    if(kadmlia::kKadForbidden == message.status()) {
        TOP_INFO_NAME("BootstrapJoin Request Is Forbidden.");
        return;
    }


    NodeInfoPtr node_ptr;
    node_ptr.reset(new NodeInfo(message.src_node_id()));
    node_ptr->local_ip = packet.get_from_ip_addr();
    node_ptr->local_port = packet.get_from_ip_port();
    node_ptr->public_ip = packet.get_from_ip_addr();
    node_ptr->public_port = packet.get_from_ip_port();
    node_ptr->service_type = message.src_service_type();
    node_ptr->nat_type = join_res.nat_type();
    node_ptr->xid = join_res.xid();
    node_ptr->hash64 = base::xhash64_t::digest(node_ptr->xid);
    node_ptr->xip = join_res.xip();

    {
        std::unique_lock<std::mutex> lock(joined_mutex_);
        local_node_ptr_->set_public_ip(join_res.public_ip());
        local_node_ptr_->set_public_port(join_res.public_port());
        // TODO(smaug) just set dynamic xip for real client
        if (local_node_ptr_->client_mode()) {
            local_node_ptr_->AddDxip(message.src_node_id(), join_res.dxip());
            TOP_DEBUG_NAME("BootstrapJoinResponse client get dynamicxip %s",
                    HexEncode(join_res.dxip()).c_str());
        }
    }
    int ret = AddNode(node_ptr);
    {
        std::unique_lock<std::mutex> bootstrap_lock(bootstrap_nodes_mutex_);
        bootstrap_nodes_.push_back(node_ptr);
        TOP_INFO("add bootstrap node success. id:%s ip:%s port:%d", HexEncode(node_ptr->node_id).c_str(), (node_ptr->public_ip).c_str(), node_ptr->public_port);
    }
    if (ret != kKadSuccess && ret != kKadNodeHasAdded) {
        TOP_ERROR_NAME("bootstrap node must add success!");
        return;
    }

    if (ret == kKadSuccess) {
        TOP_DEBUG_NAME("update add_node(%s) from boot response(%s, %s:%d)",
                HexSubstr(node_ptr->node_id).c_str(), HexSubstr(message.src_node_id()).c_str(),
                packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
    }

    if (!SetJoin(
            join_res.bootstrap_id(),
            packet.get_from_ip_addr(),
            packet.get_from_ip_port())) {
        TOP_INFO_NAME("ignore BootstrapJoinResponse because this node already joined");
        return;
    }

    std::vector<NodeInfoPtr> nodes;
    FindClosestNodes(1, GetFindNodesMaxSize(), nodes);
    WakeBootstrap();
    return;
}

void RoutingTable::HandleHeartbeatRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_WARN_NAME("HandleHeartbeatRequest from %s:%d", packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
    if (!IsDestination(message.des_node_id(), false)) {
        return;
    }

    protobuf::Heartbeat heart_beat_info;
    if (!heart_beat_info.ParseFromString(message.data())) {
        TOP_INFO_NAME("Heartbeat ParseFromString from string failed!");
        return;
    }
    if (heart_beat_callback_) {
        ::google::protobuf::Map< ::std::string, ::std::string > extinfo_map
            = heart_beat_info.extinfo_map();
        std::map<std::string, std::string> heart_beat_info_map;
        for (auto it = extinfo_map.begin(); it != extinfo_map.end(); ++it) {
            heart_beat_info_map[it->first] = it->second;
        }
        heart_beat_callback_(heart_beat_info_map);
    }

    ResetNodeHeartbeat(message.src_node_id());
    message.clear_hop_nodes();
    std::string src_id = message.des_node_id();
    message.set_type(kKadHeartbeatResponse);
    message.set_des_node_id(message.src_node_id());
    message.set_src_node_id(src_id);
    message.set_priority(enum_xpacket_priority_type_flash);
    message.set_des_service_type(message.src_service_type());
    SendData(message, packet.get_from_ip_addr(), packet.get_from_ip_port());
}

void RoutingTable::HandleHeartbeatResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_WARN_NAME("HandleHeartbeatResponse from %s:%d", packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
    ResetNodeHeartbeat(message.src_node_id());
}

void RoutingTable::SupportSecurityJoin() {
    security_join_ptr_.reset(new security::XSecurityJoin(timer_manager_));
}

int RoutingTable::CheckAndSendRelay(transport::protobuf::RoutingMessage& message) {
    if (!message.has_client_id()) {
        return kKadFailed;
    }

    if (!message.relay_flag()) {
        return kKadFailed;
    }

    if (message.des_node_id() != local_node_ptr_->id()) {
        return kKadFailed;
    }

    ClientNodeInfoPtr client_node_ptr = ClientNodeManager::Instance()->FindClientNode(
            message.client_id());
    if (!client_node_ptr) {
        return kKadFailed;
    }

    message.set_relay_flag(false);
    message.set_des_node_id(client_node_ptr->node_id);
    SendData(message, client_node_ptr->public_ip, client_node_ptr->public_port);
    return kKadSuccess;
}

int RoutingTable::CheckAndSendMultiRelay(transport::protobuf::RoutingMessage& message) {
    if (!message.has_multi_relay() || !message.multi_relay()) {
        return kKadFailed;
    }
    if (message.has_is_root() && message.is_root()) {
        return kKadFailed;
    }
    if (message.des_node_id() != local_node_ptr_->id()) {
        return kKadFailed;
    }
    int relay_hop_info_size = message.relay_hop_info_size();
    // jude the original asker
    if (relay_hop_info_size <= 0) {
        return kKadFailed;
    }
    const transport::protobuf::RelayHopInfo& last_relay_hop_info =
        message.relay_hop_info(relay_hop_info_size - 1);

    // get last hop info
    std::string last_relay_hop_entry_id = last_relay_hop_info.relay_entry_id();
    std::string last_relay_hop_exit_id = last_relay_hop_info.relay_exit_id();
    uint64_t last_relay_hop_service_type = last_relay_hop_info.service_type();

    message.set_des_node_id(last_relay_hop_entry_id);
    message.set_des_service_type(last_relay_hop_service_type);

    std::string ip;
    uint16_t port;
    ClientNodeInfoPtr client_node_ptr =
        ClientNodeManager::Instance()->FindClientNode(last_relay_hop_exit_id);
    if (!client_node_ptr) {
        if (!local_node_ptr_->first_node()) {
            TOP_ERROR_NAME("<smaug>MultiRelay:: response arrive the %d relay node[%s] from bottom,"
                    "but can't find cache_relay info",
                    relay_hop_info_size,
                    HexEncode(local_node_ptr_->id()).c_str());
            return kKadFailed;
        }

        // specially for first node, TODO(smaug)  use a better way in the future
        ip = local_node_ptr_->local_ip();
        port = local_node_ptr_->local_port();
    } else {
        ip = client_node_ptr->public_ip;
        port = client_node_ptr->public_port;
    }

    SendData(message, ip, port);
    TOP_DEBUG_NAME("<smaug>MultiRelay:: SendData from [%s] to [%s][%s:%d]",
            HexEncode(local_node_ptr_->id()).c_str(),
            HexEncode(last_relay_hop_exit_id).c_str(),
            ip.c_str(),
            port);
    return kKadSuccess;
}

void RoutingTable::SetMultiRelayMsg(
        const transport::protobuf::RoutingMessage& message,
        transport::protobuf::RoutingMessage& res_message){
    if (message.has_multi_relay()) {
        res_message.set_multi_relay(message.multi_relay());
    }
    // keep relay_hop_info of this message (from original asker to this replier)
    int req_relay_hop_info_size = message.relay_hop_info_size();
    // attention: the last one hop_info will be abandoned
    for (int i = 0; i < req_relay_hop_info_size - 1; ++i) {
        const transport::protobuf::RelayHopInfo& req_relay_hop_info = message.relay_hop_info(i);
        transport::protobuf::RelayHopInfo* res_relay_hop_info = res_message.add_relay_hop_info();
        res_relay_hop_info->set_relay_entry_id(req_relay_hop_info.relay_entry_id());
        res_relay_hop_info->set_relay_exit_id(req_relay_hop_info.relay_exit_id());
        res_relay_hop_info->set_service_type(req_relay_hop_info.service_type());
        res_relay_hop_info->set_relay_flag(req_relay_hop_info.relay_flag());
    }

    if (message.has_xrequest_id()) {
        // set response xrequest_id
        res_message.set_xrequest_id("AAA"+ message.xrequest_id());
    }
    int req_trace_route_size = message.trace_route_size();
    for (int i = 0; i < req_trace_route_size; ++i) {
        res_message.add_trace_route(message.trace_route(i));
    }
}

// only use when send reply(response)
void RoutingTable::SmartSendReply(transport::protobuf::RoutingMessage& res_message) {
    if (CheckAndSendMultiRelay(res_message) != kKadSuccess) {
        SendToClosestNode(res_message);
    }
}

// only use when send reply(response)
void RoutingTable::SmartSendReply(transport::protobuf::RoutingMessage& res_message, bool add_hop) {
    TOP_WARN_NAME("smart MultiRelay now send data: [%s] to [%s] client[%s]",
            HexEncode(res_message.src_node_id()).c_str(),
            HexEncode(res_message.des_node_id()).c_str(),
            HexEncode(res_message.client_id()).c_str());
    if (CheckAndSendMultiRelay(res_message) != kKadSuccess) {
        SendToClosestNode(res_message, add_hop);
    }
}

bool RoutingTable::StartBootstrapCacheSaver() {
    auto get_public_nodes = [this](std::vector<NodeInfoPtr>& nodes) {
        {
            std::unique_lock<std::mutex> lock(nodes_mutex_);
            for (auto& node_ptr : nodes_) {
                if (node_ptr->IsPublicNode())
                    nodes.push_back(node_ptr);
            }
        }

        if (!bootstrap_ip_.empty() && bootstrap_port_ >= 0) {
            auto node_ptr = std::make_shared<NodeInfo>();
            node_ptr->public_ip = bootstrap_ip_;
            node_ptr->public_port = bootstrap_port_;
            nodes.push_back(node_ptr);
        }
    };

    if (!bootstrap_cache_helper_->Start(local_node_ptr_->kadmlia_key(), get_public_nodes)) {
        TOP_ERROR_NAME("boostrap_cache_helper start failed");
        return false;
    }

    TOP_INFO_NAME("bootstrap_cache_helper start success");
    return true;
}

void RoutingTable::GetPubEndpoints(std::vector<std::string>& public_endpoints) {
    bootstrap_cache_helper_->GetPublicEndpoints(public_endpoints);
}

void RoutingTable::GetBootstrapCache(std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    bootstrap_cache_helper_->GetPublicEndpoints(boot_endpoints);
}

void RoutingTable::SetFreqMessage(transport::protobuf::RoutingMessage& message) {
    message.set_hop_num(0);
    message.set_src_service_type(local_node_ptr_->service_type());
    message.set_src_node_id(local_node_ptr_->id());
    message.set_xid(global_xid->Get());
    message.set_priority(enum_xpacket_priority_type_routine);
    message.set_id(CallbackManager::MessageId());
    if (message.broadcast()) {
        auto gossip = message.mutable_gossip();
        //gossip->set_neighber_count(gossip::kGossipSendoutMaxNeighbors);
        gossip->set_neighber_count(4);
        gossip->set_stop_times(gossip::kGossipSendoutMaxTimes);
        //gossip->set_gossip_type(gossip::kGossipBloomfilterAndLayered);
        gossip->set_gossip_type(gossip::kGossipBloomfilter);
        //gossip->set_max_hop_num(kHopToLive);
        gossip->set_max_hop_num(10);
        gossip->set_evil_rate(0);
        gossip->set_switch_layer_hop_num(gossip::kGossipSwitchLayerCount);
        gossip->set_ign_bloomfilter_level(gossip::kGossipBloomfilterIgnoreLevel);
    }
}

void RoutingTable::SetVersion(transport::protobuf::RoutingMessage& message) {
    if (!message.has_version_tag()) {
        transport::protobuf::VersionTag* version_tag  = message.mutable_version_tag();
        version_tag->set_version("0.6.0");
    }
}

void RoutingTable::SetSpeClientMessage(transport::protobuf::RoutingMessage& message) {
    return;
}


uint32_t RoutingTable::AddHeartbeatInfo(const std::string& key, const std::string& value) {
    std::unique_lock<std::mutex> lock(heart_beat_info_map_mutex_);
    heart_beat_info_map_[key] = value;
    return heart_beat_info_map_.size();
}

void RoutingTable::ClearHeartbeatInfo() {
    std::unique_lock<std::mutex> lock(heart_beat_info_map_mutex_);
    heart_beat_info_map_.clear();
}

void RoutingTable::RegisterHeartbeatInfoCallback(on_heart_beat_info_receive_callback_t heart_beat_callback) {
    std::unique_lock<std::mutex> lock(heart_beat_info_map_mutex_);
    assert(heart_beat_callback_ == nullptr);
    heart_beat_callback_ = heart_beat_callback;
}

void RoutingTable::UnRegisterHeartbeatInfoCallback() {
    std::unique_lock<std::mutex> lock(heart_beat_info_map_mutex_);
    heart_beat_callback_ = nullptr;
}

}  // namespace kadmlia

}  // namespace top
