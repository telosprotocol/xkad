// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <vector>
#include <functional>
#include <string>
#include <map>
#include <mutex>
#include "xpbase/base/top_config.h"
#include "xpbase/base/args_parser.h"

namespace top {
namespace kadmlia {
namespace test {

class BenchCommand {
public:
    using Arguments = std::vector<std::string>;
    using CommandProc = std::function<void (const Arguments&)>;
    using MapCommands = std::map<std::string, CommandProc>;

public:
    static BenchCommand* Instance();
    void Init(bool show_cmd);
    void Run();
    void ProcessCommand(const std::string& cmdline);
    void RegisterCommand(const std::string& cmd_name, CommandProc cmd_proc);
    void PrintUsage();

protected:
    bool show_cmd_{true};
    std::mutex map_commands_mutex_;
    MapCommands map_commands_;
};

class Util {
public:
    static Util* Instance();
    bool HandleParamsAndConfig(int argc, char** argv, base::Config& config);

private:
    bool ParseParams(int argc, char** argv, ArgsParser& args_parser);
    bool ResetConfig(ArgsParser& args_parser, base::Config& config);
};

}  // namespace test
}  // namespace kadmlia
}  // namespace top
