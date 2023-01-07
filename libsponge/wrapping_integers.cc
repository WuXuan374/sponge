#include "wrapping_integers.hh"
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

uint64_t RANGE = 1ul << 32;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t remainder = (n + isn.raw_value()) % RANGE;
    return WrappingInt32{remainder};
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
    uint64_t absolute;
    if (n.raw_value() < isn.raw_value()) {
        absolute = n.raw_value() + RANGE - isn.raw_value();
    } else {
        absolute = n.raw_value() - isn.raw_value();
    } // absolute: [0, RANGE-1]
    if (checkpoint > absolute) {
        absolute += RANGE * ((checkpoint - absolute) / RANGE);
        if ((checkpoint - absolute) % RANGE > RANGE / 2) {
            absolute += RANGE;
        }
    }
    return absolute;
}
