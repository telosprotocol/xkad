// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "node.h"
#include "xpbase/base/top_log_name.h"
#include <functional>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "xpbase/base/top_string_util.h"
#include "xpbase/base/endpoint_util.h"

namespace top {
namespace kadmlia {
namespace test {

const int kSendTry = 1;

// --------------------------------------------------------------------------------
MyRoutingTable::MyRoutingTable(
        base::TimerManager* timer_manager,
        std::shared_ptr<transport::Transport> udp_transport,
        uint32_t kadmlia_key_len,
        std::shared_ptr<LocalNodeInfo> local_node_ptr)
        : RoutingTable(udp_transport, kadmlia_key_len, local_node_ptr) {
    timer_manager_ = timer_manager;
    RoutingTable::timer_manager_ = timer_manager;
    report_timer_ = std::make_shared<base::TimerRepeated>(timer_manager_, "MyRoutingTable");
}

bool MyRoutingTable::Init(const std::string& name_prefix, const std::string& supernode_ip, uint16_t supernode_port) {
    if (!RoutingTable::Init()) {
        TOP_FATAL_NAME("init routing table failed");
        return false;
    }

    name_ = name_prefix + name_;
    supernode_ip_ = supernode_ip;
    supernode_port_ = supernode_port;

    // report only for vpn service_type
    // TOP_FATAL("starting report timer ...");
    if (local_node_ptr_->GetXipParser().xnetwork_id() == kRoot) {
        auto cb = std::bind(&MyRoutingTable::SendReportNodes, this);
        report_timer_->Start(2000 * 1000, 2000 * 1000, cb);
        TOP_FATAL("start report timer");
    }

    // command entrance
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandSave, this, _1);
        BenchCommand::Instance()->RegisterCommand("save", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandLoad, this, _1);
        BenchCommand::Instance()->RegisterCommand("load", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandPrn, this, _1);
        BenchCommand::Instance()->RegisterCommand("prn", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandPrt, this, _1);
        BenchCommand::Instance()->RegisterCommand("prt", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandPrtSimple, this, _1);
        BenchCommand::Instance()->RegisterCommand("prt2", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandConn, this, _1);
        BenchCommand::Instance()->RegisterCommand("conn", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandConnVia, this, _1);
        BenchCommand::Instance()->RegisterCommand("connvia", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandConn2, this, _1);
        BenchCommand::Instance()->RegisterCommand("conn2", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandConn3, this, _1);
        BenchCommand::Instance()->RegisterCommand("conn3", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandQuery, this, _1);
        BenchCommand::Instance()->RegisterCommand("query", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandGetrt, this, _1);
        BenchCommand::Instance()->RegisterCommand("getrt", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandGetrt2, this, _1);
        BenchCommand::Instance()->RegisterCommand("getrt2", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandRtsize, this, _1);
        BenchCommand::Instance()->RegisterCommand("rtsize", cb);
    }
    {
        using namespace std::placeholders;
        auto cb = std::bind(&MyRoutingTable::OnCommandAutoTest, this, _1);
        BenchCommand::Instance()->RegisterCommand("autotest", cb);
    }

