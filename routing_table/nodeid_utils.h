// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>
#include <map>

#include "xpbase/base/top_utils.h"

namespace top {

namespace kadmlia {

// 12 bit for business
static const std::map<std::string, uint32_t> BusinessID = {
    { "ROOT", top::kRoot },
    { "CLIENT", top::kClient },

    { "EDGEXVPN", top::kEdgeXVPN },
    { "EDGEXMESSAGE", top::kEdgeTopMessage },
    { "EDGEXSTORAGE", top::kEdgeTopStorage },

    { "XVPN", top::kXVPN },
    { "XMESSAGE", top::kTopMessage },
    { "XSTORAGE", top::kTopStorage },
};

// 8 bit for countrycode
uint8_t GetWorldCountryID(const std::string& country_code);
uint32_t GetBusinessID(const std::string& business);

}  // namespace kadmlia

}  // namespace top
