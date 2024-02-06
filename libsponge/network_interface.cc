#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    if (_forward_table.find(next_hop_ip) != _forward_table.end()) {
        EthernetFrame ip_frame;
        ip_frame.header().type = EthernetHeader::TYPE_IPv4;
        ip_frame.header().dst = _forward_table[next_hop_ip]._add;
        ip_frame.header().src = _ethernet_address;
        ip_frame.payload() = dgram.serialize();
        _frames_out.push(ip_frame);
    } else {
        _waiting.push(std::make_tuple(dgram, next_hop));
        if (_request_time.find(next_hop_ip) != _request_time.end() &&
            _timer - _request_time[next_hop_ip] <= 5 * 1000)
            return;
        
        _request_time[next_hop_ip] = _timer;
        ARPMessage msg;
        msg.opcode = ARPMessage::OPCODE_REQUEST;
        msg.sender_ethernet_address = _ethernet_address;
        msg.sender_ip_address = _ip_address.ipv4_numeric();
        msg.target_ethernet_address = {0, 0, 0, 0, 0, 0};
        msg.target_ip_address = next_hop_ip;
        
        EthernetFrame broadcast_frame;
        broadcast_frame.payload() = move(msg.serialize());
        broadcast_frame.header().type = EthernetHeader::TYPE_ARP;
        broadcast_frame.header().dst = ETHERNET_BROADCAST;
        broadcast_frame.header().src = _ethernet_address;
        _frames_out.push(broadcast_frame);
    }
    
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    
    EthernetHeader header = frame.header();

    if (header.dst != _ethernet_address && header.dst != ETHERNET_BROADCAST)
        return std::nullopt;

    if (header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ret;
        if (ret.parse(frame.payload()) == ParseResult::NoError)
            return ret;
    }

    // if the frame's type is arp
    if (header.type == EthernetHeader::TYPE_ARP) {
        ARPMessage msg;

        if (msg.parse(frame.payload()) != ParseResult::NoError) 
            return std::nullopt;
        
        // remember the mapping for 30 seconds
        _forward_table[msg.sender_ip_address] = _mac_with_time(msg.sender_ethernet_address, _timer + 30 * 1000);
        if (msg.target_ip_address == _ip_address.ipv4_numeric() &&
            msg.opcode == ARPMessage::OPCODE_REQUEST) {
            ARPMessage reply;
            reply.opcode = ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ethernet_address = msg.sender_ethernet_address;
            reply.target_ip_address = msg.sender_ip_address;
            EthernetFrame arp_frame;
            arp_frame.payload() = reply.serialize();
            arp_frame.header().type = EthernetHeader::TYPE_ARP;
            arp_frame.header().src = reply.sender_ethernet_address;
            arp_frame.header().dst = reply.target_ethernet_address;
            _frames_out.push(arp_frame);
        } 

        if (msg.opcode == ARPMessage::OPCODE_REPLY) {
            while(!_waiting.empty() && std::get<1>(_waiting.front()).ipv4_numeric() == msg.sender_ip_address) {
                send_datagram(std::get<0>(_waiting.front()), std::get<1>(_waiting.front()));
                _waiting.pop();
            }
        }
    }

    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _timer += ms_since_last_tick;
    for (auto it = _forward_table.begin(); it != _forward_table.end();) {
        if (_timer > it->second._time)
            _forward_table.erase(it++);
        else
            it++;
    }
}