    return true;
}

void MyRoutingTable::OnCommandPrn(const BenchCommand::Arguments& args) {
    int node_count = 0;
    if (args.size() > 0) {
        node_count = std::stoi(args[0]);
    }
    StructPrn struct_prn;
    GetReportNodes(struct_prn);
    PrintReportNodes(struct_prn, node_count);
}

void MyRoutingTable::OnCommandSave(const BenchCommand::Arguments& args) {
    const std::string file_name = "all_nodes";
    pb::AllNodes pb_all_nodes;
    {
        std::unique_lock<std::mutex> lock(all_nodes_mutex_);
        for (auto& node: all_nodes_) {
            auto pb_node = pb_all_nodes.add_nodes();
            pb_node->set_local_ip(node->local_ip);
            pb_node->set_local_port(node->local_port);
            pb_node->set_public_ip(node->public_ip);
            pb_node->set_public_port(node->public_port);
            pb_node->set_service_type(node->service_type);
            pb_node->set_nat_type(node->nat_type);
            // pb_node->set_xid(node->xid);
            // pb_node->set_hash64(node->hash64);
            // pb_node->set_xip(node->xip);
            pb_node->set_node_id(node->node_id);
        }

        // local node
        {
            auto pb_node = pb_all_nodes.add_nodes();
            pb_node->set_local_ip(local_node_ptr_->local_ip());
            pb_node->set_local_port(local_node_ptr_->local_port());
            pb_node->set_public_ip(local_node_ptr_->public_ip());
            pb_node->set_public_port(local_node_ptr_->public_port());
            pb_node->set_service_type(local_node_ptr_->service_type());
            pb_node->set_nat_type(local_node_ptr_->nat_type());
            // pb_node->set_xid(node->xid());
            // pb_node->set_hash64(node->hash64);
            // pb_node->set_xip(node->xip);
            pb_node->set_node_id(local_node_ptr_->id());
        }
    }

    std::ofstream fout(file_name.c_str(), std::ios_base::out | std::ios_base::binary);
    const auto data = pb_all_nodes.SerializeAsString();
    fout.write(data.c_str(), data.size());
    fout.close();
    TOP_FATAL("save count: %d", (int)pb_all_nodes.nodes_size());
}

void MyRoutingTable::OnCommandLoad(const BenchCommand::Arguments& args) {
    const std::string file_name = "all_nodes";
    pb::AllNodes pb_all_nodes;

    std::string data;
    {
        std::ostringstream strout;
        std::ifstream fin(file_name.c_str(), std::ios_base::in | std::ios_base::binary);
        strout << fin.rdbuf();
        fin.close();
        data = strout.str();
        TOP_FATAL("data size: %d", (int)data.size());
    }

    if (!pb_all_nodes.ParseFromString(data)) {
        TOP_FATAL("parse failed");
        return;
    }

    {
        std::unique_lock<std::mutex> lock(all_nodes_mutex_);
        all_nodes_.clear();
        all_nodes_map_.clear();
        for (int i = 0; i < pb_all_nodes.nodes_size(); ++i) {
            auto pb_node = pb_all_nodes.nodes(i);
            if (pb_node.node_id() == local_node_ptr_->id()) {
                continue;  // skip local node
            }
            NodeInfoPtr node_ptr;
            node_ptr.reset(new NodeInfo(pb_node.node_id()));
            node_ptr->local_ip = pb_node.local_ip();
            node_ptr->local_port = pb_node.local_port();
            node_ptr->public_ip = pb_node.public_ip();
            node_ptr->public_port = pb_node.public_port();
            node_ptr->service_type = pb_node.service_type();
            node_ptr->nat_type = pb_node.nat_type();
            // node_ptr->xid = pb_node.xid();
            // node_ptr->hash64 = pb_node.hash64();
            // node_ptr->xip = pb_node.xip();
            SetNodeBucket(node_ptr);
            all_nodes_.push_back(node_ptr);
            all_nodes_map_[node_ptr->node_id] = node_ptr;
        }
        TOP_FATAL("load count: %d", (int)all_nodes_.size());
    }
}

void MyRoutingTable::OnCommandPrt(const BenchCommand::Arguments& args) {
    auto nodes = GetClosestNodes(local_node_ptr_->id(), kRoutingMaxNodesSize);
    auto nodes_info = GetDumpSelf() + GetDumpNodes(nodes, kRoutingMaxNodesSize);
    std::cout << nodes_info << std::endl;
}

void MyRoutingTable::OnCommandPrtSimple(const BenchCommand::Arguments& args) {
    auto nodes = GetClosestNodes(local_node_ptr_->id(), kRoutingMaxNodesSize);
    auto nodes_info = GetDumpSelf() + GetDumpNodesSimple(nodes, kRoutingMaxNodesSize);
    std::cout << nodes_info << std::endl;
}

void MyRoutingTable::SendReportNodes() {
    auto all_nodes = nodes();
    
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id("");
    message.set_type(kTestReportNodes);
    message.set_priority(enum_xpacket_priority_type_flash);

    pb::TestReportNodes msg;
    msg.set_nodes_size(all_nodes.size());
    message.set_data(msg.SerializeAsString());

    SetSpeClientMessage(message);
    SendData(message, supernode_ip_, supernode_port_); // send to super node
    // TOP_FATAL("report nodes");
}

void MyRoutingTable::HandleReportNodes(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    // TOP_FATAL("handle report nodes");
    if (local_node_ptr_->GetXipParser().xnetwork_id() != kRoot) {
        TOP_DEBUG_NAME("not root, can't process report nodes");
        return;
    }

    pb::TestReportNodes msg;
    if (!msg.ParseFromString(message.data())) {
        TOP_FATAL("parse data failed");
        return;
    }

    std::unique_lock<std::mutex> lock(report_mutex_);
    NodeReport node_report;
    node_report.count = msg.nodes_size();
    node_report.ep = packet.get_from_ip_addr() + ":" + std::to_string(packet.get_from_ip_port());
    std::string key = node_report.ep + " | " + HexSubstr(message.src_node_id());
    map_report_nodes_[key] = node_report;
}

void MyRoutingTable::PrintReportNodes(const StructPrn& struct_prn, int node_count) {
    if (node_count < 0) {
        TOP_FATAL("node_count(%d) is invalid", node_count);
        return;
    }

    TOP_FATAL("print report nodes(time=%ld):", struct_prn.time_now);
    int total = 0;
    int full = 0;
    int abnormal = 0;
    int n = 0;
    for (auto& kv : struct_prn.details) {
        total += 1;
        const auto count = kv.second.count;
        if (count == node_count) {
            full += 1;
        } else if (count > node_count) {
            abnormal += 1;
        } else {
            const int kMaxLine = 20;
            if (n < kMaxLine) {
                TOP_FATAL("  [%d] %s: %d", n, kv.first.c_str(), count);
                n += 1;
                if (n >= kMaxLine) {
                    TOP_FATAL("  [...] ...");
                }
            }
        }
    }
    TOP_FATAL("total = %d, full = %d, abnormal = %d", total, full, abnormal);
}

void MyRoutingTable::GetReportNodes(StructPrn& struct_prn) {
    // struct_prn.details.clear();
    std::unique_lock<std::mutex> lock(report_mutex_);
    struct_prn.time_now = ::time(NULL);
    // for (auto& kv : map_report_nodes_) {
    //     auto fmt = base::StringUtil::str_fmt("%s: %d", kv.first.c_str(), kv.second.count);
    //     struct_prn.details.push_back(fmt);
    // }
    struct_prn.details = map_report_nodes_;
}

void MyRoutingTable::SendConn2Request(bool send_or_relay) {
    std::vector<NodeInfoPtr> nodes;
    {
        std::unique_lock<std::mutex> lock(all_nodes_mutex_);
        nodes = all_nodes_;
    }

    for (int send_try = 0; send_try < kSendTry; ++send_try) {
        int n = 0;
        for (auto& node: nodes) {
            if (n % 100 == 0) {
                SleepMs(100);
            }
            n += 1;
            transport::protobuf::RoutingMessage message;
            SetFreqMessage(message);
            message.set_type(kTestConn2Request);
            message.set_des_service_type(node->service_type);
            message.set_des_node_id(node->node_id);
            pb::TestConn2Request req;
            req.set_tp1(GetSteadyTimepoint());
            req.set_send_or_relay(send_or_relay);
            req.set_src_ip(local_node_ptr_->public_ip());
            req.set_src_port(local_node_ptr_->public_port());
            message.set_data(req.SerializeAsString());
            SendToClosestNode(message);
        }
    }
    TOP_FATAL("send test conn2");
}

void MyRoutingTable::ClearQuery() {
    std::vector<NodeInfoPtr> nodes;
    {
        std::unique_lock<std::mutex> lock(all_nodes_mutex_);
        nodes = all_nodes_;
    }
    
    {
        std::unique_lock<std::mutex> lock(test_conn_mutex_);
        search_map_.clear();
        hops_map_.clear();
        for (auto& node: nodes) {
            search_map_[node->node_id].found = false;
            search_map_[node->node_id].node = node;
            // TOP_FATAL("testing node(%s)", HexSubstr(node->node_id).c_str());
        }
    }
}

void MyRoutingTable::OnCommandConn(const BenchCommand::Arguments& args) {
    TOP_FATAL("on command conn ...");
    if (args.empty()) {
        TOP_FATAL("need one arg");
        return;
    }

    auto node = GetNode(args[0]);
    if (!node) {
        TOP_FATAL("node must in all nodes map");
        return;
    }

    RelayConnRequest(node->node_id);
    TOP_FATAL("start test conn");
}

void MyRoutingTable::OnCommandConnVia(const BenchCommand::Arguments& args) {
    TOP_FATAL("on command connvia ...");
    if (args.size() < 2) {
        TOP_FATAL("need 2 args");
        return;
    }

    auto des_node = GetNode(args[0]);
    auto via_node = GetNode(args[1]);
    if (!des_node || !via_node) {
        TOP_FATAL("both nodes must in all nodes map");
        return;
    }

    SendConnVia(via_node, des_node);
    TOP_FATAL("start test connvia");
}

void MyRoutingTable::OnCommandConn3(const BenchCommand::Arguments& args) {
    TOP_FATAL("on command conn3 ...");
    OnCommandConn2(args);
    SleepWait(5);
    OnCommandQuery(args);
}

void MyRoutingTable::OnCommandConn2(const BenchCommand::Arguments& args) {
    TOP_FATAL("on command conn2 ...");
    ClearQuery();
    SendConn2Request(false);  // relay
    // SendConn2Request(true);  // send
    TOP_FATAL("start test conn2");
}

void MyRoutingTable::OnCommandQuery(const BenchCommand::Arguments& args) {
    StructQuery struct_query;
    GetQuery(struct_query);
    DumpQuery(struct_query);
}

void MyRoutingTable::GetQuery(StructQuery& struct_query) {
    struct_query.hops.clear();
    struct_query.not_found_nodes.clear();
    std::unique_lock<std::mutex> lock(test_conn_mutex_);
    
    for (auto& kv : hops_map_) {
        long long avg = -1;
        if (kv.second.nodes.size() > 0) {
            avg = kv.second.total / kv.second.nodes.size();
        }
        auto fmt = base::StringUtil::str_fmt("%4d %5d ------- %3d %3d %3d", kv.first, (int)kv.second.nodes.size(),
                (int)avg, (int)kv.second.max, (int)kv.second.min);
        struct_query.hops.push_back(fmt);
    }

    for (auto& kv : search_map_) {
        if (!kv.second.found) {
            struct_query.not_found_nodes.push_back(kv.second.node);
        }
    }
}

void MyRoutingTable::DumpQuery(const StructQuery& struct_query) {
    TOP_FATAL("hops count rrt(ms) avg max min:");
    for (auto& hop : struct_query.hops) {
        TOP_FATAL("%s", hop.c_str());
    }
    {
        TOP_FATAL("not found(count=%ld):", (long)struct_query.not_found_nodes.size());
        auto nodes_info = GetDumpNodes(struct_query.not_found_nodes, 10);
        std::cout << nodes_info << std::endl;
    }

    std::vector<NodeInfoPtr> continuous_not_found;
    decltype(last_not_found_map_) current_not_found_map;
    for (auto& node : struct_query.not_found_nodes) {
        current_not_found_map[node->node_id] = node;
        if (last_not_found_map_.find(node->node_id) != last_not_found_map_.end()) {  // not found last and current
            continuous_not_found.push_back(node);
        }
    }
    last_not_found_map_ = current_not_found_map;
    {
        TOP_FATAL("continuous not found(count=%ld):", (long)continuous_not_found.size());
        auto nodes_info = GetDumpNodes(continuous_not_found, 10);
        std::cout << nodes_info << std::endl;
    }
}

void MyRoutingTable::SendGetrtRequest(const std::string& ip, uint16_t port) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id("");
    message.set_type(kTestGetrtRequest);
    message.set_priority(enum_xpacket_priority_type_flash);

