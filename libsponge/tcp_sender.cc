#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight;
}

void TCPSender::fill_window() {
    // std::cout << _next_seqno << '\n';
    // if syn is not sent yet`
    // std::cout << _window_size << '\n';
    // cannot use _next_seqno as indicator of is_syn, this will cause problem if 
    // syn is sent but nothing is acked.
    if (!_is_syned) {
        TCPSegment seg;
        seg.header().syn = true;
        _is_syned = true;
        _send_segment(seg);
        return;
    }

    uint64_t tmp_win_size = (_window_size == 0) ? 1 : _window_size;

    uint64_t _availiable_win_size = tmp_win_size - (_next_seqno - _ack_seqno);

    // fill window
    while (!_stream.buffer_empty() && _availiable_win_size && !_fin_sent) {
        size_t send_len = min({
            static_cast<size_t>(_availiable_win_size), 
            TCPConfig::MAX_PAYLOAD_SIZE, 
            _stream.buffer_size()
        });
        TCPSegment seg;
        seg.payload() = _stream.read(send_len);
        // cout << seg.payload().str() << " "  << send_len << ' ' << _window_size  << '\n';
        // Fin flag needs one byte, so add it to this segment only if window size is not zero.
        if (_stream.eof() && _availiable_win_size-send_len) {
            seg.header().fin = true;
            _fin_sent = true;
        }
        _send_segment(seg);
        // update avail_space
        _availiable_win_size = tmp_win_size - (_next_seqno - _ack_seqno);
    }

    if (_stream.buffer_empty() && _availiable_win_size && _stream.eof() && !_fin_sent) {
        TCPSegment seg;
        seg.header().fin = true;
        _fin_sent = true;
        _send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // remove imposible ackno
    if (unwrap(ackno, _isn, _next_seqno) > _next_seqno) 
       return;

    _window_size = window_size;
    _ack_seqno = unwrap(ackno, _isn, _next_seqno);
    bool received = false;

    while (!_outstanding.empty()) {
        TCPSegment seg = _outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= _ack_seqno) {
            _outstanding.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
            received = true;
        } else {
            break;
        }
    }

    if (received) {
        _time = 0;
        _consecutive_retransmissions = 0;
        _rto = _initial_retransmission_timeout;
    }
    
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _time += ms_since_last_tick;
    // std::cout << ms_since_last_tick << ' ' << _time << '\n';

    if (_time >= _rto && !_outstanding.empty()) {
        _segments_out.push(_outstanding.front());
        _time = 0;
        if (_window_size) {
            _rto <<= 1;
            _consecutive_retransmissions++;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
