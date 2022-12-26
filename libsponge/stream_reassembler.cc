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

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _blocks(), _assembled_end_index(0), _unassembled_count(0), _eof_index(SIZE_MAX) {}

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
    push_string_to_set(data, index);
    assemble_string_from_set();
}

void StreamReassembler::push_string_to_set(const std::string &data, const size_t index) {
    /**
     * 把接收的 string 写入 queue 中
     * 只有索引 >= _assembled_end_index 的部分，才有必要写入
     * 写入 queue 的时候，要注意 capacity 限制 
     * 修改状态: _unassembled_count
    */

    const pair<size_t, size_t> range = get_non_overlap_range(data, index);
    const size_t start_idx = range.first;
    const size_t end_idx = range.second;
    if (start_idx == SIZE_MAX && end_idx == SIZE_MAX) {
        return;
    }

    size_t current_bytes = bytes_count();
    if (current_bytes >= _capacity) {
        return; // 这个 string 直接被舍弃
    }
    // 能够写入的最大长度
    size_t bytes_left = _capacity - current_bytes;
    size_t written_count = std::min(end_idx-start_idx, bytes_left);
    if (written_count > 0) {
        Block b;
        b.start_index = start_idx;
        b.data = data.substr(start_idx - index, written_count);
        _blocks.insert(b);
        _unassembled_count += written_count;
    }
}


void StreamReassembler::assemble_string_from_set() {
    /**
     * 遍历 _idx_to_substring, 依次将 string 写入 byte stream 中
     * 这里头我们就不需要考虑 capacity 了，本操作不影响 capacity
     * 需要维护的状态: _unassembled_count
    */
    while (!_blocks.empty()) {
        // 起始位置最小的元素，是最有可能被重组的
        std::set<Block>::iterator it = _blocks.begin();
        Block minimum = *it;
        const size_t cur_index = minimum.start_index;
        const std::string cur_data = minimum.data;
        if (cur_index > _assembled_end_index) {
            break; // 队列中不存在能够重组的字符串
        }
        _blocks.erase(it);
        _unassembled_count -= cur_data.length();
        assemble_string_to_stream(cur_data, cur_index);

        if (_assembled_end_index < cur_index + cur_data.length()) {
            const std::string string_left = cur_data.substr(_assembled_end_index - cur_index);
            Block b;
            b.start_index = _assembled_end_index;
            b.data = string_left;
            _blocks.insert(b);
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

pair<size_t, size_t> StreamReassembler::get_non_overlap_range(const std::string &data, const size_t index) {
    /**
     * 将字符串加入 set 的时候，应该避免 overlap
     * 这里的逻辑是: 后加入 set 的字符串，应该只截取没有 overlap 的部分
     * start_idx: _assembled_end_index, index, set 中元素结尾位置 --> 最大值
     * end_idx: set 中元素的开始位置，index + data.length() --> 最小值
    */
    size_t start_idx = _assembled_end_index > index ? _assembled_end_index : index;
    size_t end_idx = index + data.length();

    std::set<Block>::iterator it = _blocks.begin();
    while (it != _blocks.end()) {
        size_t cur_start_idx = (*it).start_index;
        size_t cur_end_idx = cur_start_idx + (*it).data.length();

        if (cur_start_idx <= start_idx) {
            if (cur_end_idx >= end_idx) {
                return make_pair(SIZE_MAX, SIZE_MAX); // 不应该插入这个 string 
            } else if (cur_end_idx <= start_idx) {
                it++;
            } else {
                start_idx = cur_end_idx;
                it++;
            }
        } else if (cur_start_idx <= end_idx) {
            if (cur_end_idx <= end_idx) {
                auto next = it++; // 这个字符串会被新添加的字符串全覆盖
                _unassembled_count -= (*it).data.length();
                _blocks.erase(it);
                it = next;
            } else {
                end_idx = cur_start_idx;
                it++;
            }
        } else { // cur_start_idx > end_idx
            break; // 不会再有 overlap 了
        }
    }
    
    return make_pair(start_idx, end_idx);
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