    SetSpeClientMessage(message);
    SendData(message, ip, port);
}

void MyRoutingTable::SendGetrt2Request(const std::string& ip, uint16_t port) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id("");
    message.set_type(kTestGetrt2Request);
    message.set_priority(enum_xpacket_priority_type_flash);

    SetSpeClientMessage(message);
    SendData(message, ip, port);
}

void MyRoutingTable::OnCommandGetrt(const BenchCommand::Arguments& args) {
    if (args.empty()) {
        return;
    }

    base::LineParser line_parser(args[0].c_str(), ':');
    if (line_parser.Count() != 2) {
        TOP_FATAL("ip:port parse failed");
        return;
    }

    const std::string peer_ip = line_parser[0];
    int peer_port = 0;
    try {
        peer_port = std::stoi(line_parser[1]);
    } catch (...) {
        TOP_FATAL("ip:port parse failed");
        return;
    }

    SendGetrtRequest(peer_ip, peer_port);
}

void MyRoutingTable::ClearRtdiff() {
    {
        std::unique_lock<std::mutex> lock(rt_map_mutex_);
        rt_diff_count_ = 0;
        rt_size_map_.clear();
    }
}

void MyRoutingTable::OnCommandGetrt2(const BenchCommand::Arguments& args) {
    TOP_FATAL("on command getrt2 ...");
    ClearRtdiff();

    std::vector<NodeInfoPtr> nodes;
    {
        std::unique_lock<std::mutex> lock(all_nodes_mutex_);
        nodes = all_nodes_;
    }

    for (int send_try = 0; send_try < kSendTry; ++send_try) {
        int n = 0;
        for (auto& node : nodes) {
            if (n % 50 == 0) {
                SleepMs(50);
            }
            n += 1;
            SendGetrt2Request(node->public_ip, node->public_port);
        }
    }
}

