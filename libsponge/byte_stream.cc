#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

// using namespace std;

 // m_capacity 只负责记录最大容量；目前 stream 实际装载了多少东西，由 m_buffer 存储
ByteStream::ByteStream(const size_t capacity) : m_buffer(""), m_capacity(capacity), m_write_count(0), m_read_count(0), m_input_ended(false), _error(false) {} 


size_t ByteStream::write(const std::string &data) {
    if (!input_ended()) {
        std::string content = data.substr(0, remaining_capacity());
        // as queue, first come first out
        m_buffer.append(content); 
        // 实际写入的长度
        m_write_count += content.size();
        return content.size();
    } else {
        set_error();
        return 0;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
std::string ByteStream::peek_output(const size_t len) const {
    // 边界情况，len <= 0, 返回空字符串
    return m_buffer.substr(0, len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    // 测试用例可能会单独调用这个 pop_output 函数，并要求我们这时候就要更新 m_read_count
    m_read_count += min(len, buffer_size());
    m_buffer.erase(0, len);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (!eof()) {
        const std::string content = peek_output(len);
        pop_output(len);
        return content;
    } else {
        //! 如果已经 eof 了，我感觉是应该报错的；但是测试程序似乎还是会调用 read() 方法（没有做是否 eof 的检查）
        //! TODO: 为了通过测试用例，这里暂时不报错；不过后面还得看看嗷。
        // set_error();
        return "";
    }
}

void ByteStream::end_input() {
    m_input_ended = true;
}

bool ByteStream::input_ended() const { 
    return m_input_ended;
}

size_t ByteStream::buffer_size() const { 
    return m_buffer.size();
}

bool ByteStream::buffer_empty() const { 
    return m_buffer.empty();
}

bool ByteStream::eof() const { 
    return input_ended() && buffer_empty();
}

size_t ByteStream::bytes_written() const { 
    return m_write_count;
}

size_t ByteStream::bytes_read() const { 
    return m_read_count;
}

size_t ByteStream::remaining_capacity() const { 
    return m_capacity - buffer_size();
}
