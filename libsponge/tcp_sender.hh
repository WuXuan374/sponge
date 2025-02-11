#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <set>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! total number of milliseconds, this TCPSender has been alive
    size_t _ms_alive{0};

    //! 计时器的开始时间
    size_t _timer_start{SIZE_MAX};

    //! 连续重传次数
    size_t _consecutive_retransmissions{0};

    //! TODO: 这里应该根据 absolute number 进行比较
    struct cmp {
      bool operator() (TCPSegment a, TCPSegment b) const {
        if (a.header().seqno.raw_value() < b.header().seqno.raw_value()) {
          return true;
        } else if (a.length_in_sequence_space() < b.length_in_sequence_space()){
          return true;
        }
        return false;
      }
    };

    set<TCPSegment, cmp> _outstanding_segments{};

    //! retransmission timer for the connection
    uint64_t _initial_retransmission_timeout;

    //! RTO 是可能变化的
    uint64_t _retranmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    //! 应该和 receiver 所发送的 ack 对应?
    uint64_t _next_seqno{0};

    //! 接收方的 window size, 每次收到 Receiver 发送的 segment 时，更新
    //! 文档中说，当 Sender 还没有收到 ack 时，将 receiver window size 假设成 1
    uint64_t _receiver_window_size{1};

    uint64_t sender_window_size() const;

    //! 同样存放 absolute sequence number 进行比较!
    uint64_t _receiver_ackno{SIZE_MAX};

    uint64_t _bytes_in_flight{0};

    //! 给出起始 seqno 和数据的整体长度
    //! 将数据转成1个或多个 TCPSegment, 进行发送
    void send_segments(uint64_t start_seqno, uint64_t data_len);

    //! 已经把 Segment 配置好了，只要将其写入 _segments_out 和 _outstanding_segments 即可
    void resend_segment(TCPSegment seg);

    //! 由于 window 的限制，segment 的不同部分是存在优先级的
    //! 体现为 SYN flag > payload > FIN flag
    TCPSegment construct_segment(uint64_t seqno, uint64_t payload_len, bool syn=true, bool fin=true);

    //! 检查 seqno 是否合法，若不合法，则不应该发送这个 segment
    bool check_seqno(uint64_t seqno);

    //! 发送 segment 的时候需要维护一系列状态，统一放到这个函数中
    void _send_segment(TCPSegment seg, uint64_t seqno);

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    //! 新增了 syn 参数，控制 segment 中的 syn 字段
    void send_empty_segment(bool syn=false, bool fin=true);

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }

    //! 已经发送了 FIN, 并且收到对应的 ack
    //! 则连接已经终止，应该停止发送 segments
    bool _fin_acked{false};

    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