void MyRoutingTable::OnCommandRtsize(const BenchCommand::Arguments& args) {
    std::unique_lock<std::mutex> lock(rt_map_mutex_);

    if (args.size() >= 1) {
        int rt_size = ParseArg(args[0], 1);
        std::vector<NodeInfoPtr> nodes;
        for (auto& kv : rt_map_) {
            if (GetLines(kv.second) == rt_size) {
                std::unique_lock<std::mutex> lock2(all_nodes_mutex_);
                auto it = all_nodes_map_.find(kv.first);
                if (it == all_nodes_map_.end()) {
                    continue;
                }
                nodes.push_back(it->second);
            }
        }

        TOP_FATAL("last rt_map_ which rt_size = %d:", rt_size);
        auto nodes_info = GetDumpNodes(nodes, 0);
        std::cout << nodes_info << std::endl;
        return;
    }

    TOP_FATAL("last rt_size_map:");
    auto fmt = base::StringUtil::str_fmt("%7s %5s", "rt_size", "count");
    std::cout << fmt << std::endl;
    for (auto& kv : rt_size_map_) {
        auto fmt = base::StringUtil::str_fmt("%7d %5d", kv.first, kv.second);
        std::cout << fmt << std::endl;
    }
    TOP_FATAL("last rt_diff_count: %d", rt_diff_count_);
}

int MyRoutingTable::ParseArg(const std::string& arg, int default_value) {
    int value = default_value;
    try {
        value = std::stoi(arg);
    } catch (...) {
        TOP_FATAL("invalid args");
    }

    return value;
}

void MyRoutingTable::OnCommandAutoTest(const BenchCommand::Arguments& args) {
    if (args.size() < 1) {
        TOP_FATAL("args: node_count");
        return;
    }

    int node_count = 0;
    // int rt_size = kRoutingMaxNodesSize;
    try {
        node_count = std::stoi(args[0]);
        if (node_count < 0) {
            TOP_FATAL("invalid node_count");
            return;
        }
    } catch (...) {
        TOP_FATAL("invalid args");
        return;
    }

    AutoTestAllBoot(node_count);
    TOP_FATAL("all boot ok, time point: %ld", (long)time_all_boot_);

    // AutoTestAllFull(rt_size);
    // TOP_FATAL("all full ok, time point: %ld", (long)time_all_full_);
    // TOP_FATAL("full time: %ld", time_all_full_ - time_all_boot_);

    AutoTestAllGetrt2();
    TOP_FATAL("all getrt2 ok, time point: %ld", (long)time_all_getrt2_);
    TOP_FATAL("getrt2 time: %ld", time_all_getrt2_ - time_all_boot_);

    AutoTestAllConn();
    TOP_FATAL("all conn ok, time point: %ld", (long)time_all_conn_);
    TOP_FATAL("conn time: %ld", time_all_conn_ - time_all_boot_);
}

bool MyRoutingTable::AutoTestAllBoot(int node_count) {
    TOP_FATAL("auto test all boot ...");
    StructPrn struct_prn;
    for (;;) {
        GetReportNodes(struct_prn);
        if ((int)struct_prn.details.size() >= node_count) {
            time_all_boot_ = ::time(NULL);
            break;
        }

        SleepWait(3);
    }

    return true;
}

bool MyRoutingTable::AutoTestAllFull(int rt_size) {
    TOP_FATAL("auto test all full ...");
    StructPrn struct_prn;
    for (;;) {
        int sum = 0;
        GetReportNodes(struct_prn);
        for (auto& kv : struct_prn.details) {
            if (kv.second.count >= rt_size) {
                sum += 1;
            }
        }
        if (sum >= rt_size) {
            time_all_full_ = ::time(NULL);
            break;
        }

        SleepWait(3);
    }

    return true;
}

bool MyRoutingTable::AutoTestAllConn() {
    TOP_FATAL("auto test all conn ...");
    const BenchCommand::Arguments args;
    StructQuery struct_query;
    int success_count = 0;
    const int max_continuous_success_count = 3;
    for (;;) {
        OnCommandConn2(args);
        SleepWait(5);  // wait for sending response back
        GetQuery(struct_query);
        DumpQuery(struct_query);

        if (struct_query.not_found_nodes.empty()) {
            TOP_FATAL("success");
            success_count += 1;
            if (success_count == 1) {
                time_all_conn_ = ::time(NULL);
            }
            if (success_count >= max_continuous_success_count) {
                break;
            }
        } else {
            TOP_FATAL("failed");
            success_count = 0;
        }

        SleepWait(5);
    }

    return true;
}

bool MyRoutingTable::AutoTestAllGetrt2() {
    TOP_FATAL("auto test all getrt2 ...");
    int diff_count = 0;
    // GetGetrt2(diff_count);  // ignore first call

    int success_count = 0;
    const int max_continuous_success_count = 3;
    for (;;) {
        GetGetrt2(diff_count);
        if (diff_count == 0) {
            TOP_FATAL("success");
            success_count += 1;
            if (success_count == 1) {
                time_all_getrt2_ = ::time(NULL);
            }
            if (success_count >= max_continuous_success_count) {
                break;
            }
        } else {
            TOP_FATAL("failed");
            success_count = 0;
        }

        SleepWait(5);
    }

    return true;
}

void MyRoutingTable::SleepWait(int seconds) {
    for (int i = 0; i < seconds; ++i) {
        std::cout << ".";
        fflush(stdout);
        sleep(1);
    }
    std::cout << std::endl;
}

