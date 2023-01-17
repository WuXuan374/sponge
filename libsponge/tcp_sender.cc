#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

//! 记住 TCPSegment 有以下内容需要特殊设置
//! sequence number, SYN flag, payload, FIN flag
//! 更新私有变量 _next_seqno

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _receiver_window_size(SIZE_MAX) {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {
    if (!_stream.buffer_empty()) {
        send_segments(std::min(_stream.buffer_size(), _receiver_window_size));
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { DUMMY_CODE(ackno, window_size); }

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {
    TCPSegment tcp_seg;
    tcp_seg.header().seqno = wrap(_next_seqno, _isn);
    // 检查一下
    if (!tcp_seg.length_in_sequence_space() == 0) {
        cout << "ERROR send_empty_segment(); length: " << tcp_seg.length_in_sequence_space() << endl;
    }
    // 把 segment 写入 _segments_out, 就视作完成了 segment 的发送
    _segments_out.push(tcp_seg);
}

void TCPSender::send_segments(uint64_t target_length) {
    while (target_length > 0) {
        TCPSegment tcp_seg;
        if (_next_seqno == 0) {
            tcp_seg.header().syn = true;
        }
        tcp_seg.header().seqno = wrap(_next_seqno, _isn);
        uint64_t payload_len = std::max(target_length, TCPConfig::MAX_PAYLOAD_SIZE);
        target_length -= payload_len;
        tcp_seg.payload() = Buffer(_stream.read(payload_len));
        _segments_out.push(tcp_seg);
        _next_seqno += tcp_seg.length_in_sequence_space();
    }
}
