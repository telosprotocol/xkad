// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/local_node_info.h"

#include <limits>

#include "xbase/xhash.h"
#include "common/xdfcurve.h"
#include "common/xaes.h"
#include "common/secp256k1.h"
#include "common/sha2.h"

#include "xpbase/base/rand_util.h"
#include "xpbase/base/top_log.h"

namespace top {

namespace kadmlia {

LocalNodeInfo::LocalNodeInfo() {}
LocalNodeInfo::~LocalNodeInfo() {}

bool LocalNodeInfo::Init(
        const std::string& local_ip,
        uint16_t local_port,
        bool first_node,
        bool client,
        const std::string& idtype,
        base::KadmliaKeyPtr kadmlia_key,
        uint64_t service_type,
        uint32_t role) {
    local_ip_ = local_ip;
    local_port_ = local_port;
    first_node_ = first_node;
    client_mode_ = client;
    idtype_ = idtype;
    kadmlia_key_ = kadmlia_key;
    xip_ = std::make_shared<base::XipParser>(kadmlia_key->Xip());
    TOP_INFO("local_node_start: kad_key[%s]; xip[%s]",
            HexEncode(kad_key()).c_str(),
            HexEncode(xip_->xip()).c_str());
    if (first_node_) {
        public_ip_ = local_ip_;
        public_port_ = local_port_;
    }
    if (!nat_manager_->GetLocalNatType(nat_type_)
            || nat_type_ == kNatTypeUnknown) {
        TOP_ERROR("bluenat get local nat type(%d) failed", nat_type_);
        return false;
    }
    service_type_ = kadmlia_key_->GetServiceType();
    role_ = role;
    score_ = RandomUint32() % 100;
    hash64_ = base::xhash64_t::digest(global_xid->Get());
    return true;
}

void LocalNodeInfo::Reset() {
    xip_ = nullptr;
    kadmlia_key_ = nullptr;
    local_ip_ = "";
    local_port_ = 0;
    first_node_ = false;
    client_mode_ = false;
    private_key_ = "";
    public_key_ = "";
    public_ip_ = "";
    public_port_ = 0;
    nat_type_ = kNatTypeUnknown;
    idtype_ = "";
    role_ = kRoleInvalid;
}

bool LocalNodeInfo::IsPublicNode() const {
    return local_ip_ == public_ip_ && local_port_ == public_port_;
}
std::string LocalNodeInfo::kad_key() {
    return id();
}
std::string LocalNodeInfo::xid() { return global_xid->Get(); }
std::string LocalNodeInfo::xip() { return GetXipParser().xip(); }

void LocalNodeInfo::set_xip(const std::string& xip_str) {
    base::XipParser xip(xip_str);
    *xip_ = xip;
}

std::string LocalNodeInfo::id() {
    assert(kadmlia_key_);
    std::lock_guard<std::mutex> lock(kadkey_mutex_);
    return kadmlia_key_->Get();
}

base::XipParser& LocalNodeInfo::GetXipParser() {
    if (client_mode_) {
        std::unique_lock<std::mutex> lock(dxip_node_map_mutex_);
        base::XipParserPtr client_xip_ptr;
        if (dxip_node_map_.empty()) {
            client_xip_ptr.reset(new base::XipParser());
            return *client_xip_ptr;
        }
        std::string client_random_self_dxip = dxip_node_map_.begin()->first;
        client_xip_ptr.reset(new base::XipParser(client_random_self_dxip));
        return *client_xip_ptr;
    }
    return *xip_;
};

void LocalNodeInfo::AddDxip(const std::string& node_id, const std::string& dxip) {
    {
        std::unique_lock<std::mutex> lock(node_dxip_map_mutex_);
        node_dxip_map_[node_id] = dxip;
    }
    {
        std::unique_lock<std::mutex> lock(dxip_node_map_mutex_);
        dxip_node_map_[dxip] = node_id;
    }
}

void LocalNodeInfo::DropDxip(const std::string& node_id) {
    std::string tmp_dxip;
    {
        std::unique_lock<std::mutex> lock(node_dxip_map_mutex_);
        auto ifind = node_dxip_map_.find(node_id);
        if (ifind == node_dxip_map_.end()) {
            return;
        }
        tmp_dxip = ifind->second;
        node_dxip_map_.erase(ifind);
        TOP_DEBUG("dropdxip1: node(%s) dxip(%s)",
                HexSubstr(node_id).c_str(),
                HexEncode(tmp_dxip).c_str());
    }
    {
        std::unique_lock<std::mutex> lock(dxip_node_map_mutex_);
        auto ifind = dxip_node_map_.find(tmp_dxip);
        if (ifind != dxip_node_map_.end()) {
            dxip_node_map_.erase(ifind);
            TOP_DEBUG("dropdxip2: node(%s)", HexSubstr(node_id).c_str());
        }
    }
}


bool LocalNodeInfo::HasDynamicXip(const std::string& dxip) {
    std::unique_lock<std::mutex> lock(dxip_node_map_mutex_);
    auto ifind = dxip_node_map_.find(dxip);
    if (ifind != dxip_node_map_.end()) {
        return true;
    }
    return false;
}

}  // namespace kadmlia

}  // namespace top
