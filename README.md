# xkad(a high performance X-DHT)

>A High Performance Distributed Hash Table implementation of TOP Network.

## Feature highlights

+ Lightweight and scalable, designed for large networks and small devices
+ Fully designed for Multi-Cores and Muti-Threads
+ High resilience to network disruption
+ Multi-Network support & High Performance & Low memory/cpu consumption
+ TCP,UDP,XUDP,ICMP protocols support, and IPv4 & IPv6 support
+ Public key cryptography layer providing optional data signature and encryption
+ iOS, Android, Windows, MacOS, Linux support

## Example

### C++ example

The demo and bench directory includes some simple example programs:

+ Kademlia, a High Performance Kademlia


```
int main() {
    // something init
    // ....
    //

    // load config
    base::Config config;
    std::string local_ip;
    ASSERT_TRUE(config.Get("node", "local_ip", local_ip));
    uint16_t local_port = 0;
    NatManagerIntf::Instance()->SetNatType(kNatTypePublic);
    udp_transport_.reset(new top::transport::UdpTransport());
    thread_message_handler_ = std::make_shared<transport::MultiThreadHandler>();
    thread_message_handler_->Init();
    auto kad_key = std::make_shared<base::PlatformKadmliaKey>();
    kad_key->set_xnetwork_id(top::kRoot);
    uint32_t zone_id = 0;
    kad_key->set_zone_id(zone_id);
    auto local_node_ptr = CreateLocalInfoFromConfig(config, kad_key);
    const uint64_t service_type = kRoot;
    local_node_ptr->set_service_type(service_type);

    // create routing table
    routing_table_ptr_.reset(new top::kadmlia::RoutingTable(
            udp_transport_,
            kNodeIdSize,
            local_node_ptr));
    kad_message_handler_.set_routing_ptr(routing_table_ptr_);
    std::set<std::pair<std::string, uint16_t>> boot_endpoints{
        { node_mgr_->LocalIp(), node_mgr_->RealLocalPort() }
    };

    // join network
    int res = routing_table_ptr_->MultiJoin(boot_endpoints);


    // something uninit
    // ....
    return res;
}
    
```


## Contact

[TOP Network](https://www.topnetwork.org/)

## License

Copyright (c) 2017-2019 Telos Foundation & contributors

Distributed under the MIT software license, see the accompanying

file COPYING or http://www.opensource.org/licenses/mit-license.php.
