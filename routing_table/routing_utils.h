// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <mutex>
#include <random>
#include <algorithm>
#include <chrono>
#include <vector>
#include <set>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/error_code.h"
#include "xpbase/base/top_string_util.h"
#include "xpbase/base/top_config.h"
#include "xpbase/base/xip_parser.h"
#include "xpbase/base/kad_key/kadmlia_key.h"

namespace top {

namespace kadmlia {

class LocalNodeInfo;
typedef std::shared_ptr<LocalNodeInfo> LocalNodeInfoPtr;

enum RoutingMessageRequestType {
    kNone = 0,
    kRequestMsg,
    kResponseMsg,
};

enum HandshakeType {
    kHandshakeRequest = 1,
    kHandshakeResponse = 2,
};

enum RelayMessageCode {
    kErrorReturn  = 1,
    kSuccessReturn,
    kContinue,
 };

/*
enum RoutingMessageType {
    kKadConnectRequest = 0,
    kKadConnectResponse = 1,
    kKadHandshake = 2,
    kKadBootstrapJoinRequest = 3,
    kKadBootstrapJoinResponse = 4,
    kKadFindNodesRequest = 5,
    kKadFindNodesResponse = 6,
    kKadAck = 7,
    kKadHeartbeatRequest = 8,
    kKadHeartbeatResponse = 9,

    kKadNatDetectRequest = 23,
    kKadNatDetectResponse = 24,
    kKadNatDetectHandshake2Node = 25,
    kKadNatDetectHandshake2Boot = 26,
    kKadNatDetectFinish = 27,
    kKadDropNodeRequest = 28,
    kKadMessageTypeMax,  // other message mast bigger than it
};
*/

enum VpnProtocal {
    kTcp           = 8080,
    kXtcp          = 14550,
    kSsl1          = 443,
    kSsl           = 465,
    kHttp          = 80,
    kTdns          = 533,

    kUdp           = 500,
    kXudp          = 18721,
    kDns           = 53,
    kDns1          = 531,
    kIcmp          = 100,
};

struct AddressInfo {
    std::string   ip;
    uint16_t      port;
    std::string   detect_local_ip;
    uint16_t      detect_local_port;
};

struct UdpClientNatInfo {
    uint32_t     protocal;
    std::string  local_ip;
    uint16_t     local_port;
    std::string  public_ip;
    uint16_t     public_port;
};

struct VpnServer {
    int     protocal;
    std::vector<AddressInfo> vpn_info;
};

typedef std::vector<VpnServer> VpnArray;

struct MessageIdentity {
    MessageIdentity():message_id(), node_id() {}
    MessageIdentity(
        const uint32_t in_message_id,
        const std::string& in_node_id) 
        :message_id(in_message_id), 
        node_id(in_node_id){
    }
    virtual ~MessageIdentity() {}
    virtual std::string ToString() const {
        return base::StringUtil::str_fmt("%d_", message_id) + node_id;
    }
    bool operator< (const MessageIdentity& other) const {
        if(message_id < other.message_id) {
            return true;
        } else if(message_id == other.message_id) {
            if(node_id < other.node_id) {
                return true;
            }
        }
        return false;
    }
    uint32_t message_id;
    std::string node_id;
};

static const int kKadParamK = 8;
static const int kKadParamAlpha = 2;
static const int kKadParamAlphaRandom = 1;  // 0 if no random node
static const int kNodeIdTypeSize = 4;
static const int kRoutingMaxNodesSize = kNodeIdSize * 8 * kKadParamK;
static const int kFindNodesMaxSize = kRoutingMaxNodesSize;  // max size find nodes from neighbors
static const int kRandomRoutingPos = kRoutingMaxNodesSize  *  2 / 3;
static const int kClosestNodesNum = 16;
static const int kDetectionTimes = 4;
static const int kHopToLive = 20;
static const int kJoinRetryTimes = 5;
static const uint32_t kFindNodesBloomfilterBitSize = 4096;
static const uint32_t kFindNodesBloomfilterHashNum = 11;

static const std::string kUdpNatDetectMagic = "UdpNatDetectMagic";
static const std::string LOCAL_COUNTRY_DB_KEY = "local_country_code";
static const std::string LOCAL_EDGE_DB_KEY = "local_edge";
static const std::string TCP_RELAY_PORT_DB_KEY = "tcp_relay_ports";
static const std::string BOOTSTRAP_CACHE_DB_KEY = "bootstrap_cache";
static const std::string VERSION_KEY = "NODE_VERSION";
static const std::string COPYRIGHT_KEY = "NODE_COPYRIGHT";

std::string GenNodeIdType(const std::string& country, const std::string& business);
std::string GenRandomID(const std::string& country, const std::string& business);
std::string GenNodeIdType(uint8_t country_code, uint8_t service_type);
std::string GenRandomID(uint8_t country_code, uint8_t service_type);
bool GetNetworkId(const std::string& id, uint32_t& network_id);
bool GetZoneIdFromConfig(const base::Config& config, uint32_t& zone_id);
void GetPublicEndpointsConfig(
        const top::base::Config& config,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
void GetPublicServiceEndpointsConfig(
        const top::base::Config& config,
        const std::string& service_name,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
void GetAllPublicServiceEndpointsConfig(
        const top::base::Config& config,
        const std::string& service_list,
        std::set<std::pair<std::string, uint16_t>>& boot_endpoints);
uint32_t GetXNetworkID(const std::string& id);
uint8_t  GetZoneID(const std::string& id);
void toupper(std::string &str);
void tolower(std::string &str);

bool CreateGlobalXid(const base::Config& config);
LocalNodeInfoPtr CreateLocalInfoFromConfig(
        const base::Config& config,
        base::KadmliaKeyPtr kad_key);

}  // namespace kadmlia

}  // namespace top
