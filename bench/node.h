// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xpbase/base/top_utils.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/kad_key/platform_kadmlia_key.h"
#include "xtransport/udp_transport/udp_transport.h"
#include "xtransport/src/message_manager.h"
#include "util.h"

#define private public
#define protected public
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/local_node_info.h"
#include "xkad/routing_table/kad_message_handler.h"
#include "xtransport/message_manager/multi_message_handler.h"
#include "xkad/src/nat_manager.h"
#undef private
#undef protected
#include "xpbase/base/top_timer.h"
#include "node.pb.h"

namespace top {
namespace kadmlia {
namespace test {

const int kTestMsgBase              = kMessageTypeMax + 200;

const int kTestReportNodes          = kTestMsgBase + 0;         // send to super node

const int kTestConnRequest          = kTestMsgBase + 10;        // relay to des node
const int kTestConnResponse         = kTestMsgBase + 11;        // relay to src node
const int kTestConnReply            = kTestMsgBase + 12;        // send to src node

const int kTestConn2Request         = kTestMsgBase + 20;        // relay to des node
const int kTestConn2Response        = kTestMsgBase + 21;        // relay to src node

const int kTestGetrtRequest         = kTestMsgBase + 30;        // send to des node
const int kTestGetrtResponse        = kTestMsgBase + 31;        // send to src node

const int kTestGetrt2Request        = kTestMsgBase + 40;        // send to des node
const int kTestGetrt2Response       = kTestMsgBase + 41;        // send to src node

class MyRoutingTable : public RoutingTable {
public:
    MyRoutingTable(
            base::TimerManager* timer_manager,
            std::shared_ptr<transport::Transport> udp_transport,
            uint32_t kadmlia_key_len,
            std::shared_ptr<LocalNodeInfo> local_node_ptr);
    bool Init(const std::string& name_prefix, const std::string& supernode_ip, uint16_t supernode_port);
    virtual void HandleBootstrapJoinRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet) override;

    // ---------------- save/load all_nodes
    void OnCommandSave(const BenchCommand::Arguments& args);
    void OnCommandLoad(const BenchCommand::Arguments& args);

