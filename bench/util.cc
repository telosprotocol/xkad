// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"
#include <iostream>
#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_log.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/top_string_util.h"
#include <assert.h>

namespace top {
namespace kadmlia {
namespace test {

// --------------------------------------------------------------------------------
BenchCommand* BenchCommand::Instance() {
    static BenchCommand ins;
    return &ins;
}

void BenchCommand::Init(bool show_cmd) {
    show_cmd_ = show_cmd;
}

void BenchCommand::Run() {
    PrintUsage();

    for (;;) {
        if (!show_cmd_) {
            SleepUs(200 * 1000);
            continue;
        }

        std::cout << "\nEnter command > ";
        std::string cmdline;
        std::getline(std::cin, cmdline);
        {
            ProcessCommand(cmdline);
        }
    }
}

void BenchCommand::ProcessCommand(const std::string& cmdline) {
    if (cmdline.empty()) {
        return;
    }

    std::string cmd;
    Arguments args;
    try {
        top::base::LineParser line_split(cmdline.c_str(), ' ', cmdline.size());
        cmd = "";
        for (uint32_t i = 0; i < line_split.Count(); ++i) {
            if (strlen(line_split[i]) == 0) {
                continue;
            }

            if (cmd == "")
                cmd = line_split[i];
            else
                args.push_back(line_split[i]);
        }
    } catch (const std::exception& e) {
        TOP_WARN("Error processing command: %s", e.what());
    }

    std::unique_lock<std::mutex> lock(map_commands_mutex_);
    auto it = map_commands_.find(cmd);
    if (it == map_commands_.end()) {
        std::cout << "Invalid command : " << cmd << std::endl;
        // PrintUsage();
    } else {
        (it->second)(args);  // call command procedure
    }
}

void BenchCommand::RegisterCommand(const std::string& cmd_name, CommandProc cmd_proc) {
    assert(cmd_proc);
    std::unique_lock<std::mutex> lock(map_commands_mutex_);

    auto it = map_commands_.find(cmd_name);
    if (it != map_commands_.end()) {
        TOP_WARN("command(%s) exist and ignore new one", cmd_name.c_str());
        return;
    }

    map_commands_[cmd_name] = cmd_proc;
    TOP_INFO("add command(%s)", cmd_name.c_str());
}

void BenchCommand::PrintUsage() {
    // std::cout << "\thelp Print options.\n";
    // std::cout << "\tjoin Normal Join.\n";
    // std::cout << "\tprt Print Local Routing Table.\n";
    // std::cout << "\trrt <dest_index> Request Routing Table from peer node with the specified"
    //           << " identity-index.\n";
    // std::cout << "\tsave save all nodes but nodes in routing table.\n";
    // std::cout << "\trt relay test, this node to other hop count.\n";
    // std::cout << "\tart all nodes relay test, this node to other hop count.\n";
    // std::cout << "\tgroup get groups by target id.\n";
    // std::cout << "\tsync sync all nodes from bootstrap node.\n";
    // std::cout << "\tgets get service nodes of service_type.\n"; 
    // std::cout << "\tsets set service type for gets.\n";

    std::cout << "all commands:" << std::endl;
    int n = 0;
    std::unique_lock<std::mutex> lock(map_commands_mutex_);
    for (auto& kv : map_commands_) {
        auto fmt = base::StringUtil::str_fmt(" [%d] %s: %s", n, kv.first.c_str(), "help?");
        n += 1;
        std::cout << fmt << std::endl;
    }
}

// --------------------------------------------------------------------------------
Util* Util::Instance() {
    static Util ins;
    return &ins;
}

bool Util::HandleParamsAndConfig(int argc, char** argv, base::Config& config) {
    top::ArgsParser args_parser;
    if (!ParseParams(argc, argv, args_parser)) {
        TOP_FATAL("parse params failed!");
        return false;
    }

    if (args_parser.HasParam("h")) {
        std::cout << "Allowed options:" << std::endl;
        std::cout << "\t-h [help]            print help info" << std::endl;
        std::cout << "\t-g [show_cmd]        show cmd" << std::endl;
        std::cout << "\t-p [peer]            bootstrap peer[ip:port]" << std::endl;
        std::cout << "\t-i [identity_index]  only first node need" << std::endl;
        std::cout << "\t-l [local_port]      local udp port" << std::endl;
        std::cout << "\t-a [local_ip]        local ip " << std::endl;
        std::cout << "\t-L [log_path]        log path" << std::endl;
        std::cout << "\t-D [log_level]       log level" << std::endl;
        exit(0);
    }

    if (!ResetConfig(args_parser, config)) {
        TOP_FATAL("reset edge config with arg parser failed!");
        return false;
    }

    return true;
}

bool Util::ParseParams(int argc, char** argv, ArgsParser& args_parser) {
    args_parser.AddArgType('h', "help", top::kNoValue);
    args_parser.AddArgType('g', "show_cmd", top::kMustValue);
    args_parser.AddArgType('p', "peer", top::kMustValue);
    args_parser.AddArgType('i', "identity_index", top::kMustValue);
    args_parser.AddArgType('l', "local_port", top::kMustValue);
    args_parser.AddArgType('a', "local_ip", top::kMustValue);
    args_parser.AddArgType('L', "log_path", top::kMustValue);
    args_parser.AddArgType('D', "log_level", top::kMustValue);

    std::string tmp_params = "";
    for (int i = 1; i < argc; i++) {
        if (strlen(argv[i]) == 0) {
            tmp_params += static_cast<char>(31);
        } else {
            tmp_params += argv[i];
        }
        tmp_params += " ";
    }

    std::string err_pos;
    if (args_parser.Parse(tmp_params, err_pos) != top::kadmlia::kKadSuccess) {
        std::cout << "parse params failed!" << std::endl;
        return false;
    }

    return true;
}

bool Util::ResetConfig(ArgsParser& args_parser, base::Config& config) {
    int show_cmd = 1;
    if (args_parser.GetParam("g", show_cmd) == top::kadmlia::kKadSuccess) {
        if (!config.Set("node", "show_cmd", show_cmd == 1)) {
            TOP_FATAL("set config failed [node][show_cmd][%d]", show_cmd);
            return false;
        }
    }

    std::string peer;
    args_parser.GetParam("p", peer);
    if (!peer.empty()) {
        if (!config.Set("node", "public_endpoints", peer)) {
            TOP_FATAL("set config failed [node][public_endpoints][%s]", peer.c_str());
            return false;
        }
    }

    int identity_index = 1;
    if (args_parser.GetParam("i", identity_index) == top::kadmlia::kKadSuccess) {
        bool first_node = false;
        if (identity_index == 0) {
            first_node = true;
        }
        if (!config.Set("node", "first_node", first_node)) {
            TOP_FATAL("set config failed [node][first_node][%d]", first_node);
            return false;
        }
    }

    uint16_t local_port = 0;
    if (args_parser.GetParam("l", local_port) == top::kadmlia::kKadSuccess) {
        if (!config.Set("node", "local_port", local_port)) {
            TOP_FATAL("set config failed [node][local_port][%d]", local_port);
            return false;
        }
    }

    std::string local_ip;
    args_parser.GetParam("a", local_ip);
    if (!local_ip.empty()) {
        if (!config.Set("node", "local_ip", local_ip)) {
            TOP_FATAL("set config failed [node][local_ip][%s]", local_ip.c_str());
            return false;
        }
    }

    std::string log_path;
    if (args_parser.GetParam("L", log_path) == top::kadmlia::kKadSuccess) {
        if (!config.Set("node", "log_path", log_path)) {
            TOP_FATAL("set config failed [node][log_path][%s]", log_path.c_str());
            return false;
        }
    }

    int log_level = 0;
    if (args_parser.GetParam("D", log_level) == top::kadmlia::kKadSuccess) {
        if (!config.Set("node", "log_level", log_level)) {
            TOP_FATAL("set config failed [node][log_level][%d]", log_level);
            return false;
        }
    }

    return true;
}

}  // namespace test
}  // namespace kadmlia
}  // namespace top