void MyRoutingTable::HandleGetrtRequest(
        transport::protobuf::RoutingMessage& req_message,
        base::xpacket_t& packet) {
    auto nodes = GetClosestNodes(local_node_ptr_->id(), kRoutingMaxNodesSize);
    auto nodes_info = GetDumpSelf() + GetDumpNodes(nodes, kRoutingMaxNodesSize);

    {
        transport::protobuf::RoutingMessage message;
        SetFreqMessage(message);
        message.set_des_service_type(local_node_ptr_->service_type());
        message.set_des_node_id("");
        message.set_type(kTestGetrtResponse);
        message.set_priority(enum_xpacket_priority_type_flash);
        message.set_data(nodes_info);

        SetSpeClientMessage(message);
        SendData(message, packet.get_from_ip_addr(), packet.get_from_ip_port()); // send to src node
    }
}

void MyRoutingTable::HandleGetrt2Request(
        transport::protobuf::RoutingMessage& req_message,
        base::xpacket_t& packet) {
    auto nodes = GetClosestNodes(local_node_ptr_->id(), kRoutingMaxNodesSize);
    auto nodes_info = GetDumpSelf() + GetDumpNodesSimple(nodes, kRoutingMaxNodesSize);

    {
        transport::protobuf::RoutingMessage message;
        SetFreqMessage(message);
        message.set_des_service_type(local_node_ptr_->service_type());
        message.set_des_node_id("");
        message.set_type(kTestGetrt2Response);
        message.set_priority(enum_xpacket_priority_type_flash);
        message.set_data(nodes_info);

        SetSpeClientMessage(message);
        SendData(message, packet.get_from_ip_addr(), packet.get_from_ip_port()); // send to src node
    }
}

int MyRoutingTable::GetLines(const std::string& str) {
    int sum = 0;
    std::istringstream strin(str);
    std::string buf;
    for (;;) {
        if (!std::getline(strin, buf)) {
            break;
        }

        sum += 1;
    }

    return sum;
}

void MyRoutingTable::HandleGetrtResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_FATAL("getrt response:");
    std::cout << message.data() << std::endl;
}

void MyRoutingTable::HandleGetrt2Response(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    std::unique_lock<std::mutex> lock(rt_map_mutex_);
    const auto node_id = message.src_node_id();
    rt_size_map_[GetLines(message.data())] += 1;
    // if (rt_map_[node_id].empty()) {  // first update
    //     rt_map_[node_id] = message.data();
    // } else {
        if (rt_map_[node_id] != message.data()) {
            const int max_diff_head = 20;
            const int max_diff_body = 1;
            assert(max_diff_head >= max_diff_body);
            if (rt_diff_count_ < max_diff_head) {
                auto head = base::StringUtil::str_fmt("%4d] update %s", rt_diff_count_, HexSubstr(node_id).c_str());
                std::cout << head << std::endl;
            }
            if (rt_diff_count_ < max_diff_body) {
                // opt: just dump diff lines!
                auto body = CompareDiffLines(rt_map_[node_id], message.data());
                std::cout << body << std::endl;
            }

            rt_diff_count_ += 1;
            rt_map_[node_id] = message.data();
        }
    // }
}

std::string MyRoutingTable::CompareDiffLines(const std::string& str1, const std::string& str2) {
    std::string ret;
    std::istringstream strin1(str1);
    std::istringstream strin2(str2);
    std::string line1, line2;

    // ignore the first line
    std::getline(strin1, line1);
    std::getline(strin2, line2);
    if (line1.empty() && line2.empty()) {
        return "both empty";
    }

    // the same head line
    if (!line1.empty()) {
        ret = line1 + "\n";
    } else {
        ret = line2 + "\n";
    }
    while (std::getline(strin1, line1)) {
        std::getline(strin2, line2);
        ret += base::StringUtil::str_fmt("%30s %6s %30s\n",
                line1.c_str(),
                line1 != line2 ? "vs" : "",
                line2.c_str());
    }

    while (std::getline(strin2, line2)) {
        ret += base::StringUtil::str_fmt("%30s %6s %30s\n",
                "",
                "vs",
                line2.c_str());
    }

    return ret;
}

void MyRoutingTable::GetGetrt2(int& diff_count) {
    ClearRtdiff();
    const BenchCommand::Arguments args;
    OnCommandGetrt2(args);

    SleepWait(5);
    std::unique_lock<std::mutex> lock(rt_map_mutex_);
    diff_count = rt_diff_count_;
}

void MyRoutingTable::SendConn2Response(const std::string& data, const std::string& des_node_id, const std::string& ip, uint16_t port) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id(des_node_id);
    message.set_type(kTestConn2Response);
    message.set_priority(enum_xpacket_priority_type_flash);
    message.set_data(data);

    SetSpeClientMessage(message);
    SendData(message, ip, port);
}

void MyRoutingTable::SendConnReply(const std::string& reply, const std::string& ip, uint16_t port) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id("");
    message.set_type(kTestConnReply);
    message.set_priority(enum_xpacket_priority_type_flash);
    message.set_data(reply);

    SetSpeClientMessage(message);
    SendData(message, ip, port);
}

void MyRoutingTable::SendConnVia(NodeInfoPtr via_node, NodeInfoPtr des_node) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_type(kTestConnRequest);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id(des_node->node_id);

    pb::TestConnRequest req;
    req.set_tp1(GetSteadyTimepoint());
    req.set_src_ip(local_node_ptr_->public_ip());
    req.set_src_port(local_node_ptr_->public_port());
    message.set_data(req.SerializeAsString());

    SetSpeClientMessage(message);
    SendData(message, via_node->public_ip, via_node->public_port);
}

NodeInfoPtr MyRoutingTable::GetNode(const std::string& node_id) {
    std::unique_lock<std::mutex> lock(all_nodes_mutex_);
    for (auto& node : all_nodes_) {
        if (HexSubstr(node->node_id) == node_id) {
            return node;
        }
    }

    return nullptr;
}

