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
    if (seg.header().rst) 
        _abort();

    _receiver.segment_received(seg);

    // check if no need to linger
    if (_receiver.inbound_ended() && !_sender.outbound_ended()) {
        _linger_after_streams_finish = false;
    }

    bool is_sent = false;
    // if ack is set, then tell sender about the features in interest
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
        _send_segment();
    }

    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        is_sent = _send_segment();
        if (!is_sent) {
            _sender.send_empty_segment();
            _send_segment();
        }
    }

}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    ByteStream &stream = _sender.stream_in();
    
    size_t ret = stream.write(data);
    _sender.fill_window();
    _send_segment();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    // in case of retransmission, send here
        _send_segment();
    _time_since_last_segment_received += ms_since_last_tick; 

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //abort the connection
        _abort();
        // send a rst to the peer
        _send_rst();
    }

    _clean_shutdown();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    _send_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _send_segment();
}

bool TCPConnection:: _send_segment() { 
    bool is_sent = false;
    std::queue<TCPSegment>& sender_segment_out = _sender.segments_out();
    while (!sender_segment_out.empty()) {
        is_sent = true;
        TCPSegment seg = sender_segment_out.front();
        sender_segment_out.pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    return is_sent;
}

void TCPConnection::_abort() {
    _sender.stream_in().error();
    _receiver.stream_out().error();
    _is_active = false;
}

void TCPConnection::_send_rst() {
    // send a rst to the peer
    _sender.send_empty_segment();
    // since the sender's queue is emptied the instance it is filled with 
    // some segment, the empty segment must be at the front
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.ackno().has_value()) {
        seg.header().ackno = _receiver.ackno().value();
        seg.header().win = _receiver.window_size(); 
    }
    seg.header().rst = true;
    _segments_out.push(seg);
}

void TCPConnection::_clean_shutdown() {
    // if the prerequisited are not satisfied, do nothing
    if (!(_receiver.inbound_ended() && 
          _sender.outbound_ended() &&
          _sender.outbound_acknowledged()))
        return;
    
    if (_linger_after_streams_finish && 
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout)
        _is_active = false;

    if (!_linger_after_streams_finish)
        _is_active = false;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _abort();
            _send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
