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
        _isn = WrappingInt32{seg.header().seqno};
        _base_ackno += 1; // 除了 payload 之外，SYN 和 FIN flag 各占据了一个 sequence number
        _syn_received = true;
    }
    // 如果还没有发送 SYN (反映为 _isn 还没有被初始化)；则连接还没建立，我们不应该接收数据
    if (!_syn_received) { 
        return; 
    }

    // Push any data, or end-of-stream marker, to the StreamReassembler.
    bool eof = false;
    // 只考虑第一次接收的 FIN flag
    if (!_fin_received && seg.header().fin == true) {
        eof = true;
        _fin_received = true;
    }

    // seqno 也存在相应的偏移: 
    // 在 seqno 大于 _isn 时，由于 syn 占据了一位 sequence number, 因此需要 -1
    // TODO: 这个做法还是不太好，比如说重复发送带有 SYN flag 的 segment, 似乎 -1 就不对了
    WrappingInt32 seq_number = seg.header().seqno;
    if (!seg.header().syn) {
        seq_number = seq_number - 1;
    }

    uint64_t index = unwrap(
        seq_number, 
        _isn, 
        _reassembler.assembled_end_index() // ack 作为 checkpoint
    );
    _reassembler.push_substring(seg.payload().copy(), index, eof);
    // 检查是否将所有 segment 组合完成了；如果是的话，_base_ackno + 1, 因为 FIN flag 占据一个 sequence number
    if (_reassembler.input_ended()) {
        _base_ackno += 1;
    }

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (_syn_received) { // 已经被初始化了
        // 加上 SYN 和 FIN 带来的 ack 偏移: _base_ackno
        return wrap(_reassembler.assembled_end_index(), _isn) + _base_ackno; 
    }
    // If the ISN hasn't been set, return an empty optional
    return {};
}


size_t TCPReceiver::window_size() const { 
    // distance between "first unassembled index (对应 ackno)" 和 "first unaccepatable index"
    // 整体的容量是 _capacity, 减去 buffer 里头还没有被读的数据，剩下的就是 reassembler 里头可以存储的数据，即上文所提的 distance.
    return _capacity - _reassembler.stream_out().buffer_size();
}