    // ---------------- prn
    void HandleReportNodes(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    struct NodeReport {
        int count{0};
        std::string ep;
    };
    struct StructPrn {
        time_t time_now;
        // struct Node {
        //     std::string ip_port;
        //     std::string
        // };
        std::map<std::string, NodeReport> details;
    };
    void PrintReportNodes(const StructPrn& struct_prn, int node_count);
    // void PrintNodes(int node_count);
    void GetReportNodes(StructPrn& struct_prn);
    void SendReportNodes();
    void OnCommandPrn(const BenchCommand::Arguments& args);
    void OnCommandPrt(const BenchCommand::Arguments& args);
    void OnCommandPrtSimple(const BenchCommand::Arguments& args);

    // ---------------- conn
    void OnCommandConn(const BenchCommand::Arguments& args);
    void HandleConnRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleConnResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleConnReply(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void SendConnReply(const std::string& reply, const std::string& ip, uint16_t port);
    void RelayConnRequest(const std::string& node_id);
    void RelayConnResponse(const std::string& data, const uint64_t des_service_type, const std::string& des_node_id);
    // ---------------- connvia
    void SendConnVia(NodeInfoPtr via_node, NodeInfoPtr des_node);
    void OnCommandConnVia(const BenchCommand::Arguments& args);

    // ---------------- conn2/query
    void HandleConn2Request(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleConn2Response(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void SendConn2Response(const std::string& data, const std::string& des_node_id, const std::string& ip, uint16_t port);
    void RelayConn2Response(const std::string& data, const uint64_t des_service_type, const std::string& des_node_id);
    void SendConn2Request(bool send_or_relay);
    void ClearQuery();
    void OnCommandConn2(const BenchCommand::Arguments& args);  // conn2 when directly sendto first node
    void OnCommandConn3(const BenchCommand::Arguments& args);  // conn + query
    void OnCommandQuery(const BenchCommand::Arguments& args);
    struct StructQuery {
        std::vector<std::string> hops;
        // std::vector<std::string> not_found_nodes;
        std::vector<NodeInfoPtr> not_found_nodes;
    };
    void GetQuery(StructQuery& struct_query);
    void DumpQuery(const StructQuery& struct_query);

    // ---------------- getrt
    void OnCommandGetrt(const BenchCommand::Arguments& args);
    void SendGetrtRequest(const std::string& ip, uint16_t port);
    void HandleGetrtRequest(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleGetrtResponse(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);

    // ---------------- getrt2/rtsize
    void ClearRtdiff();
    void OnCommandGetrt2(const BenchCommand::Arguments& args);
    void OnCommandRtsize(const BenchCommand::Arguments& args);
    void SendGetrt2Request(const std::string& ip, uint16_t port);
    void HandleGetrt2Request(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    void HandleGetrt2Response(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);
    std::string CompareDiffLines(const std::string& str1, const std::string& str2);
    void GetGetrt2(int& diff_count);
    int GetLines(const std::string& str);

    // ---------------- autotest
    void OnCommandAutoTest(const BenchCommand::Arguments& args);
    bool AutoTestAllBoot(int node_count);
    bool AutoTestAllFull(int rt_size);
    bool AutoTestAllConn();
    bool AutoTestAllGetrt2();
    void SleepWait(int seconds);

private:
    std::string GetDumpNodes(std::vector<NodeInfoPtr> nodes, int max_detail);  // columns: id/dis/pub/local/nat(3.3KB)
    std::string GetDumpNodesSimple(std::vector<NodeInfoPtr> nodes, int max_detail);  // columns: id/dis(1.16KB)
    std::string GetDumpSelf();
    long long GetSteadyTimepoint();
    int ParseArg(const std::string& arg, int default_value);
    NodeInfoPtr GetNode(const std::string& node_id);

private:
    std::string supernode_ip_;
    uint16_t supernode_port_{0};
    base::TimerManager* timer_manager_{base::TimerManager::Instance()};

    std::mutex all_nodes_mutex_;
    std::vector<NodeInfoPtr> all_nodes_;
    std::map<std::string, NodeInfoPtr> all_nodes_map_;

    // ---------------- prn
    std::mutex report_mutex_;
    std::shared_ptr<base::TimerRepeated> report_timer_;
    std::map<std::string, NodeReport> map_report_nodes_;

    // ---------------- conn2/query
    std::mutex test_conn_mutex_;
    struct NodeSearch {
        bool found{false};
        NodeInfoPtr node;
    };
    struct NodeHops {
        long long total{0};
        long long max{-1};
        long long min{9999};
        std::vector<NodeInfoPtr> nodes;
    };
    std::map<std::string, NodeSearch> search_map_;  // mark node found
    std::map<int, NodeHops> hops_map_;
    std::map<std::string, NodeInfoPtr> last_not_found_map_;

    // ---------------- getrt2
    std::mutex rt_map_mutex_;
    std::map<std::string, std::string> rt_map_;  // about 700KB for 600 nodes
    int rt_diff_count_{0};
    std::map<int, int> rt_size_map_;

    // ---------------- autotest
    long time_all_boot_{-1};  // prn and total is the all nodes, as the first_time
    long time_all_full_{-1};
    long time_all_conn_{-1};
    long time_all_getrt2_{-1};
};

class MyMessageHandler : public KadMessageHandler {
public:
    void Init();
};

class MyKadkey : public base::PlatformKadmliaKey {
public:
    MyKadkey(const std::string& str_for_hash, bool hash_tag)
            : PlatformKadmliaKey(str_for_hash, hash_tag) {
        node_id_ = str_for_hash;
    }

    virtual std::string Get() override {
        return node_id_;
    }

private:
    std::string node_id_;
};

class MyNodeMgr {
public:
    MyNodeMgr() {}
    ~MyNodeMgr();
    bool Init(const base::Config& config, const std::string& name);
    bool JoinRt();
    uint16_t RealLocalPort();
    void HandleMessage(
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet);

private:
    bool LoadConfig(const base::Config& config);
    bool NatDetect();

private:
    std::shared_ptr<base::TimerManager> timer_manager_impl_;
    MyMessageHandler message_handler_;
    transport::MessageManager message_manager_;
    std::shared_ptr<transport::MultiThreadHandler> message_threads_;
    transport::UdpTransportPtr udp_transport_;
    std::shared_ptr<LocalNodeInfo> local_node_info_;
    std::shared_ptr<MyRoutingTable> routing_table_;

    bool first_node_{false};
    std::string local_ip_;
    uint16_t local_port_{0};
    std::string supernode_ip_;
    uint16_t supernode_port_{0};
    uint16_t real_local_port_{0};  // system assigned

    transport::UdpTransportPtr nat_transport_;
    NatManagerIntf* nat_manager_{nullptr};
};

}  // namespace test
}  // namespace kadmlia
}  // namespace top
