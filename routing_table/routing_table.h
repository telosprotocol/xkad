// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <mutex>
#include <map>
#include <unordered_set>
#include <string>
#include <memory>
#include <thread>
#include <condition_variable>
#include <set>

#include "xpbase/base/top_timer.h"
#include "xpbase/base/top_config.h"
#include "xtransport/transport.h"
#include "xtransport/proto/transport.pb.h"
#include "xpbase/base/xid/xid_generator.h"
#include "xkad/routing_table/routing_utils.h"
#include "xkad/routing_table/node_info.h"
#include "xkad/proto/kadmlia.pb.h"
#include "xkad/routing_table/callback_manager.h"
#include "xkad/routing_table/bootstrap_cache_helper.h"
// #include "xkad/gossip/rumor_handler.h"
#include "xkad/routing_table/local_node_info.h"
#include "xkad/routing_table/dynamic_xip_manager.h"
#include "xsecurity/xsecurity_join.hpp"
#include "xbase/xbase.h"
#include "heartbeat_manager.h"

namespace top {

using on_heart_beat_info_receive_callback_t = std::function<void(std::map<std::string, std::string>& )>;

namespace kadmlia {

typedef std::function<void(int /*network_health*/)> NetworkStatusFunctor;

struct Functors {
    Functors()
        : network_status() {}

