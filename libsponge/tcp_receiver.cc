#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! 可以这样认为: 第一个 sequence number 被 SYN 占据；最后一个 sequence number 被 FIN 占据
void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 注意: 收到过 FIN 之后仍然可以接收报文; ByteBuffer 关闭之后，才不能够接收报文
    if (_reassembler.stream_out().input_ended()) {
        return;
    }

    // Set the Initial Sequence Number if necessary.
    // 注意: 如果 ISN 已经被初始化过了，不能重复初始化
    if (!_syn_received && seg.header().syn == true) {
        _isn = WrappingInt32{seg.header().seqno + 1}; // 第一个 sequence number 被 SYN 占据
        _syn_received = true;
    }
    // 如果还没有发送 SYN，则连接还没建立，我们不应该接收数据
    if (!_syn_received) { 
        return; 
    }
    // 不合法的 sequence number: 比 _isn 还小
    if (!check_seqno(seg)) {
        return;
    }

    // Push any data, or end-of-stream marker, to the StreamReassembler.
    bool eof = false;
    // 重复收到 FIN 报文？就以第一个为准
    if (!_fin_received && seg.header().fin == true) {
        eof = true;
        _fin_received = true;
    }

    // 对于带有 SYN 的报文，数据的实际起始序号，应该是报文中的 seqno + 1
    uint64_t index;
    if (seg.header().syn) {
        index = unwrap(
            WrappingInt32{seg.header().seqno+1}, 
            _isn, 
            _reassembler.assembled_end_index() // ack 作为 checkpoint
        );
    } else {
        index = unwrap(
            seg.header().seqno, 
            _isn, 
            _reassembler.assembled_end_index() // ack 作为 checkpoint
        );
    }
    _reassembler.push_substring(seg.payload().copy(), index, eof);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    //! 这个函数需要处理 assembled_end_index() 和 ackno 之间的转换问题；
    //! 转换问题: 就目前所知， SYN 和 FIN flag 各自占据一个 sequence number, 这不会体现在 assembled_end_index() 中，需要我们手动补充
    if (_syn_received) { // 已经被初始化了
        // 如果所有数据都重组好了，则 FIN 标志会占据一个 sequence number, 故 ackno 需要在 reassembler 的基础上 + 1
        size_t base_ackno = 0; 
        if (_reassembler.stream_out().input_ended()) {
            base_ackno += 1; 
        }
        return wrap(_reassembler.assembled_end_index(), _isn) + base_ackno; 
    }
    // If the ISN hasn't been set, return an empty optional
    return {};
}


size_t TCPReceiver::window_size() const { 
    // distance between "first unassembled index (对应 ackno)" 和 "first unaccepatable index"
    // 整体的容量是 _capacity, 减去 buffer 里头还没有被读的数据，剩下的就是 reassembler 里头可以存储的数据，即上文所提的 distance.
    return _capacity - _reassembler.stream_out().buffer_size();
}

bool TCPReceiver::check_seqno(TCPSegment seg) const {
    WrappingInt32 seqno = seg.header().seqno;
    if (_isn - seqno > 1) {
        return false;
    }
    if (_isn - seqno == 1) {
        // isn 是不考虑 syn flag 的
        return seg.header().syn;
    }
    return true;
}