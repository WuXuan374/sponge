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

//! TODO: _next_seqno: 每次发送 segments 之后，需要维护
//! TODO: _bytes_in_flight: 每次发送 segments 之后，或者收到 ack 之后，需要维护

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _timer_start{SIZE_MAX}
    , _initial_retransmission_timeout{retx_timeout}
    , _retranmission_timeout{retx_timeout} 
    , _stream(capacity) {}

//! 应该是一个 O(1) 复杂度的函数，不能够去遍历得到; 应该在每次发送 segments 时维护 _bytes_in_flight
uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight;
}

void TCPSender::fill_window() {
    // 文档中说明，当接收方的 window size 为 0 时，要将其视作 1
    // 发送一个可能被 reject 的 segment, 从而得到 window size 的更新
    uint64_t window_size = _receiver_window_size > 1 ? _receiver_window_size : 1;
    send_segments(_next_seqno, std::min(_stream.buffer_size(), window_size));
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    _receiver_window_size = window_size;
    uint64_t absolute_ackno = unwrap(ackno, _isn, _receiver_ackno != SIZE_MAX ? _receiver_ackno : 0);
    if (_receiver_ackno == SIZE_MAX || absolute_ackno > _receiver_ackno) {
        // receiver a bigger ackno, indicating the receipt of new data
        _receiver_ackno = absolute_ackno;
        _retranmission_timeout = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;

        // 如果找到被 ack 的报文，注意维护 _outstanding_segments
        auto it = _outstanding_segments.begin();
        while (it != _outstanding_segments.end()) {
            TCPSegment seg = *it;
            if (unwrap(seg.header().seqno, _isn, _next_seqno) + seg.length_in_sequence_space() <= _receiver_ackno) {
                _bytes_in_flight -= seg.length_in_sequence_space();
                auto next_it = std::next(it, 1);
                _outstanding_segments.erase(it);
                it = next_it;
            } else {
                it++;
            }
        }
        if (_outstanding_segments.empty()) {
            // 所有的 outstanding segments 都 ack 了, 则清除计时器（将其设为初始值）
            _timer_start = SIZE_MAX;
        } else {
            // 仍存在 outstading segments, 则重启计时器
            _timer_start = _ms_alive;
        }
    }

    // 调用 fill_window(), 此时 _next_seqno, buffer size, window size 都可能发生变化了
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//! 在这个函数中，_outstanding_segments 中存储的都是还没 ack 的 segments; 统一在 ack_received 函数中处理被 ack 的 segments
// TODO: 这个 _ms_alive 和 _timer_start 是不是也得考虑越界的情况
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
            resend_segment(seg_min);
        }
    }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    // 同样要有 SYN 和 FIN 的检查
    TCPSegment tcp_seg;
    tcp_seg.header().seqno = wrap(_next_seqno, _isn);
    if (_next_seqno == 0) {
        tcp_seg.header().syn = true;
    }
    if (_stream.input_ended() && _stream.buffer_empty()) {
        tcp_seg.header().fin = true;
    }
    // 把 segment 写入 _segments_out, 就视作完成了 segment 的发送
    _segments_out.push(tcp_seg);

    // 检查 segment 的长度，如果大于 0（存在 SYN 或 FIN），则需要加入 _outstand_segments 中，并维护 seqno 和 _bytes_in_flight
    size_t seg_len = tcp_seg.length_in_sequence_space();
    if (seg_len > 0) {
        _next_seqno += seg_len;
        _bytes_in_flight += seg_len;
        _timer_start = _ms_alive;
        _outstanding_segments.insert(tcp_seg);
    }
    
    // 如果是 empty segment, 则不需要后续处理了
}

//! \param[in] start_seqno (absolute sequence number)
//! \param[in] data_len length of payload
//! data_len 可能为 0; 如果最后发现 segment 长度为 0, 则不发送
//! 有可能 payload 长度为 0, 但是带有 SYN/FIN flag, 这种仍然要发送
void TCPSender::send_segments(uint64_t start_seqno, uint64_t data_len) {
    // 特殊情况
    if (data_len == 0) {
        TCPSegment tcp_seg;
        if (start_seqno == 0) {
            tcp_seg.header().syn = true;
        }
        tcp_seg.header().seqno = wrap(start_seqno, _isn);
        if (_stream.input_ended() && _stream.buffer_empty()) {
            tcp_seg.header().fin = true;
        }
        // 只发送非空的 segment
        if (tcp_seg.length_in_sequence_space() > 0) {
            _segments_out.push(tcp_seg);
            start_seqno += tcp_seg.length_in_sequence_space();
            // 添加计时器，加入 _outstanding_segments
            _timer_start = _ms_alive;
            _outstanding_segments.insert(tcp_seg);
            // 维护 _next_seqno 和 _bytes_in_flight
            _bytes_in_flight += tcp_seg.length_in_sequence_space();
            _next_seqno = std::max(start_seqno, _next_seqno);
        }
    } else {
        while (data_len > 0) {
            TCPSegment tcp_seg;
            if (start_seqno == 0) {
                tcp_seg.header().syn = true;
            }
            tcp_seg.header().seqno = wrap(start_seqno, _isn);
            uint64_t payload_len = std::min(data_len, TCPConfig::MAX_PAYLOAD_SIZE);
            data_len -= payload_len;
            tcp_seg.payload() = Buffer(_stream.read(payload_len));
            if (_stream.input_ended() && _stream.buffer_empty()) {
                tcp_seg.header().fin = true;
            }
            _segments_out.push(tcp_seg);
            start_seqno += tcp_seg.length_in_sequence_space();
            
            // 添加计时器，加入 _outstanding_segments
            _timer_start = _ms_alive;
            _outstanding_segments.insert(tcp_seg);
            // 维护 _next_seqno 和 _bytes_in_flight
            _bytes_in_flight += tcp_seg.length_in_sequence_space();
            _next_seqno = std::max(start_seqno, _next_seqno);
        }
    }
}

//! 重传一个 segment, 应该不需要维护 _next_seqno 和 _bytes_in_flight 了
//! TODO: 是否还需要检查 window size?
void TCPSender::resend_segment(TCPSegment seg) {
    _segments_out.push(seg);
    // reset timer
    _timer_start = _ms_alive;
    // Set 本身是有去重能力的
    _outstanding_segments.insert(seg);
}
// TODO: 我感觉计时器不是全局的，有问题这里