void MyRoutingTable::RelayConnRequest(const std::string& node_id) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_type(kTestConnRequest);
    message.set_des_service_type(local_node_ptr_->service_type());
    message.set_des_node_id(node_id);

    pb::TestConnRequest req;
    req.set_tp1(GetSteadyTimepoint());
    req.set_src_ip(local_node_ptr_->public_ip());
    req.set_src_port(local_node_ptr_->public_port());
    message.set_data(req.SerializeAsString());
    SendToClosestNode(message);
    TOP_FATAL("send test conn");
}

void MyRoutingTable::RelayConnResponse(const std::string& data, const uint64_t des_service_type, const std::string& des_node_id) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_type(kTestConnResponse);
    message.set_des_service_type(des_service_type);
    message.set_des_node_id(des_node_id);
    message.set_data(data);
    SendToClosestNode(message);
}

void MyRoutingTable::RelayConn2Response(const std::string& data, const uint64_t des_service_type, const std::string& des_node_id) {
    transport::protobuf::RoutingMessage message;
    SetFreqMessage(message);
    message.set_type(kTestConn2Response);
    message.set_des_service_type(des_service_type);
    message.set_des_node_id(des_node_id);
    message.set_data(data);
    SendToClosestNode(message);
}

void MyRoutingTable::HandleConn2Request(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    pb::TestConn2Request req;
    if (!req.ParseFromString(message.data())) {
        TOP_FATAL("parse failed");
        return;
    }

    // relay
    if (!IsDestination(message.des_node_id(), true)) {
        TOP_DEBUG_NAME("relay the message");
        SendToClosestNode(message);
        return;
    }

    // can't find
    if (message.des_node_id() != local_node_ptr_->id()) {
        TOP_DEBUG_NAME("cant find node");
        return;
    }

    // find the des node
    TOP_DEBUG_NAME("find the des node");
    pb::TestConn2Response res;
    res.set_tp1(req.tp1());
    res.set_hops1(message.hop_num());
    res.set_send_or_relay(req.send_or_relay());
    if (req.send_or_relay()) {
        SendConn2Response(res.SerializeAsString(), message.src_node_id(), req.src_ip(), req.src_port());
    } else {
        RelayConn2Response(res.SerializeAsString(), message.src_service_type(), message.src_node_id());
    }
}

void MyRoutingTable::HandleConn2Response(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    pb::TestConn2Response res;
    if (!res.ParseFromString(message.data())) {
        TOP_FATAL("parse failed");
        return;
    }

    std::unique_lock<std::mutex> lock(test_conn_mutex_);
    if (res.send_or_relay()) {  // TODO: ???
        message.set_des_node_id(local_node_ptr_->id());
    }

    // relay
    if (!IsDestination(message.des_node_id(), true)) {
        TOP_DEBUG_NAME("relay the message");
        SendToClosestNode(message);
        return;
    }

    // can't find
    if (message.des_node_id() != local_node_ptr_->id()) {
        TOP_DEBUG_NAME("cant find node");
        return;
    }

    // find the des node
    int hops1 = res.hops1();
    int hops = message.hop_num() + hops1;
    auto tp1 = res.tp1();
    auto td = GetSteadyTimepoint() - tp1;
    auto& node_search = search_map_[message.src_node_id()];
    if (td >= 0) {
        node_search.found = true;  // mark found
        auto& node_hops = hops_map_[hops];
        node_hops.total += td;
        node_hops.max = std::max(td, node_hops.max);
        node_hops.min = std::min(td, node_hops.min);
        node_hops.nodes.push_back(node_search.node);
    }
    // TOP_FATAL("find node(%s)", HexSubstr(node_musk.second->node_id).c_str());
}

void MyRoutingTable::HandleConnRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    pb::TestConnRequest req;
    if (!req.ParseFromString(message.data())) {
        TOP_FATAL("parse failed");
        return;
    }

    // relay
    if (!IsDestination(message.des_node_id(), true)) {
        const std::string msg = "[conn_request] relay the message";
        TOP_DEBUG_NAME("%s", msg.c_str());
        std::string reply = base::StringUtil::str_fmt("hops(%d) %s %s",
                message.hop_num(), HexSubstr(local_node_ptr_->id()).c_str(), msg.c_str());
        SendConnReply(reply, req.src_ip(), req.src_port());
        SendToClosestNode(message);
        return;
    }

    // can't find
    if (message.des_node_id() != local_node_ptr_->id()) {
        const std::string msg = "[conn_request] cant find node";
        TOP_DEBUG_NAME("%s", msg.c_str());
        std::string reply = base::StringUtil::str_fmt("hops(%d) %s %s",
                message.hop_num(), HexSubstr(local_node_ptr_->id()).c_str(), msg.c_str());
        SendConnReply(reply, req.src_ip(), req.src_port());
        return;
    }

    // find the des node
    const std::string msg = "[conn_request] find the des node";
    TOP_DEBUG_NAME("%s", msg.c_str());
    std::string reply = base::StringUtil::str_fmt("hops(%d) %s %s",
                message.hop_num(), HexSubstr(local_node_ptr_->id()).c_str(), msg.c_str());
    SendConnReply(reply, req.src_ip(), req.src_port());

    pb::TestConnResponse res;
    res.set_tp1(req.tp1());
    res.set_hops1(message.hop_num());
    res.set_src_ip(req.src_ip());
    res.set_src_port(req.src_port());
    RelayConnResponse(res.SerializeAsString(), message.src_service_type(), message.src_node_id());
}

