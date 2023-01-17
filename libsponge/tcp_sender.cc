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

//! outstanding segments 相关的，后面统一处理

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
    , _receiver_window_size(SIZE_MAX) 
    , _retranmission_timeout{retx_timeout} 
    , _timer_start(SIZE_MAX) {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {
    if (!_stream.buffer_empty()) {
        send_segments(_next_seqno, std::min(_stream.buffer_size(), _receiver_window_size));
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    _receiver_window_size = window_size;
    if (ackno.raw_value() > _next_seqno) {
        _next_seqno = ackno.raw_value();
    }
    // 遍历 _outstanding_segments, 尝试移除已经被 ack 的
    bool segment_acked = false;
    for (auto it = _outstanding_segments.begin(); it != _outstanding_segments.end(); it++) {
        TCPSegment seg = *it;
        if (seg.header().seqno.raw_value() + seg.length_in_sequence_space() <= ackno.raw_value()) {
            _outstanding_segments.erase(it);
            segment_acked = true;
        }
    }
    // 有 _outstanding_segments 被成功移除
    if (segment_acked) {
        _retranmission_timeout = _initial_retransmission_timeout;
        _timer_start = _ms_alive;
        _consecutive_retransmissions = 0;
    }

    // 调用 fill_window(), 此时 _next_seqno, buffer size, window size 都可能发生变化了
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _ms_alive += ms_since_last_tick;
    // 如果有计时器，并且超时了
    if (_timer_start != SIZE_MAX && _ms_alive - _timer_start > _retranmission_timeout) {
        if (_outstanding_segments.empty()) {
            cout << "ERROR tick(): " << "timeout, but _outstanding_segments is empty" << endl;
        } else {
            if (_receiver_window_size > 0) {
                _consecutive_retransmissions += 1;
                _retranmission_timeout = 2 * _retranmission_timeout;
            }

            // 重传 seqno 最小的 Segment
            TCPSegment seg_min = *(_outstanding_segments.begin());
            send_segment(seg_min);
        }
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment tcp_seg;
    tcp_seg.header().seqno = wrap(_next_seqno, _isn);
    // 检查一下
    if (!tcp_seg.length_in_sequence_space() == 0) {
        cout << "ERROR send_empty_segment(); length: " << tcp_seg.length_in_sequence_space() << endl;
    }
    // 把 segment 写入 _segments_out, 就视作完成了 segment 的发送
    _segments_out.push(tcp_seg);
    // 发送 empty segment 时，不需要启动计时器, 也不需要将其写入 outstanding_segments 中
}

//! \param[in] start_seqno (absolute sequence number)
//! \param[in] data_len length of payload
//! TODO:函数调用处，需要完成参数的检查
void TCPSender::send_segments(uint64_t start_seqno, uint64_t data_len) {
    while (data_len > 0) {
        TCPSegment tcp_seg;
        if (start_seqno == 0) {
            tcp_seg.header().syn = true;
        }
        tcp_seg.header().seqno = wrap(start_seqno, _isn);
        uint64_t payload_len = std::min(data_len, TCPConfig::MAX_PAYLOAD_SIZE);
        data_len -= payload_len;
        tcp_seg.payload() = Buffer(_stream.read(payload_len));
        _segments_out.push(tcp_seg);
        start_seqno += tcp_seg.length_in_sequence_space();
        
        // 添加计时器，加入 _outstanding_segments
        _timer_start = _ms_alive;
        _outstanding_segments.insert(tcp_seg);
    }
}

void TCPSender::send_segment(TCPSegment seg) {
    _segments_out.push(seg);
    // Set 本身是有去重能力的
    _outstanding_segments.insert(seg);

    // reset timer
    _timer_start = _ms_alive;
}
