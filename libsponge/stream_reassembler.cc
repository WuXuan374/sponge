#include "stream_reassembler.hh"
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

/**
 * 这三个变量是始终要记得维护的
 * _idx_to_substring
 * _assembled_end_index
 * _unassembled_count
*/

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _idx_to_string(), _assembled_end_index(0), _unassembled_count(0), _eof_index(SIZE_MAX) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_index = index + data.length();
        check_eof();
    }
    // 分成两步走，先把 string 写入队列
    // 然后统一检查队列中有哪些 bytes 可以被重组
    push_string_to_queue(data, index);
    assemble_string_from_queue();
}

void StreamReassembler::push_string_to_queue(const std::string &data, const size_t index) {
    /**
     * 把接收的 string 写入 queue 中
     * 只有索引 >= _assembled_end_index 的部分，才有必要写入
     * 写入 queue 的时候，要注意 capacity 限制 
     * 修改状态: _unassembled_count
    */

    size_t current_bytes = bytes_count();
    if (current_bytes >= _capacity) {
        return; // 这个 string 直接被舍弃
    }
    // 能够写入的最大长度
    size_t bytes_left = _capacity - current_bytes;

    const size_t start_idx = _assembled_end_index > index ? (_assembled_end_index - index) : 0;
    if (start_idx >= data.length()) { // 空字符串也没有存储的必要
        return;
    }

    // 注意 substr 的 start_idx, 是相对于这个字符串而言的，不是整体的 index
    const std::string written_data = data.substr(start_idx, bytes_left);
    if (written_data.length() > 0) {
        _idx_to_string.push(
            make_pair(
                index + start_idx, 
                written_data
            )
        );
        _unassembled_count += written_data.length();
    }
}


void StreamReassembler::assemble_string_from_queue() {
    /**
     * 遍历 _idx_to_substring, 依次将 string 写入 byte stream 中
     * 这里头我们就不需要考虑 capacity 了，本操作不影响 capacity
     * 需要维护的状态: _unassembled_count
    */
    while (!_idx_to_string.empty()) {
        const size_t cur_index = _idx_to_string.top().first;
        const std::string cur_data = _idx_to_string.top().second;
        cout << "80: " << cur_index << " " << _assembled_end_index << endl;
        
        if (cur_index > _assembled_end_index) {
            break; // 队列中不存在能够重组的字符串
        }
        _idx_to_string.pop();
        _unassembled_count -= cur_data.length();
        assemble_string_to_stream(cur_data, cur_index);

        // 根据 _assembled_end_index 的值，我们可以记录有多少 byte 被写入，并相应处理
        if (_assembled_end_index < cur_index + cur_data.length()) {
            // string 没有全部被写入
            // cout << "91: " << _assembled_end_index << p.first << " " << _assembled_end_index - p.first << " " << p.second.length() << endl;
            const std::string string_left = cur_data.substr(_assembled_end_index - cur_index);
            _idx_to_string.push(make_pair(_assembled_end_index, string_left));
            _unassembled_count += string_left.length();
        }
    }
}

void StreamReassembler::assemble_string_to_stream(const std::string &data, const size_t index) {
    /**
     * 将 string 写入 byte stream 中
     * 这一步不需要考虑 capacity, 调用时会保证
    */

    // data 的所有 bytes 都已经写入 byte stream 了
    if (_assembled_end_index - index >= data.length()) {
        return;
    }
    const std::string written_data = data.substr(_assembled_end_index - index, _output.remaining_capacity());
    _output.write(written_data);
    // 修改状态
    _assembled_end_index += written_data.length();
    check_eof();
}

void StreamReassembler::check_eof() {
    /**
     * _assembled_end_index 发生变化
     * 或者 _eof_index 发生变化
     * 就应该调用此函数
    */
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

