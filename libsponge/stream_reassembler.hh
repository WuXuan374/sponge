#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <set>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    
    struct Block {
      size_t start_index = 0;
      std::string data = "";
      bool operator<(const Block b) const { return start_index < b.start_index; }
    };
    
    set<Block> _blocks;

    size_t _assembled_end_index; // 已经重组好的数据的 end index, 注意是开区间
    
    size_t _unassembled_count; // unassembled string 长度
    size_t _eof_index;

    // Byte Stream unread length + unassembled string length
    size_t bytes_count() const;

    void push_string_to_set(const std::string &data, const size_t index);

    // 把 string 写入 byte stream, 这一个函数内不需要考虑 capacity
    void assemble_string_to_stream(const std::string &data, const size_t index);

    // 组合之前没有重组成功，存储在 set 中的 string
    void assemble_string_from_set();

    void check_eof();

    pair<size_t, size_t> get_non_overlap_range(const std::string &data, const size_t index);

    void remove_from_set(size_t count);

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
