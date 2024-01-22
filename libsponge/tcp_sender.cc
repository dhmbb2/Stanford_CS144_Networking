#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    uint64_t ret = 0;
    for (auto& t : _timers) {
        ret += t.seg().length_in_sequence_space();
    }
    return ret;
}

void TCPSender::fill_window() {
    // if syn is not sent yet
    if (_next_seqno == 0) {
        TCPSegment seg;
        seg.header().syn = true;
        _send_segment(seg);
        return;
    }

    while (!_stream.buffer_empty() && !_window_size) {
        size_t send_len = min({
            static_cast<size_t>(_window_size), 
            TCPConfig::MAX_PAYLOAD_SIZE, 
            _stream.buffer_size()
        });
        TCPSegment seg;
        seg.payload() = _stream.read(send_len);
        if (_stream.input_ended()) 
            seg.header().fin = true;
        _send_segment(seg);
    }

    // if window size = 0, we need to treat it as one
    if (!_window_size) {
        TCPSegment seg;
        if (_stream.input_ended()) {
            seg.header().fin = true;
            _send_segment(seg);
        } else {
            seg.payload() = _stream.read(1);
            _send_segment(seg);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;

    vector<Timer> new_timers;
    for (auto& t : _timers) {
        if (t.index() + t.seg().length_in_sequence_space() <= unwrap(ackno, _isn, _next_seqno)) {
            _consecutive_retransmissions = 0;
            _rto = _initial_retransmission_timeout;
        } else {
            new_timers.push_back(t);
        }
    }
    _timers = new_timers;
    
    if (window_size != 0)
        fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _time += ms_since_last_tick;

    Timer& seg_to_resend = _timers[0];
    uint64_t min_index = std::numeric_limits<uint64_t>::max();

    for (auto& t : _timers) {
        t.tick(ms_since_last_tick);
        if (t.time() > t.rto() && t.index() < min_index) {
            seg_to_resend = t;
            min_index = t.index();
        }
    }
    
    if (min_index != std::numeric_limits<uint64_t>::max()) {
        _segments_out.push(seg_to_resend.seg());
        ++_consecutive_retransmissions;
        _rto <<= 1;
        seg_to_resend.set_rto(_rto);
        seg_to_resend.reset();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
