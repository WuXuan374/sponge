#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // Set the Initial Sequence Number if necessary.
    // 注意: 如果 ISN 已经被初始化过了，不能重复初始化
    if (!_syn_received && seg.header().syn == true) {
        _isn = seg.header().seqno;
        _syn_received = true;
    }
    // 如果还没有发送 SYN (反映为 _isn 还没有被初始化)；则连接还没建立，我们不应该接收数据
    // 如果已经收到了 FIN, 则不应该继续接收数据
    if (!_syn_received || _fin_received) { 
        return; 
    }

    // Push any data, or end-of-stream marker, to the StreamReassembler.
    bool eof = false;
    if (!_fin_received && seg.header().fin == true) {
        eof = true;
        _fin_received = true;
    }

    // seqno 也存在相应的偏移: 
    // 在 seqno 大于 _isn 时，由于 syn 占据了一位 sequence number, 因此需要 -1
    WrappingInt32 seq_number = seqno(seg);

    uint64_t index = unwrap(
        seq_number, 
        _isn, 
        _reassembler.assembled_end_index() // ack 作为 checkpoint
    );
    _reassembler.push_substring(seg.payload().copy(), index, eof);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    //! 这个函数需要处理 assembled_end_index() 和 ackno 之间的转换问题；
    //! 转换问题: 就目前所知， SYN 和 FIN flag 各自占据一个 sequence number, 这不会体现在 assembled_end_index() 中，需要我们手动补充
    if (_syn_received) { // 已经被初始化了
        // 加上 SYN 和 FIN 带来的 ack 偏移: _base_ackno
        size_t base_ackno = 1; // SYN 带来的 ack 偏移
        if (_reassembler.input_ended()) {
            base_ackno += 1; // 所有数据都重组好了，此时考虑 FIN 带来的 ack 偏移
        }
        return wrap(_reassembler.assembled_end_index(), _isn) + base_ackno; 
    }
    // If the ISN hasn't been set, return an empty optional
    return {};
}

WrappingInt32 TCPReceiver::seqno(const TCPSegment &seg) const {
    //! 转换问题: 就目前所知，SYN 和 FIN 会额外占据一个 sequence number; 这不会体现在 reassembler 的序号中，因此需要转换
    size_t base_seqno = 0;
    if (_syn_received) {
        base_seqno += 1;
        if (_fin_received) {
            base_seqno += 1;
        }
    }
    return WrappingInt32{seg.header().seqno - base_seqno};
}


size_t TCPReceiver::window_size() const { 
    // distance between "first unassembled index (对应 ackno)" 和 "first unaccepatable index"
    // 整体的容量是 _capacity, 减去 buffer 里头还没有被读的数据，剩下的就是 reassembler 里头可以存储的数据，即上文所提的 distance.
    return _capacity - _reassembler.stream_out().buffer_size();
}
