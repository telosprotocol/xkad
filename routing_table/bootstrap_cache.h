// Copyright (c) 2017-2019 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <mutex>

#include "xpbase/base/top_utils.h"
#include "xpbase/base/top_config.h"

namespace top {
namespace kadmlia {

using BootstrapEndpoint = std::pair<std::string, uint16_t>;
using VecBootstrapEndpoint = std::vector<BootstrapEndpoint>;

class BootstrapCacheManager;
class BootstrapCache {
    friend class BootstrapCacheManager;
    using Lock = std::unique_lock<std::mutex>;
    const uint32_t INVALID_SERVICE_TYPE = (uint32_t)-1;

public:
    BootstrapCache();
    ~BootstrapCache();
    bool GetCache(VecBootstrapEndpoint& vec_bootstrap_endpoint);
    bool SetCache(const VecBootstrapEndpoint& vec_bootstrap_endpoint);

private:
    uint64_t service_type_{INVALID_SERVICE_TYPE};
    BootstrapCacheManager* manager_{nullptr};

private:
    DISALLOW_COPY_AND_ASSIGN(BootstrapCache);
};

using BootstrapCachePtr = std::shared_ptr<BootstrapCache>;

BootstrapCachePtr GetBootstrapCache(uint64_t service_type);

class BootstrapCacheManager {
    using Lock = std::unique_lock<std::mutex>;
public:
    static BootstrapCacheManager* Instance();
    BootstrapCacheManager();
    ~BootstrapCacheManager();
    bool Init();
    BootstrapCachePtr GetBootStrapCache(uint64_t service_type);
    bool SetCache(uint64_t service_type, const VecBootstrapEndpoint& vec_bootstrap_endpoint);
    bool GetCache(uint64_t service_type, VecBootstrapEndpoint& vec_bootstrap_endpoint);

private:
    std::mutex mutex_;
    bool inited_{false};
    std::map<uint64_t, BootstrapCachePtr> map_;

private:
    DISALLOW_COPY_AND_ASSIGN(BootstrapCacheManager);
};

}  // namespace kadmlia
}  // namespace top
