#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    _routing_table.push_back(route_entry(
        route_prefix,
        prefix_length,
        next_hop,
        interface_num
    ));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    uint8_t max_prefix_len=0;
    route_entry ans{};
    bool router_found = false;
    for (auto& x: _routing_table) {
        if (!_is_match(dgram.header().dst, x._route_prefix, x._prefix_length))
            continue;
        if (!router_found) {
            max_prefix_len = x._prefix_length;
            router_found = true;
            ans = x;
        }
        if (x._prefix_length > max_prefix_len) {
            max_prefix_len = x._prefix_length;
            ans = x;
        }
    }
    if (!router_found) 
        return;

    if (dgram.header().ttl <= 1)
        return;
    dgram.header().ttl--;

    if (ans._next_hop.has_value()) 
        _interfaces[ans._interface_num].send_datagram(dgram, ans._next_hop.value());
    else 
        _interfaces[ans._interface_num].send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}

bool Router::_is_match(uint32_t ip1, uint32_t ip2, uint8_t len) {
    // undefined behaviour if moving bits are greater than number of bits the value type has
    uint32_t mask = (len == 0) ? 0 : 0xffffffffu << (32 - len);
    return (ip1 & mask) == (ip2 & mask);
}
