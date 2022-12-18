#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {  
    // m_capacity 只负责记录最大容量；目前 stream 实际装载了多少东西，由 m_buffer 存储
    m_capacity = capacity;
    m_write_count = 0;
    m_read_count = 0;
    m_input_ended = false;
    _error = false;
}

size_t ByteStream::write(const string &data) {
    if (!input_ended()) {
        string content = data.substr(0, remaining_capacity());
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
string ByteStream::peek_output(const size_t len) const {
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
        const string content = peek_output(len);
        pop_output(len);
        return content;
    } else {
        set_error();
        return NULL;
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
