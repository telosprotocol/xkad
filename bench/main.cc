// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <assert.h>
#include "util.h"
#include "node.h"
#include "xpbase/base/kad_key/get_kadmlia_key.h"
#include "xkad/routing_table/routing_utils.h"
#include "xpbase/base/endpoint_util.h"

using namespace top::kadmlia::test;

namespace top {
    uint32_t gloabl_platform_type = kPlatform;
    std::shared_ptr<top::base::KadmliaKey> global_xid;
    std::string global_node_id = RandomString(kNodeIdSize);
    std::string global_node_id_hash("");
}

int main(int argc, char* argv[]) {
    // load argv
    base::Config config;
    {
        // default config as first node
        config.Set("node", "first_node", true);
        config.Set("node", "show_cmd", true);
        config.Set("node", "local_ip", "127.0.0.1");
        config.Set("node", "local_port", 0);
        config.Set("node", "public_endpoints", "127.0.0.1:0");
        config.Set("node", "log_path", "./xkad_bench.log");
        config.Set("node", "log_level", enum_xlog_level_debug);  // debug
        config.Set("node", "zone_id", 1);  // for global xid
        if (!Util::Instance()->HandleParamsAndConfig(argc, argv, config)) {
            TOP_FATAL("handle argv failed");
            return 1;
        }
    }

    // init log
    {
        std::string log_path;
        if (!config.Get("node", "log_path", log_path)) {
            assert(0);
        }
        int log_level = enum_xlog_level_debug;
        if (!config.Get("node", "log_level", log_level)) {
            assert(0);
        }
        xinit_log(log_path.c_str(), true, true);
        xset_log_level((enum_xlog_level)log_level);
    }

    // init global xid
    if (!top::kadmlia::CreateGlobalXid(config)) {
        assert(0);
    }

    bool first_node = false;
    if (!config.Get("node", "first_node", first_node)) {
        assert(0);
    }

    bool show_cmd = false;
    if (!config.Get("node", "show_cmd", show_cmd)) {
        assert(0);
    }
    BenchCommand::Instance()->Init(show_cmd);
    MyNodeMgr node_mgr;

    if (first_node) {
        if (!node_mgr.Init(config, "1")) {
            TOP_FATAL("node_mgr init failed");
            return 1;
        }
    } else {
        if (!node_mgr.Init(config, "2")) {
            TOP_FATAL("node_mgr init failed");
            return 1;
        }

        if (!node_mgr.JoinRt()) {
            TOP_FATAL("node_mgr join failed");
            return 1;
        }
    }

    BenchCommand::Instance()->Run();

    return 0;
}
