# 说明
- 精简之前的笔记，既包括理论，也包括实现

# lab3
- 实现 TCP 的发送端
## 整体工作
- 从 stream 中读取数据
- 将 stream 中的数据转化成 TCP segments, 并发送给 receiver
    - 维护 seqno, ackno 等
    - 注意 flow control
- 对于超时未收到 ack 的 segment, 需要重传
## 代码细节
### 想在循环过程中，移除 iterator 所对应的元素？
- 错误示范
```c++
for (auto it = _outstanding_segments.begin(); it != _outstanding_segments.end(); it++) {
    TCPSegment seg = *it;
    if (seg.header().seqno.raw_value() + seg.length_in_sequence_space() <= ackno.raw_value()) {
        _bytes_in_flight -= seg.length_in_sequence_space();
        _outstanding_segments.erase(it);
    }
}
```
- 正确示范（需要记录 next iterator）
```c++
while (it != _outstanding_segments.end()) {
    TCPSegment seg = *it;
    if (seg.header().seqno.raw_value() + seg.length_in_sequence_space() <= ackno.raw_value()) {
        _bytes_in_flight -= seg.length_in_sequence_space();
        auto next_it = std::next(it, 1);
        _outstanding_segments.erase(it);
        it = next_it;
    } else {
        it++;
    }
}
```
### 函数名前面的 '&'?
- 示例
```c++
ByteStream &stream_in() { return _stream; }
```
- 说明这个函数 **returned by reference**, 返回的是一个 ByteStream 的引用
- 我感觉我们自己写的，不需要
## 实现（设计）细节
### 发送 segment
- 构造 segment:
    - 由于 window size 有限，因此 segment 中的数据存在优先级
    - SYN flag 优先级最高，然后是 payload, 最后才是 FIN
    - 比如 window size = 3, len(payload) = 3, 已经到达stream 末尾；此时就不应该发送 FIN 符号，只发送 payload