// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xkad/routing_table/nodeid_utils.h"
#include "xbase/xhash.h"

#include <limits>

namespace top {

namespace kadmlia {

uint8_t GetWorldCountryID(const std::string& country_code) {
    uint32_t hash = base::xhash32_t::digest(country_code);
    return (hash & 0xFF000000);
}

uint32_t GetBusinessID(const std::string& business) {
    auto iter = BusinessID.find(business);
    if (iter == BusinessID.end()) {
        return std::numeric_limits<uint32_t>::max();
    }

    return iter->second;
}

}  // namespace kadmlia

}  // namespace top
