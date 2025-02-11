#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.
// TODO: 要检查一下 connection alive

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const { 
    return _connection_alive_ms - _timepoint_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if (!active()) return;
    if (seg.header().rst) {
        // 收到 RST, 直接关闭连接，不需要再发送RST
        unclean_shutdown();
        return;
    }

    if (seg.header().ack) {
        _sender.ack_received(
            seg.header().ackno,
            seg.header().win
        );
    }
    _receiver.segment_received(seg);

    if (_connection_alive_ms != SIZE_MAX) {
        _timepoint_last_segment_received = _connection_alive_ms;
    } else {
        _connection_alive_ms = 0;
        _timepoint_last_segment_received = 0;
    }

    bool send_empty = true; // 是否需要额外补充一个 segment

    // 已经发送了 FIN, 这时候收到对面的 FIN, 应该回复一个 ack
    if (seg.header().fin && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2) {
        // 由于此时stream 已经 eof 了，所以 fill window 无法回复，得手动补充一个 ack
        TCPSegment tcp_seg;
        tcp_seg.header().ack = true;
        tcp_seg.header().ackno = _receiver.ackno().value();
        _segments_out.push(tcp_seg);
        send_empty = false; // 已经回复了一个 segment, 无需补充
    }
    
    
    // 收到了 SYN, 并且 receiver 位于 LISTEN 状态; 则发送一个 SYN
    if (seg.header().syn && !(_receiver.ackno().has_value())) {
        _sender.send_empty_segment(true);
        send_empty = false; // 已经回复了一个 segment, 无需补充
    }
    
    
    if (_receiver.ackno().has_value()) { // 说明已经收到了 SYN, 建立了连接
        size_t prev_size = _sender.segments_out().size();
        _sender.fill_window();
        
        // _segments_out 才能反映是否发送了 segment (即使是空的 segment)
        if (_sender.segments_out().size() == prev_size && send_empty) {
            // 1. seg 占据了 sequence number; 2. "keep-alive" segment
            if (seg.length_in_sequence_space() > 0 || seg.header().seqno == _receiver.ackno().value()-1) {
                _sender.send_empty_segment();
            }
        }
    }
    
    // 真正把数据传递出去
    push_segments_out();
}

//! 根据注释里头的要求来写
bool TCPConnection::active() const { 
    return _active;
}

//! 这个函数应该是向 sender 的 ByteStream 写入，然后 sender 自己从中读取数据，并发送 TCPSegment
size_t TCPConnection::write(const string &data) {
    if (!active()) {
        return 0;
    }
    size_t len = _sender.stream_in().write(data);
    _sender.fill_window();
    push_segments_out();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    check_connection_state();
    if (_connection_alive_ms == SIZE_MAX) {
        _connection_alive_ms = 0;
    } else {
        _connection_alive_ms += ms_since_last_tick;
    }
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        // 除了关闭连接，还需要发送带有 RST 的 segment
        unclean_shutdown();
        bool send_succeed = push_segments_out(true);
        // 队列中没有可发送的 segment, 需要另外发送一个 segment, 并带上 RST
        if (!send_succeed) {
            _sender.send_empty_segment();
            push_segments_out(true);
        }
    } else {
        push_segments_out();
    }
    
}

void TCPConnection::end_input_stream() {
    //! 不能再写入这个 byte stream, 但是 sender 仍然可以从中读数据
    if (!active()) {
        return;
    }
    _sender.stream_in().end_input();
    // 此时可能会发送 FIN
    _sender.fill_window();
    push_segments_out(); 
}

void TCPConnection::connect() {
    // 检查是否位于 CLOSED 状态
    if (_sender.next_seqno_absolute() == 0) {
        // 带上了 SYN 标记
        _sender.send_empty_segment(true);
        push_segments_out();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

bool TCPConnection::push_segments_out(bool rst_flag) {
    bool flag = false;
    while (!_sender.segments_out().empty()) {
        TCPSegment tcp_seg = _sender.segments_out().front();
        // 维护 ACK flag, receiver 这边的 ackno 和 window_size
        if (_receiver.ackno().has_value()) {
            tcp_seg.header().ack = true;
            tcp_seg.header().ackno = _receiver.ackno().value();
        }
        tcp_seg.header().win = _receiver.window_size();
        if (rst_flag) {
            tcp_seg.header().rst = true;
        }
        // TCPConnection 的 segments_out
        _segments_out.push(tcp_seg);
        // 维护相应状态
        _sender.segments_out().pop();
        flag = true;
    }
    check_connection_state();
    return flag;
}

//! push_segment_out 中进行检查；tick() 也会调用 push_segment_out
void TCPConnection::check_connection_state() {
    if (!active()) {
        // 重新建立连接: Receiver 位于 `SYN_RECV`, Sender 位于 `SYN_SENT`, 视作重新建立了连接
        // 注意不能处于 error 状态
        if (!(_receiver.stream_out().error()) && !(_sender.stream_in().error())) {
            if (_receiver.ackno().has_value() && !(_receiver.stream_out().input_ended()) && _sender.next_seqno_absolute() > 0) {
                _active = true;
            }
        }
        return; 
    }
    
    // inbound stream ended, outbound stream not EOF (对面先发送 FIN, 则我发送 FIN 之后可以直接关闭)
    if (_receiver.stream_out().input_ended() && !(_sender.stream_in().eof())) {
        _linger_after_streams_finish = false;
    }
    // inbound stream ended and fully assembled & outbound stream ended and fully sent
    if (_receiver.stream_out().eof() && _receiver.unassembled_bytes() == 0 && _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && bytes_in_flight() == 0) {
        // 我已经发送了 FIN 并且收到了 ACK
        // 1. passive close (最开始我是收到 fin 的这一方) 2. 超时未收到消息
        if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            clean_shutdown();
        }
    }
}

void TCPConnection::unclean_shutdown() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::clean_shutdown() {
    _active = false;
}