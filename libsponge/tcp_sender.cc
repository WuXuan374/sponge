#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

// TODO: _outstanding_segments.find() 是否奏效？

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retranmission_timeout{retx_timeout} 
    , _stream(capacity) {}

//! 应该是一个 O(1) 复杂度的函数，不能够去遍历得到; 应该在每次发送 segments 时维护 _bytes_in_flight
uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight;
}

//! 文档中提到，_receiver_window_size 为 0 时，将其视作 1
uint64_t TCPSender::sender_window_size() const {
    uint64_t recv_win_size = (_receiver_window_size == 0) ? 1: _receiver_window_size;
    if (recv_win_size >= _bytes_in_flight) {
        return recv_win_size - _bytes_in_flight;
    } else {
        return 0;
    }
}

//! 每次收到 ack 之后调用该方法，故可以确认该方法中的 _receiver_window_size 是最新的，无需再减去 bytes_in_flight
void TCPSender::fill_window() {
    // 文档中说明，当接收方的 window size 为 0 时，要将其视作 1
    // 发送一个可能被 reject 的 segment, 从而得到 window size 的更新
    //! 阅读测试代码，发现 fill_window 会被单独调用，因此我们在 ack_received() 中不需要手动调用该函数
    send_segments(_next_seqno, std::min(_stream.buffer_size(), sender_window_size()));
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t absolute_ackno = unwrap(ackno, _isn, _receiver_ackno != SIZE_MAX ? _receiver_ackno : 0);
    // 忽略 ackno 不合理的报文 (超过了 _next_seqno)
    if (absolute_ackno > _next_seqno) {
        return;
    }

    _receiver_window_size = window_size;
    
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

    // 检查是否发送了 FIN, 并收到对应的 ack
    if (_stream.input_ended() && _stream.buffer_empty()) {
        if (_next_seqno == _stream.bytes_written() + 2 && _next_seqno == _receiver_ackno) {
            _fin_acked = true;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//! 在这个函数中，_outstanding_segments 中存储的都是还没 ack 的 segments; 统一在 ack_received 函数中处理被 ack 的 segments
// TODO: 这个 _ms_alive 和 _timer_start 是不是也得考虑越界的情况
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _ms_alive += ms_since_last_tick;
    // 如果有计时器，并且超时了
    if (_timer_start != SIZE_MAX && _ms_alive - _timer_start >= _retranmission_timeout) {
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

//! 数据长度非空时，注意维护 _receiver_window_size
//! TODO: 需要传一个 invalid sequence number, 那么就传一个比 ack 更小的吧
void TCPSender::send_empty_segment(bool syn) {
    if (!check_seqno(_next_seqno)) {
        return;
    }

    // TCPSegment tcp_seg = construct_segment(_next_seqno, 0);
    // 约定这个特别的 segment, 其 seqno 为 receiver_ackno-1; 兜底的 absolute value 为 0(对应还没有建立连接，发送 SYN)
    TCPSegment tcp_seg = construct_segment(
        _receiver_ackno != SIZE_MAX ? (_receiver_ackno - 1) : 0,
        0,
        syn
    );
    size_t seg_len = tcp_seg.length_in_sequence_space();
    
    // window 足够 && 不在 _outstanding 队列中
    if (sender_window_size() >= seg_len && _outstanding_segments.find(tcp_seg) == _outstanding_segments.end()) {
        // 这里头多了一个长度为 0 的 segment, 不加入 _outstanding, 但是仍然发送的规则，因此单独出来
        _segments_out.push(tcp_seg);
        if (seg_len > 0) {
            if (_timer_start == SIZE_MAX) {
                // segment 非空，并且没有正在运行的计时器
                _timer_start = _ms_alive;
            }
            _outstanding_segments.insert(tcp_seg);
            _next_seqno += seg_len;
            _bytes_in_flight += seg_len;
        }
    }
}

//! \param[in] start_seqno (absolute sequence number)
//! \param[in] data_len length of payload
//! data_len 可能为 0; 如果最后发现 segment 长度为 0, 则不发送
//! 有可能 payload 长度为 0, 但是带有 SYN/FIN flag, 这种仍然要发送
void TCPSender::send_segments(uint64_t start_seqno, uint64_t data_len) {
    if (!check_seqno(start_seqno)) {
        return;
    }
    // 特殊情况
    if (data_len == 0) {
        TCPSegment tcp_seg = construct_segment(start_seqno, 0);
        size_t seg_len = tcp_seg.length_in_sequence_space();

        // segment 非空 && window 足够 && 不在 _outstanding 队列中
        if (seg_len > 0 && sender_window_size() >= seg_len && _outstanding_segments.find(tcp_seg) == _outstanding_segments.end()) {
            _send_segment(tcp_seg, start_seqno);
            start_seqno += seg_len;
        }
    } else {
        while (data_len > 0) {
            uint64_t payload_len = std::min(data_len, TCPConfig::MAX_PAYLOAD_SIZE);
            data_len -= payload_len;
            TCPSegment tcp_seg = construct_segment(start_seqno, payload_len);
            size_t seg_len = tcp_seg.length_in_sequence_space();

            // window 足够 && 不在 _outstanding 队列中
            if (sender_window_size() >= seg_len && _outstanding_segments.find(tcp_seg) == _outstanding_segments.end()) {
                _send_segment(tcp_seg, start_seqno);
                start_seqno += seg_len;
            }
        }
    }
}

//! 重传一个 segment, 应该不需要维护 _next_seqno 和 _bytes_in_flight 了
//! 是否还需要检查 window size? 我认为是不需要的，之前发送该 segment 时就已经考虑了其对于 window 的占用；现在重传，并没有多占据 window
void TCPSender::resend_segment(TCPSegment seg) {
    if (_fin_acked) {
        return;
    }
    // 调用处已经确保了这个 seg 来自 _outstanding_segments, 故不需要再次加入
    // 同时，我们发现 set 对于 TCPSegment 并不能去重
    _segments_out.push(seg);
    // reset timer
    _timer_start = _ms_alive;
}

TCPSegment TCPSender::construct_segment(uint64_t seqno, uint64_t payload_len, bool syn) {
    TCPSegment tcp_seg;
    tcp_seg.header().seqno = wrap(seqno, _isn);
    if (syn && seqno == 0) {
        tcp_seg.header().syn = true;
    }
    
    // payload 的优先级次之
    if (tcp_seg.length_in_sequence_space() + payload_len <= sender_window_size()) {
        tcp_seg.payload() = Buffer(_stream.read(payload_len));
    }

    // FIN 的优先级最低，在 window 还有剩余的情况下才添加该标记
    if (_stream.input_ended() && _stream.buffer_empty() && tcp_seg.length_in_sequence_space() < sender_window_size()) {
        tcp_seg.header().fin = true;
    }
    return tcp_seg;
}

bool TCPSender::check_seqno(uint64_t seqno) {
    // 已经发送了 FIN 并收到了对应的 ACK，不应该再发送数据了
    if (_fin_acked) {
        return false;
    }
    // 不合法的 seqno (哪怕是发送带 FIN 的 segment, sender 的 seqno 最多是 bytes_written()+1)
    if (seqno >= _stream.bytes_written() + 2) {
        return false;
    }
    return true;
}

void TCPSender::_send_segment(TCPSegment seg, uint64_t start_seqno) {
    size_t seg_len = seg.length_in_sequence_space();
    _segments_out.push(seg);
    _outstanding_segments.insert(seg);
    _next_seqno = std::max(start_seqno + seg_len, _next_seqno);
    _bytes_in_flight += seg_len;
    if (seg_len > 0 && _timer_start == SIZE_MAX) {
        _timer_start = _ms_alive;
    }
}