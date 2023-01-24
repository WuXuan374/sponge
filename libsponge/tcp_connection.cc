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
    // 首先检查是否存在连接
    if (!active()) {
        return;
    }
    if (_connection_alive_ms != SIZE_MAX) {
        _timepoint_last_segment_received = _connection_alive_ms;
    }
    
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        // TODO: kill the connection, 可能需要整理成一个函数
        _connection_alive = false;
        // 接下来发送的 segment 要带上 _rst_flag
        _need_sent_rst = true;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(
            seg.header().ackno,
            seg.header().win
        );
    }
    uint64_t prev_bytes_in_flight = _sender.bytes_in_flight();
    _sender.fill_window();
    // 对于非空的 incoming segment (占据至少一个 sequence number), 至少要回复一个 segment
    if (_sender.bytes_in_flight() == prev_bytes_in_flight) {
        // TODO: invalid sequence number 怎么弄
        // 发送端会发送比 ack 小的 seqno
        if (
            seg.length_in_sequence_space() > 0 ||
            invalid_sequence_number(seg.header().seqno)
        )
        _sender.send_empty_segment();
    }
}

bool TCPConnection::active() const { 
    return _connection_alive;
}

//! 这个函数应该是向 sender 的 ByteStream 写入，然后 sender 自己从中读取数据，并发送 TCPSegment
size_t TCPConnection::write(const string &data) {
    if (!active()) {
        return 0;
    }
    size_t len = _sender.stream_in().write(data);
    _sender.fill_window();
    // TODO: ACK flag 的设置如何实现？并且需要把 segment 写入 _segments_out ?
    push_segments_out();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if (!active()) {
       return; 
    }
    if (_connection_alive_ms == SIZE_MAX) {
        _connection_alive_ms = 0;
    } else {
        _connection_alive_ms += ms_since_last_tick;
    }
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // abort the connection, send a reset segment to the peer
        // TODO: close connection
        // TODO: empty segment 怎么带上 RST flag?
        _sender.send_empty_segment();
        _need_sent_rst = true;
    }
}

void TCPConnection::end_input_stream() {
    //! 不能再写入这个 byte stream, 但是 sender 仍然可以从中读数据
    if (!active()) {
        return;
    }
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    _connection_alive = true;
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

bool TCPConnection::invalid_sequence_number(WrappingInt32 seqno) {
    if (_receiver.ackno().has_value()) {
        return _receiver.ackno().value() - seqno > 0;
    }
    return false;
}

void TCPConnection::push_segments_out() {
    // TODO: 维护 _need_sent_rst
    size_t available_window_size = _receiver.window_size();
    while (!_sender.segments_out().empty() && available_window_size > 0) {
        TCPSegment tcp_seg = _sender.segments_out().front();
        // 维护 ACK flag
        if (_receiver.ackno().has_value()) {
            tcp_seg.header().ack = true;
        } 
        if (_need_sent_rst) {
            tcp_seg.header().rst = true;
        }
        if (tcp_seg.length_in_sequence_space() > available_window_size) {
            // 超过 window size, 这个 segment 发送失败
            break;
        } else {
            _segments_out.push(tcp_seg);
            // 维护相应状态
            _sender.segments_out().pop();
            available_window_size -= tcp_seg.length_in_sequence_space();
        }
    }
}
