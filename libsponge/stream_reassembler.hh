#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <queue>

// class ComparisonClass {
//   public:
//       bool operator() (pair<size_t, std::string> const& a, pair<size_t, std::string> const& b) {
//           return a.first < b.first;
//       }
//   };

struct CustomCompare {
  bool operator()(const pair<size_t, std::string>& a, const pair<size_t, std::string>& b) {
    return a.first > b.first;
  }
};

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    
    // idx 到 string 的映射，按照 idx 升序存储
    priority_queue< pair<size_t, std::string>, std::vector<pair<size_t, std::string>>, CustomCompare > _idx_to_string;
    // priority_queue<pair<size_t, std::string>> _idx_to_string;
    size_t _assembled_end_index; // 已经重组好的数据的 end index, 注意是开区间
    // unassembled string 长度
    // 每次新增 unassembled string, 或者有 string 被写入 stream 的时候，要记得修改
    size_t _unassembled_count; 
    size_t _eof_index;

    // Byte Stream unread length + unassembled string length
    size_t bytes_count() const;

    void push_string_to_queue(const std::string &data, const size_t index);

    // 把 string 写入 byte stream, 这一个函数内不需要考虑 capacity
    void assemble_string_to_stream(const std::string &data, const size_t index);

    // 组合之前没有重组成功，存储在优先级队列中的 string
    void assemble_string_from_queue();

    void check_eof();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
