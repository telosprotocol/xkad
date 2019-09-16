// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/routing_utils.h"

#ifdef _MSC_VER
#define _WINSOCKAPI_
#include <windows.h>
#endif

#include <locale>
#include <limits>
#include <algorithm>

#include "xpbase/base/top_log.h"
#include "xpbase/base/top_utils.h"
#include "xpbase/base/endpoint_util.h"
#include "xkad/routing_table/routing_table.h"
#include "xkad/routing_table/nodeid_utils.h"
#include "xpbase/base/line_parser.h"
#include "xpbase/base/xid/xid_parser.h"
#include "xpbase/base/xip_parser.h"
#include "xpbase/base/kad_key/get_kadmlia_key.h"
#include "xbase/xhash.h"

namespace top {

namespace kadmlia {

//extern std::shared_ptr<base::KadmliaKey> global_xid;

void toupper(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}

void tolower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}

// edge node
std::string GenNodeIdType(uint8_t country_code, uint8_t service_type) {
    // 3 bytes network_id(service_type), 1 byte country_code
    // after serialize string, xid: [country_code:network_id], eg. [38040000]
    uint32_t ser = service_type;
    char  s[4];
    memcpy(s, &country_code, 1);
    memcpy(s + 1, &ser, 3);
    std::string str(s, 4);
    return str;
}

std::string GenNodeIdType(const std::string& in_country, const std::string& in_business) {
    // NodeId type total kNodeIdType = 4 bytes, 3 bytes network_id + 1 byte country_code
    if (in_country.empty() || in_business.empty()) {
        return "";
    }
    std::string country(in_country);
    std::string business(in_business);
    top::kadmlia::toupper(country);
    top::kadmlia::toupper(business);

    // country_code
    uint8_t countrycode = GetWorldCountryID(country);
    if (countrycode == std::numeric_limits<uint8_t>::max()) {
        return "";
    }

    // network_id
    uint32_t businesscode = GetBusinessID(business);
    if (businesscode >= std::numeric_limits<uint32_t>::max()) {
        return "";
    }

    return GenNodeIdType(countrycode, businesscode);
}

std::string GenRandomID(const std::string& country, const std::string& business) {
    std::string idtype(GenNodeIdType(country, business));
    if (idtype.empty()) {
        return std::string("");
    }
    return idtype + RandomString(kNodeIdSize - kNodeIdTypeSize);
}

std::string GenRandomID(uint8_t country_code, uint8_t service_type) {
    /*
    std::string idtype(GenNodeIdType(country_code, service_type));
    if (idtype.empty()) {
        return std::string("");
    }
    return idtype + RandomString(kNodeIdSize - kNodeIdTypeSize);
    */
    base::KadmliaKeyPtr kad_key = base::GetKadmliaKey();
    kad_key->set_xnetwork_id(service_type);
    kad_key->set_zone_id(country_code);
    return kad_key->Get();
}

bool GetNetworkId(const std::string& id, uint32_t& network_id) {
    if (id.size() != kNodeIdSize) {
        return false;
    }

    network_id = *((uint8_t*)(id.c_str() + 1));  // NOLINT
    return true;
}

bool GetZoneIdFromConfig(const base::Config& config, uint32_t& zone_id) {
    if (!config.Get("node", "zone_id", zone_id)) {
        std::string country;
        if (!config.Get("node", "country", country)) {
            TOP_FATAL("get node country from config failed!");
            return false;
        }
        zone_id = base::xhash32_t::digest(country);
    }
    return true;
}

void GetPublicEndpointsConfig(
        const top::base::Config& config,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    std::string public_endpoints;
    if (!config.Get("node", "public_endpoints", public_endpoints)) {
        TOP_INFO("get node.public_endpoints failed");
        return;
    }

    top::base::ParseEndpoints(public_endpoints, boot_endpoints);
}

void GetPublicServiceEndpointsConfig(
        const top::base::Config& config,
        const std::string& service_name,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    std::string service_public_endpoints;
    if (!config.Get(service_name, "bootstrap", service_public_endpoints)) {
        TOP_ERROR("<smaug> node join service p2p network must has bootstrap endpoints");
        return;
    }
    top::base::ParseEndpoints(service_public_endpoints, boot_endpoints);
}

void GetAllPublicServiceEndpointsConfig(
        const top::base::Config& config,
        const std::string& service_list,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints) {
    top::base::LineParser line_split(service_list.c_str(), ',', service_list.size());
    for (uint32_t i = 0; i < line_split.Count(); ++i) {
        const std::string service_name = line_split[i];
        GetPublicServiceEndpointsConfig(config, service_name, boot_endpoints);
    }
}

uint32_t GetXNetworkID(const std::string& id) {
    base::XIDParser xid_parse;
    xid_parse.ParserFromString(id);
    std::shared_ptr<base::XID> xid_ptr = xid_parse.GetXID();
    uint32_t xnetwork_id = xid_ptr->GetXNetworkID();
    //uint8_t zone_id = xid_ptr->GetZoneID();
    return xnetwork_id;
}

uint8_t GetZoneID(const std::string& id) {
    base::XIDParser xid_parse;
    xid_parse.ParserFromString(id);
    std::shared_ptr<base::XID> xid_ptr = xid_parse.GetXID();
    //uint32_t xnetwork_id = xid_ptr->GetXNetworkID();
    uint8_t zone_id = xid_ptr->GetZoneID();
    return zone_id;
}

bool CreateGlobalXid(const base::Config& config) try {
    assert(!global_node_id.empty());
    global_node_id_hash = GetStringSha128(global_node_id);  // NOLINT
    global_xid = base::GetKadmliaKey(global_node_id, true);
    global_xid->set_xnetwork_id(kRoot);
    uint32_t zone_id;
    if (!kadmlia::GetZoneIdFromConfig(config, zone_id)) {
        TOP_FATAL("get zone id from config failed!");
        return false;
    }
    global_xid->set_zone_id(check_cast<uint8_t>(((zone_id >> 24) & 0xFF)));
    return true;
} catch(...) {
    TOP_FATAL("catch ...");
    return false;
}

LocalNodeInfoPtr CreateLocalInfoFromConfig(
        const base::Config& config,
        base::KadmliaKeyPtr kad_key) try {
    uint32_t zone_id = 0;
    if (!kadmlia::GetZoneIdFromConfig(config, zone_id)) {
        TOP_ERROR("get node zone id from config failed!");
        return nullptr;
    }
    auto xip = std::make_shared<base::XipParser>(kad_key->Xip());
    std::string idtype(top::kadmlia::GenNodeIdType(
            check_cast<uint8_t>(((zone_id >> 24) & 0xFF)),
            xip->xnetwork_id()));
    if (idtype.empty()) {
        TOP_ERROR("get node id is null!");
        return nullptr;
    }
    bool client_mode = false;
    config.Get("node", "client_mode", client_mode);
    bool first_node = false;
    config.Get("node", "first_node", first_node);
    std::string local_ip;
    if (!config.Get("node", "local_ip", local_ip)) {
        TOP_ERROR("get node local_ip from config failed!");
        return nullptr;
    }
    kadmlia::LocalNodeInfoPtr local_node_ptr = nullptr;
    //uint16_t local_port = transport_ptr_->local_port();
    // reset real local port in routing_table init
    uint16_t local_port = 0;
    local_node_ptr.reset(new top::kadmlia::LocalNodeInfo());
    if (!local_node_ptr->Init(
            local_ip,
            local_port,
            first_node,
            client_mode,
            idtype,
            kad_key,
            kad_key->xnetwork_id(),
            kRoleInvalid)) {
        TOP_ERROR("init local node info failed!");
        return nullptr;
    }

    uint16_t http_port = static_cast<uint16_t>(RandomUint32());
    config.Get("node", "http_port", http_port);
    uint16_t ws_port = static_cast<uint16_t>(RandomUint32());
    config.Get("node", "ws_port", ws_port);
    local_node_ptr->set_rpc_http_port(http_port);
    local_node_ptr->set_rpc_ws_port(ws_port);
    return local_node_ptr;
} catch (std::exception& e) {
    TOP_ERROR("catched error[%s]", e.what());
    return nullptr;
}

}  // namespace kadmlia

}  // namespace top