    NetworkStatusFunctor network_status;
};

class NodeDetectionManager;
class LocalNodeInfo;

class RoutingTable : public std::enable_shared_from_this<RoutingTable> {
public:
    RoutingTable(std::shared_ptr<transport::Transport>, uint32_t, std::shared_ptr<LocalNodeInfo>);
    virtual ~RoutingTable();
    virtual bool Init();
    virtual bool UnInit();
    virtual int AddNode(NodeInfoPtr node);
    virtual int DropNode(NodeInfoPtr node);
    virtual void GetPubEndpoints(std::vector<std::string>& public_endpoints);
    virtual void GetBootstrapCache(std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    virtual bool IsDestination(const std::string& des_node_id, bool check_closest);
    virtual void SetFreqMessage(transport::protobuf::RoutingMessage& message);
    virtual void SetSpeClientMessage(transport::protobuf::RoutingMessage& message);
    virtual uint64_t GetRoutingTableType() { return local_node_ptr_->service_type();}
    virtual void PrintRoutingTable();

    // message handler
    virtual void HandleMessage(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    virtual void HandleFindNodesRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void HandleFindNodesResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void SendConnectRequest(const std::string& id, uint64_t service_type);
    virtual void SendConnectRequest(
            const std::string& findnodes_routing_id,
            const std::string& ip,
            uint16_t port,
            const std::string& id,
            uint64_t service_type);
    virtual void HandleConnectRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void HandleHandshake(transport::protobuf::RoutingMessage& message, base::xpacket_t& packet);
    virtual void SendBootstrapJoinResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void HandleBootstrapJoinRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void HandleBootstrapJoinResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void HandleHeartbeatRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void HandleHeartbeatResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    virtual void SupportSecurityJoin();

public:
    std::shared_ptr<std::vector<kadmlia::NodeInfoPtr>> GetUnLockNodes() {
        return no_lock_for_use_nodes_;
    }

    void HandleNodeQuit(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void SortNodesByTargetXid(const std::string& target_xid, std::vector<NodeInfoPtr>& nodes);
//     void SpreadAllNeighborsRapid(transport::protobuf::RoutingMessage&);
//     bool SupportRumor(bool just_root);
    bool CanAddNode(NodeInfoPtr node);
    NodeInfoPtr GetRandomNode();
    void SetVersion(transport::protobuf::RoutingMessage& message);
    void SetMultiRelayMsg(
            const transport::protobuf::RoutingMessage& message,
            transport::protobuf::RoutingMessage& res_message);
    void SmartSendReply(transport::protobuf::RoutingMessage& res_message);
    void SmartSendReply(transport::protobuf::RoutingMessage& res_message, bool add_hop);
    int ClosestToTarget(const std::string& target, bool& closest);
    int ClosestToTarget(
            const std::string& target,
            const std::set<std::string>& exclude,
            bool& closest);
    std::vector<NodeInfoPtr> nodes();
    void GetRangeNodes(
            const uint64_t& min,
            const uint64_t& max,
            std::vector<NodeInfoPtr>& vec);
    void GetRangeNodes(
            uint32_t min_index,
            uint32_t max_index, // [,]
            std::vector<NodeInfoPtr>& vec);
    int32_t GetSelfIndex();
    uint32_t nodes_size();
    int CheckAndSendRelay(transport::protobuf::RoutingMessage& message);
    int MultiJoin(const std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    void MultiJoinAsync(const std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
    bool IsJoined();
    void SetUnJoin();
    void WakeBootstrap();
    void FindClosestNodes(int attempts, int count, const std::vector<NodeInfoPtr>& nodes);
    NodeInfoPtr GetNode(const std::string& id);
    NodeInfoPtr GetClosestNode(
            const std::string& target_id,
            bool not_self,
            const std::set<std::string>& exclude,
            bool base_xip = false);
    std::vector<NodeInfoPtr> GetClosestNodes(
            const std::string& target_id,
            uint32_t number_to_get,
            bool base_xip = false);
    void SendToClosestNode(transport::protobuf::RoutingMessage& message);
    void SendToClosestNode(transport::protobuf::RoutingMessage& message, bool add_hop);
    void ResetNodeHeartbeat(const std::string& id);
    bool CloserToTarget(
        const std::string& id1,
        const std::string& id2,
        const std::string& target_id);
    bool HasNode(NodeInfoPtr node);
    NodeInfoPtr FindLocalNode(const std::string node_id);
    void FindCloseNodesWithEndpoint(
            const std::string& des_node_id,
            const std::pair<std::string, uint16_t>& boot_endpoints);
    // use heartbeat to sync something
    uint32_t AddHeartbeatInfo(const std::string& key, const std::string& value);
    void ClearHeartbeatInfo();
    void RegisterHeartbeatInfoCallback(on_heart_beat_info_receive_callback_t heart_beat_callback);
    void UnRegisterHeartbeatInfoCallback();

    std::string bootstrap_id() {
        return bootstrap_id_;
    }
    void set_bootstrap_id(const std::string& id) {
        bootstrap_id_ = id;
    }
    std::string bootstrap_ip() {
        return bootstrap_ip_;
    }
    void set_bootstrap_ip(const std::string& ip) {
        bootstrap_ip_ = ip;
    }
    uint16_t bootstrap_port() {
        return bootstrap_port_;
    }
    void set_bootstrap_port(uint16_t port) {
        bootstrap_port_ = port;
    }
    std::shared_ptr<transport::Transport> get_transport() {
        return transport_ptr_;
    }
    std::shared_ptr<LocalNodeInfo> get_local_node_info() {
        return local_node_ptr_;
    }

    DynamicXipManagerPtr get_dy_manager() {
        return dy_manager_;
    }

    int SendData(
            const xbyte_buffer_t& data,
            const std::string& peer_ip,
            uint16_t peer_port,
            uint16_t priority = enum_xpacket_priority_type_priority);

    int SendData(
            transport::protobuf::RoutingMessage& message,
            const std::string& peer_ip,
            uint16_t peer_port);
    int SendData(transport::protobuf::RoutingMessage& message, NodeInfoPtr node_ptr);

protected:
    virtual int Bootstrap(
            const std::string& peer_ip,
            uint16_t peer_port,
            uint64_t des_service_type);
    virtual int SendFindClosestNodes(
            const std::string& node_id,
            int count,
            const std::vector<NodeInfoPtr>& nodes,
            uint64_t des_service_type);
    virtual int SendHeartbeat(NodeInfoPtr node_ptr, uint64_t des_service_type);

    bool SetJoin(const std::string& boot_id, const std::string& boot_ip, int boot_port);
    int CheckAndSendMultiRelay(transport::protobuf::RoutingMessage& message);
    // -1: all bits equal(and return kKadFailed)
    // 0: all bits equal expect the last bit
    // 1: all bits equal expect the last second bit
    // 8*kNodeIdSize-1: the first bit is different already
    int SetNodeBucket(NodeInfoPtr node);
    bool ValidNode(NodeInfoPtr node);
    int SortNodesByTargetXid(const std::string& target_xid, int number);
    int SortNodesByTargetXip(const std::string& target_xip, int number);
    // make sure nodes_ is sorted by kad algo
    virtual bool NewNodeReplaceOldNode(NodeInfoPtr node, bool remove);
    virtual uint32_t GetFindNodesMaxSize();
    void RecursiveSend(transport::protobuf::RoutingMessage& message, int retry_times);
    void HeartbeatProc();
    void HeartbeatCheckProc();
    void Rejoin();
    void FindNeighbours();
    void GetExistsNodesBloomfilter(
            const std::vector<NodeInfoPtr>& nodes,
            std::vector<uint64_t>& bloomfilter_vec);
    virtual bool StartBootstrapCacheSaver();
    void TellNeighborsDropAllNode();
    void SendDropNodeRequest(const std::string& id);
    void OnHeartbeatFailed(const std::string& ip, uint16_t port);
    void GetRandomAlphaNodes(std::map<std::string, std::string>& query_nodes);
    void GetClosestAlphaNodes(std::map<std::string, std::string>& query_nodes);
    void DumpNodes();

    uint32_t RoutingMaxNodesSize_;
    std::shared_ptr<transport::Transport> transport_ptr_;
    std::shared_ptr<LocalNodeInfo> local_node_ptr_;

    std::string name_{"<bluert>"};
    std::vector<NodeInfoPtr> nodes_;
    std::mutex nodes_mutex_;
    std::map<std::string, NodeInfoPtr> node_id_map_;
    std::mutex node_id_map_mutex_;
    std::shared_ptr<std::map<uint64_t, NodeInfoPtr>> node_hash_map_;
    std::mutex node_hash_map_mutex_;
    std::mutex bootstrap_mutex_;
    std::condition_variable bootstrap_cond_;
    std::mutex joined_mutex_;
    std::atomic<bool> joined_;
    std::string bootstrap_id_;
    std::string bootstrap_ip_;
    uint16_t bootstrap_port_;
    // keep the first bootstrap id
    std::vector<NodeInfoPtr> bootstrap_nodes_;
    std::mutex bootstrap_nodes_mutex_;

    int find_neighbour_num_;
    std::shared_ptr<NodeDetectionManager> node_detection_ptr_;
    base::TimerManager* timer_manager_{base::TimerManager::Instance()};
    std::shared_ptr<base::TimerRepeated> timer_rejoin_;
    std::shared_ptr<base::TimerRepeated> timer_find_neighbours_;
    std::shared_ptr<base::TimerRepeated> timer_heartbeat_;
    std::shared_ptr<base::TimerRepeated> timer_heartbeat_check_;
    std::shared_ptr<base::TimerRepeated> timer_prt_;
    bool destroy_;
    uint32_t find_nodes_period_;

    std::set<std::pair<std::string, uint16_t>> set_endpoints_;
    std::mutex set_endpoints_mutex_;
    bool after_join_;

    std::shared_ptr<kadmlia::BootstrapCacheHelper> bootstrap_cache_helper_;
    uint32_t kadmlia_key_len_;
//     bool support_rumor_;
//     gossip::RumorHandlerSptr rumor_handler_;
    DynamicXipManagerPtr dy_manager_;
    std::map<std::string, std::string> heart_beat_info_map_;
    std::mutex heart_beat_info_map_mutex_;
    on_heart_beat_info_receive_callback_t heart_beat_callback_;
    std::mutex heart_beat_callback_mutex_;
    std::shared_ptr<security::XSecurityJoin> security_join_ptr_;

private:
//     bool CheckRumorLicense() const;
    void SetTestTraceInfo(transport::protobuf::RoutingMessage& message);

    std::shared_ptr<std::vector<NodeInfoPtr>> no_lock_for_use_nodes_{ nullptr };

    DISALLOW_COPY_AND_ASSIGN(RoutingTable);
};  // class RoutingTable

typedef std::shared_ptr<RoutingTable> RoutingTablePtr;

}  // namespace kadmlia

}  // namespace top