void MyRoutingTable::HandleConnResponse(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    pb::TestConnResponse res;
    if (!res.ParseFromString(message.data())) {
        TOP_FATAL("parse failed");
        return;
    }

    // relay
    if (!IsDestination(message.des_node_id(), true)) {
        const std::string msg = "[conn_response] relay the message";
        TOP_DEBUG_NAME("%s", msg.c_str());
        std::string reply = base::StringUtil::str_fmt("hops(%d) %s %s",
                message.hop_num(), HexSubstr(local_node_ptr_->id()).c_str(), msg.c_str());
        SendConnReply(reply, res.src_ip(), res.src_port());
        SendToClosestNode(message);
        return;
    }

    // can't find
    if (message.des_node_id() != local_node_ptr_->id()) {
        const std::string msg = "[conn_response] cant find node";
        TOP_DEBUG_NAME("%s", msg.c_str());
        std::string reply = base::StringUtil::str_fmt("hops(%d) %s %s",
                message.hop_num(), HexSubstr(local_node_ptr_->id()).c_str(), msg.c_str());
        SendConnReply(reply, res.src_ip(), res.src_port());
        return;
    }

    // find the des node
    const std::string msg = "[conn_response] find the des node";
    TOP_DEBUG_NAME("%s", msg.c_str());
    std::string reply = base::StringUtil::str_fmt("hops(%d) %s %s",
                message.hop_num(), HexSubstr(local_node_ptr_->id()).c_str(), msg.c_str());
    TOP_FATAL("%s", reply.c_str());

    int hops1 = res.hops1();
    int hops = message.hop_num() + hops1;
    auto tp1 = res.tp1();
    auto td = GetSteadyTimepoint() - tp1;
    TOP_FATAL("total cost %lld ms, %d hops", td, hops);
}

