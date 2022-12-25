#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

// using namespace std;
/**
 * 这三个变量是始终要记得维护的
 * _idx_to_substring
 * _assembled_end_index
 * _unassembled_count
*/

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _idx_to_substring({}), _assembled_end_index(0), _unassembled_count(0), _eof_index(SIZE_MAX) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string &data, const size_t index, const bool eof) {
    // 首先进行 capacity 的检查
    size_t current_bytes = bytes_count();
    if (current_bytes >= _capacity) {
        return; // 这个 string 直接被舍弃
    }
    // 无论是写入 stream 还是留在 unassembled 存储中，能够写入的最大长度
    size_t bytes_left = _capacity - current_bytes;
    // 能不能直接写入 byte stream 中
    if (index <= _assembled_end_index) { // 可能存在 overlap
        // 超出 capacity 的，同样是直接被舍弃
        if (_assembled_end_index-index < data.length()) {
            string written_data = data.substr(_assembled_end_index-index, bytes_left); 
            _output.write(written_data);
            _assembled_end_index += written_data.length();
            // 检查是否有之前的数据，能够被重组
            assemble_string();
            check_eof();
        }
    } else {
        string written_data = data.substr(0, bytes_left); 
        // 没有新数据写入 byte stream 中，那么已有的 unassembled string 肯定不发生变化
        _idx_to_substring[index] = written_data;
        _unassembled_count += written_data.length();
    }
    if (eof) {
        _eof_index = index + data.length();
        check_eof();
    }
}

void StreamReassembler::assemble_string() {
    // 尝试将 unassembled string 写入 stream 中
    while (true) {
        // 将 unassembled string 写入 stream 中, 并不影响 Reassembler 的容量
        // 但是受限于 ByteStream 剩余的容量
        size_t bytes_left = _output.remaining_capacity();
        if (bytes_left <= 0) return;
        
        auto search = _idx_to_substring.find(_assembled_end_index);
        if (search == _idx_to_substring.end()) return;

        // 找到了 <key, value> pair, 可以写入 bytes_left 个 byte
        size_t start_idx = search -> first;
        string data = search -> second;
        string written_data = data.substr(0, bytes_left);

        _output.write(written_data);
        _assembled_end_index += written_data.length();
        _unassembled_count -= written_data.length();
        // 如果这个 string 没有全部写完
        if (written_data.length() < data.length()) {
            // substr, 第二个参数默认为 npos, 则返回 [pos, size()]
            _idx_to_substring[start_idx+written_data.length()] = data.substr(written_data.length());
        }
        check_eof();
    }
}

void StreamReassembler::check_eof() {
    if (_eof_index != SIZE_MAX) {
        if (_assembled_end_index == _eof_index) {
            _output.end_input();
        }
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _unassembled_count; 
}

bool StreamReassembler::empty() const {  
    return unassembled_bytes() == 0;
}

size_t StreamReassembler::bytes_count() const {
    return _output.buffer_size() + unassembled_bytes();
}

