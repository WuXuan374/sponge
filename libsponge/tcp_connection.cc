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
    if (!active()) {
        return;
    }

    // if (seg.header().syn && _receiver.ackno().has_value()) {
    //     return;
    // }

    if (_connection_alive_ms != SIZE_MAX) {
        _timepoint_last_segment_received = _connection_alive_ms;
    }
    
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        // TODO: 如何终止连接?
        return;
    }

    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(
            seg.header().ackno,
            seg.header().win
        );
    }

    size_t prev_size = _sender.segments_out().size();
    _sender.fill_window();
    
    // _segments_out 才能反映是否发送了 segment (即使是空的 segment)
    if (_sender.segments_out().size() == prev_size) {
        // 对于非空的 incoming segment (占据至少一个 sequence number), 至少要回复一个 segment
        // TODO: invalid sequence number 怎么弄
        // 发送端会发送比 ack 小的 seqno
        // if (
        //     seg.length_in_sequence_space() > 0 ||
        //     invalid_sequence_number(seg.header().seqno)
        // )
        if (seg.length_in_sequence_space() > 0) {
            _sender.send_empty_segment();
        }
        
    }
    // 真正把数据传递出去
    push_segments_out();
}

//! 并不是直接维护一个状态变量
//! 根据注释里头的要求来写
bool TCPConnection::active() const { 
    return (
        !(_receiver.stream_out().eof() || _receiver.stream_out().error()) 
        || !(_sender.stream_in().eof() || _sender.stream_in().error()) 
        || _linger_after_streams_finish
    ); 

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
    if (!active()) {
       return; 
    }
    if (_connection_alive_ms == SIZE_MAX) {
        _connection_alive_ms = 0;
    } else {
        _connection_alive_ms += ms_since_last_tick;
    }
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        // abort the connection, send a reset segment to the peer
        // TODO: close connection
        // TODO: empty segment 怎么带上 RST flag?
        // _sender.send_empty_segment();
        _need_sent_rst = true;
    }
    push_segments_out();
}

void TCPConnection::end_input_stream() {
    //! 不能再写入这个 byte stream, 但是 sender 仍然可以从中读数据
    if (!active()) {
        return;
    }
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    // 需要发送一个 SYN segment
    _connection_alive = true;
    if (_sender.next_seqno_absolute() == 0) {
        // _sender.send_empty_segment();
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

bool TCPConnection::invalid_sequence_number(WrappingInt32 seqno) {
    // 对应 tcp_sender.cc L132
    if (_receiver.ackno().has_value()) {
        return (_receiver.ackno().value() - seqno) == 1;
    } 
    return false;
}

void TCPConnection::push_segments_out() {
    // TODO: 维护 _need_sent_rst
    size_t available_window_size = _receiver.window_size();
    while (!_sender.segments_out().empty() && available_window_size > 0) {
        TCPSegment tcp_seg = _sender.segments_out().front();
        // 维护 ACK flag, receiver 这边的 ackno 和 window_size
        if (_receiver.ackno().has_value()) {
            tcp_seg.header().ack = true;
            tcp_seg.header().ackno = _receiver.ackno().value();
        }
        tcp_seg.header().win = _receiver.window_size();
        // TODO: 那么，什么时候不再发送 RST 呢?
        // if (_need_sent_rst) {
        //     tcp_seg.header().rst = true;
        // }
        if (tcp_seg.length_in_sequence_space() > available_window_size) {
            // 超过 window size, 这个 segment 发送失败
            break;
        } else {
            // TCPConnection 的 segments_out
            _segments_out.push(tcp_seg);
            // 维护相应状态
            _sender.segments_out().pop();
            available_window_size -= tcp_seg.length_in_sequence_space();
        }
    }
    check_connection_state();
}

//! TODO: 什么时候进行这个状态的检查？
//! push_segment_out 中进行检查；tick() 也会调用 push_segment_out
void TCPConnection::check_connection_state() {
    if (!_connection_alive) {
        return;
    }
    if (_receiver.stream_out().eof() && (!_sender.stream_in().eof())) {
        _linger_after_streams_finish = false;
    }
    if (!_linger_after_streams_finish) {
        // inbound stream has been fully assembled and has ended
        if (_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended()) {
            // outbound stream has been fully acknowledged
            if (_sender.bytes_in_flight() == 0) {
                _connection_alive = false;
            }
        }
    } else {
        if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _connection_alive = false;
        }
    }
    if (!_connection_alive) {
        // 本次调用发生了状态的变化
        // 发送 FIN 报文
        _sender.send_empty_segment();
        push_segments_out();
    }
}

bool TCPConnection::receiver_in_syn_recv() {
    return _receiver.ackno().has_value() && (!_receiver.stream_out().input_ended());
}