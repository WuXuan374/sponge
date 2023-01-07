#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // Set the Initial Sequence Number if necessary.
    if (seg.header().syn == true) {
        _isn = std::make_optional<WrappingInt32>(seg.header().seqno);
    }
    // Push any data, or end-of-stream marker, to the StreamReassembler.
    bool eof = false;
    if (seg.header().fin == true) {
        eof = true;
    }
    uint64_t index = unwrap(
        seg.header().seqno, 
        _isn.value(), 
        0 // TODO:
    );
    _reassembler.push_substring(seg.payload().copy(), index, eof);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    // 需要在 stream_reassembler 里头，加一些 getter 方法，拿到 _assembled_end_index 等
    if (_isn.value_or(false) == false) {
        return {};
    }
    // return wrap()
 }

size_t TCPReceiver::window_size() const { return {}; }
