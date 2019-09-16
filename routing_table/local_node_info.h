// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <assert.h>
#include <string>
#include <mutex>
#include <list>
#include <memory>

#include "xkad/routing_table/routing_utils.h"
#include "xpbase/base/xid/xid_def.h"
#include "xkad/nat_detect/nat_defines.h"
#include "xpbase/base/kad_key/kadmlia_key.h"
#include "xpbase/base/xip_parser.h"
#include "xkad/nat_detect/nat_manager_intf.h"

namespace top {

namespace kadmlia {

class LocalNodeInfo {
public:
    LocalNodeInfo();
    ~LocalNodeInfo();

    bool Init(
            const std::string& local_ip,
            uint16_t local_port,
            bool first_node,
            bool client,
            const std::string& idtype,
            base::KadmliaKeyPtr kadmlia_key,
            uint64_t service_type,
            uint32_t role);
    void Reset();
    bool IsPublicNode() const;
    std::string kad_key();
    std::string xid();
    std::string xip();
    std::string id();
    base::XipParser& GetXipParser();
    std::string local_ip() { return local_ip_; }
    uint16_t local_port() { return local_port_; }
    void set_local_port(uint16_t local_port) { local_port_ = local_port; }
    bool first_node() { return first_node_; }
    void set_first_node(bool first_node) { first_node_ = first_node; }
    bool client_mode() { return client_mode_; }
    std::string private_key() { return private_key_; }
    std::string public_key() { return public_key_; }
    std::string public_ip() { return public_ip_; }
    std::string idtype() { return idtype_; }
    uint16_t public_port() { return public_port_; }
    int32_t nat_type() { return nat_type_; }
    void set_public_ip(const std::string& ip) { public_ip_ = ip; }
    void set_public_port(uint16_t port) { public_port_ = port; }
    uint64_t service_type() { return service_type_; }
    void set_service_type(uint64_t service_type) { service_type_ = service_type; }
    uint32_t routing_table_id() { return routing_table_id_; }
    void set_routing_table_id(uint32_t routing_table_id) { routing_table_id_ = routing_table_id; }
    uint32_t role() { return role_; }
    void set_role(int32_t role_type) { role_ = role_type; }
    base::KadmliaKeyPtr kadmlia_key() { 
        std::lock_guard<std::mutex> lock(kadkey_mutex_);
        return kadmlia_key_; 
    }
    void set_kadmlia_key(base::KadmliaKeyPtr kadmlia_key) { 
        std::lock_guard<std::mutex> lock(kadkey_mutex_);
        kadmlia_key_ = kadmlia_key; 
    }
    uint32_t score() { return score_; }
    void set_xip(const std::string& xip_str);
    inline bool use_kad_key() {
        if (kadmlia_key_) {
            return true;
        }
        return false;
    }
    bool HasDynamicXip(const std::string& dxip);
    void AddDxip(const std::string& node_id, const std::string& dxip);
    void DropDxip(const std::string& node_id);
    uint16_t rpc_http_port() { return rpc_http_port_; }
    uint16_t rpc_ws_port() { return rpc_ws_port_; }
    void set_rpc_http_port(uint16_t http_port) { rpc_http_port_ = http_port; }
    void set_rpc_ws_port(uint16_t ws_port) { rpc_ws_port_ = ws_port; }
    bool is_root() { return is_root_; }
    void set_is_root(bool root) { is_root_ = root; }
    uint64_t hash64() { return hash64_; }

private:
    std::string local_ip_;
    uint16_t local_port_{ 0 };
    uint16_t rpc_http_port_{ 0 };
    uint16_t rpc_ws_port_{ 0 };
    bool first_node_{ false };
    bool client_mode_{ false };
    std::string private_key_;
    std::string public_key_;
    std::string public_ip_;
    uint16_t public_port_{ 0 };
    int32_t nat_type_{kNatTypeUnknown};
    std::string idtype_;
    uint64_t service_type_{ kInvalidType };
    uint32_t routing_table_id_{0};
    uint32_t role_{ kRoleInvalid };
    base::XipParserPtr xip_{std::make_shared<base::XipParser>()};
    std::mutex kadkey_mutex_;
    base::KadmliaKeyPtr kadmlia_key_{ nullptr };
    uint32_t score_;
    // key is node_id, value is dynamicxip distribute by node_id
    std::map<std::string, std::string> node_dxip_map_;
    std::mutex node_dxip_map_mutex_;
    std::map<std::string, std::string> dxip_node_map_;
    std::mutex dxip_node_map_mutex_;
    bool is_root_{ false };
    uint64_t hash64_{ 0 };
    kadmlia::NatManagerIntf* nat_manager_{kadmlia::NatManagerIntf::Instance()};


    DISALLOW_COPY_AND_ASSIGN(LocalNodeInfo);
};

typedef std::shared_ptr<LocalNodeInfo> LocalNodeInfoPtr;

}  // namespace kadmlia

}  // namespace top
