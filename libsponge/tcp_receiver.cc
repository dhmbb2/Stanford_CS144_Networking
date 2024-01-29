#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();

    if (!header.syn && _isn == std::nullopt) 
        return;
    
    std::string data = seg.payload().copy();

    if (header.syn && _isn == std::nullopt) {
        _isn = header.seqno;
        if (header.fin && !_is_fin)
            _is_fin = true;
        _reassembler.push_substring(data, 0, header.fin);
        return;
    }

    if (header.fin && !_is_fin)
        _is_fin = true;

    size_t index = unwrap(header.seqno, *_isn, _reassembler.next_index());

    // is the segment's seqno is larger than window size, just ignore it
    if (index > _reassembler.next_index() + window_size())
        return;

    // corner case where syn's seqno has value
    if (index == 0) 
        data = (data.length() < 2) ? "" : data.substr(1);
    else 
        index -= 1; // minus syn's seqno

    _reassembler.push_substring (
        data, 
        index, 
        header.fin
    );

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_isn == std::nullopt)
        return std::nullopt;
    return wrap(_reassembler.next_index() + 1 + (_reassembler.empty() && _is_fin), *_isn);
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();
}