void MyRoutingTable::HandleConnReply(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_FATAL("%s from %s:%d", message.data().c_str(), packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
}

std::string MyRoutingTable::GetDumpNodes(std::vector<NodeInfoPtr> nodes, int max_detail) {
    if (max_detail < 1) {
        max_detail = 9999;
    }

    std::string ret;
    int n = 0;
    int zip_node_count = 0;
    for (auto& node : nodes) {
        if (n < max_detail) {
            ret += base::StringUtil::str_fmt("%4d] %s, dis(%3d), pub(%15s:%5d), local(%15s:%5d), nat(%d)\n",
                    n, HexSubstr(node->node_id).c_str(), node->bucket_index,
                    node->public_ip.c_str(), node->public_port,
                    node->local_ip.c_str(), node->local_port,
                    node->nat_type);
        } else {
            zip_node_count += 1;
        }

        n += 1;
    }

    if (zip_node_count > 0) {
        ret += base::StringUtil::str_fmt("[...zip_node_count(%d)...] ...\n", zip_node_count);
    }

    return ret;
}

std::string MyRoutingTable::GetDumpNodesSimple(std::vector<NodeInfoPtr> nodes, int max_detail) {
    if (max_detail < 1) {
        max_detail = 9999;
    }

    std::string ret;
    int n = 0;
    int zip_node_count = 0;
    for (auto& node : nodes) {
        if (n < max_detail) {
            ret += base::StringUtil::str_fmt("%4d] %s, dis(%3d)\n",
                    n, HexSubstr(node->node_id).c_str(), node->bucket_index);
        } else {
            zip_node_count += 1;
        }

        n += 1;
    }

    if (zip_node_count > 0) {
        ret += base::StringUtil::str_fmt("[...zip_node_count(%d)...] ...\n", zip_node_count);
    }

    return ret;
}

std::string MyRoutingTable::GetDumpSelf() {
    return base::StringUtil::str_fmt("self] %s, dis(  0), pub(%15s:%5d), local(%15s:%5d), nat(1)\n",
            HexSubstr(local_node_ptr_->id()).c_str(),
            local_node_ptr_->public_ip().c_str(), local_node_ptr_->public_port(),
            local_node_ptr_->local_ip().c_str(), local_node_ptr_->local_port());
}

long long MyRoutingTable::GetSteadyTimepoint() {
    auto td = std::chrono::steady_clock::now().time_since_epoch();
    auto td2 = std::chrono::duration_cast<std::chrono::milliseconds>(td);
    return td2.count();
}

void MyRoutingTable::HandleBootstrapJoinRequest(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    TOP_DEBUG_NAME("handle boot req from %s:%d", packet.get_from_ip_addr().c_str(), packet.get_from_ip_port());
    RoutingTable::HandleBootstrapJoinRequest(message, packet);

    protobuf::BootstrapJoinRequest join_req;
    if (!join_req.ParseFromString(message.data())) {
        TOP_DEBUG_NAME("parse boot req failed");
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
    SetNodeBucket(node_ptr);

    if (local_node_ptr_->first_node()) {
        std::unique_lock<std::mutex> lock(all_nodes_mutex_);
        auto& old_node = all_nodes_map_[node_ptr->node_id];
        if (old_node) {
            if (old_node->public_ip == node_ptr->public_ip && old_node->public_port == node_ptr->public_port) {
                // maybe cause by rejoin!
                TOP_FATAL("node rejoin node_id(%s)", HexSubstr(node_ptr->node_id).c_str());
            } else {
                TOP_FATAL("duplicate node_id(%s)", HexSubstr(node_ptr->node_id).c_str());
                // assert(0);
            }
        } else {
            old_node = node_ptr;
            all_nodes_.push_back(node_ptr);
        }
    }
}

// --------------------------------------------------------------------------------
void MyMessageHandler::Init() {
    KadMessageHandler::Init();

    message_manager_->RegisterMessageProcessor(kTestReportNodes, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleReportNodes(message, packet);
    });

    message_manager_->RegisterMessageProcessor(kTestConnRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleConnRequest(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kTestConnResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleConnResponse(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kTestConnReply, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleConnReply(message, packet);
    });

    message_manager_->RegisterMessageProcessor(kTestConn2Request, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleConn2Request(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kTestConn2Response, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleConn2Response(message, packet);
    });

    message_manager_->RegisterMessageProcessor(kTestGetrtRequest, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleGetrtRequest(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kTestGetrtResponse, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleGetrtResponse(message, packet);
    });

    message_manager_->RegisterMessageProcessor(kTestGetrt2Request, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleGetrt2Request(message, packet);
    });
    message_manager_->RegisterMessageProcessor(kTestGetrt2Response, [this](
            transport::protobuf::RoutingMessage& message,
            base::xpacket_t& packet){
        auto ptr = std::dynamic_pointer_cast<MyRoutingTable>(routing_ptr_);
        assert(ptr);
        ptr->HandleGetrt2Response(message, packet);
    });

}

// --------------------------------------------------------------------------------

bool MyNodeMgr::Init(const base::Config& config, const std::string& name) {
    timer_manager_impl_ = base::TimerManager::CreateInstance();
    timer_manager_impl_->Start(1);

    if (!LoadConfig(config)) {
        TOP_FATAL("load config failed");
        return false;
    }

    // basic config
    const bool client_mode = false;
    const std::string idtype = "";
    const std::string str_key = RandomString(kNodeIdSize);
    const bool hash_tag = true;
    // auto kad_key = std::make_shared<base::PlatformKadmliaKey>(str_key, hash_tag);
    auto kad_key = std::make_shared<MyKadkey>(str_key, hash_tag);

    // register message handler
    message_handler_.message_manager_ = &this->message_manager_;
    // message_handler_.nat_manager_ = this->nat_manager_;
    message_handler_.Init();

    // init message process threads
    message_threads_ = std::make_shared<transport::MultiThreadHandler>();
    message_threads_->m_woker_threads_count = 1;
    message_threads_->Init();
    for (auto& thread : message_threads_->m_worker_threads) {
        thread->message_manager_ = &this->message_manager_;
    }

    // init udp socket
    {
        udp_transport_.reset(new top::transport::UdpTransport());
        auto ret = udp_transport_->Start(local_ip_, local_port_, message_threads_.get());
        if (ret != top::kadmlia::kKadSuccess) {
            TOP_FATAL("udp_transport start failed!");
            return false;
        }
        real_local_port_ = udp_transport_->local_port();
        TOP_FATAL("real local port: %d", (int)real_local_port_);
    }

    // init nat
    // if (!NatDetect()) {
    //     TOP_FATAL("nat detect failed");
    //     return false;
    // }

    // init local node info
    local_node_info_ = std::make_shared<LocalNodeInfo>();
    // local_node_info_->nat_manager_ = this->nat_manager_;
    if (!local_node_info_->Init(
            local_ip_,
            local_port_,
            first_node_,
            client_mode,
            idtype,
            kad_key,
            kad_key->xnetwork_id(),
            kRoleInvalid)) {
        TOP_FATAL("local_node_info init failed!");
        return false;
    }
    local_node_info_->set_service_type(kRoot);

    // init routing table
    routing_table_ = std::make_shared<MyRoutingTable>(
            timer_manager_impl_.get(),
            udp_transport_,
            kNodeIdSize,
            local_node_info_);

    if (!routing_table_->Init(name, supernode_ip_, supernode_port_)) {
        TOP_FATAL("routing_table init failed!");
        return false;
    }

    message_handler_.set_routing_ptr(routing_table_);
    TOP_FATAL("init success");
    return true;
}

bool MyNodeMgr::LoadConfig(const base::Config& config) {
    if (!config.Get("node", "first_node", first_node_)) {
        TOP_FATAL("load first_node failed");
        return false;
    }

    if (!config.Get("node", "local_ip", local_ip_)) {
        TOP_FATAL("load local_ip failed");
        return false;
    }

    if (!config.Get("node", "local_port", local_port_)) {
        TOP_FATAL("load local_port failed");
        return false;
    }

    std::string peer;
    if (!config.Get("node", "public_endpoints", peer)) {
        TOP_FATAL("load public_endpoints failed");
        return false;
    }
    std::set<std::pair<std::string, uint16_t>> set_endpoints;
    base::ParseEndpoints(peer, set_endpoints);
    assert(!set_endpoints.empty());
    supernode_ip_ = set_endpoints.begin()->first;
    supernode_port_ = set_endpoints.begin()->second;

    return true;
}

MyNodeMgr::~MyNodeMgr() {
    if (nat_transport_) {
        nat_transport_->Stop();
    }

    if (nat_manager_) {
        nat_manager_->Stop();
    }

    if (udp_transport_) {
        udp_transport_->Stop();
    }

    if (message_threads_) {
        message_threads_->Stop();
    }

    if (routing_table_) {
        routing_table_->UnInit();
    }
}

bool MyNodeMgr::NatDetect() {
    nat_transport_.reset(new top::transport::UdpTransport());
    auto ret = nat_transport_->Start(local_ip_, 0, message_threads_.get());
    if (ret != top::kadmlia::kKadSuccess) {
        TOP_FATAL("nat_transport start failed!");
        return false;
    }

    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    boot_endpoints.insert({supernode_ip_, supernode_port_});
    transport::MultiThreadHandler* messager_handler = nullptr;
    // nat_manager_ = std::make_shared<NatManager>();
    nat_manager_ = NatManagerIntf::Instance();
    auto nat_manager = dynamic_cast<NatManager*>(nat_manager_);
    nat_manager->timer_manager_ = timer_manager_impl_.get();
    if (!nat_manager_->Start(
            first_node_,
            boot_endpoints,
            messager_handler,
            udp_transport_.get(),
            nat_transport_.get())) {
        TOP_FATAL("nat detect failed");
        return false;
    }

    TOP_FATAL("nat detect ok");
    return true;
}

bool MyNodeMgr::JoinRt() {
    std::set<std::pair<std::string, uint16_t>> boot_endpoints;
    boot_endpoints.insert({supernode_ip_, supernode_port_});
    int ret = routing_table_->MultiJoin(boot_endpoints);
    if (ret != kKadSuccess) {
        TOP_FATAL("join failed");
        return false;
    }

    TOP_FATAL("join ok");
    return true;
}

uint16_t MyNodeMgr::RealLocalPort() {
    return real_local_port_;
}

void MyNodeMgr::HandleMessage(
        transport::protobuf::RoutingMessage& message,
        base::xpacket_t& packet) {
    message_manager_.HandleMessage(message, packet);
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
