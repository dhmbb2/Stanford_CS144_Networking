#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active()) return;

    _time_since_last_segment_received = 0;
    TCPHeader header = seg.header();

    // If ret is set, then end both inbound and outbound streams
    if (seg.header().rst) {
        _sender.stream_in().error();
        _receiver.stream_out().error();
        _is_active = false;
        return
    }

    _receiver.segment_received(seg);

    // if ack is set, then tell sender about the features in interest
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
        _send_segment();
    }

    if (seg.length_in_sequence_space() > 0) {
        _sender.send_empty_segment();
        _send_segment();
    }

}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    ByteStream stream = _sender.stream_in();
    
    size_t ret = stream.write(data);
    _sender.fill_window();
    _send_segment();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

void TCPConnection::end_input_stream() {
    _sender.stream_in().input_ended();
}

void TCPConnection::connect() {
    TCPSegment seg;
    seg.header().syn = true;
    _segments_out.push(seg);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
