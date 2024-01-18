#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32((isn.raw_value() + n) % (1ul << 32));
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t tmp = 0;
    uint64_t ret = 0;

    if (n - isn < 0) 
        tmp = (1ul << 32) + (n - isn);
    else 
        tmp = n - isn;

    if (tmp >= checkpoint)
        return tmp;
    
    uint64_t base = ((checkpoint >> 32) << 32) - (1ul << 32);

    if (base + tmp < checkpoint && base + tmp + (1ul << 32) >= checkpoint)
        ret = base + tmp;
    else 
        ret = base + tmp + (1ul << 32);
    return (checkpoint - ret > ret + (1ul << 32) - checkpoint) ? ret + (1ul << 32) : ret;
}