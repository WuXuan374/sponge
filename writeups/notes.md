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
- 发送 segment 之前的检查:
    - 如果已经发送了 FIN, 并收到了对应的 ack, 此时不应该发送 segment
    - 如果这个 seqno 不合法，体现为过大（超出已读取的数据量），不发送
- 不要重复发送 segment
    - 如果 segment 已经在 _outstanding_segments 中了，不要重复发送
- 维护下列状态
    - _segments_out: 写入数据，上层应用会读取，然后真正的发送这段数据
    - _outstanding_segments: 已发送，但还没有 ack 的 segment 集合
    - _next_seqno: 下一个正常发送的 segment 的起始序号 (不考虑重发)
    - _bytes_in_flight: 发送中，还没收到 ack 的 bytes 数量;
        - 和 _outstanding_segments 是对应的
        - 用来计算 sender window size
    - _sender_window_size
    - 如果之前没有计时器，则启动一个计时器
#### Sender window size
- 计算公式: _sender_window_size = _receiver_window_size - _bytes_in_flight
- 特别之处: 如果 _receiver_window_size == 0, 将其视作 1；目的是 window size 更新之后，能够得到更新之后的值
#### fill window
- 如果 stream 中有数据，并且发送端的 window size 有空间，就能传多少数据传多少
- 可能要将数据划分成多个 segments


### 收到 ACK 之后的处理
- 检查: 忽略 ack 不合理的报文（比 next seqno 更大）
- 更新: Receiver window size, ackno(如果更大)
- 维护状态:
    - RTO 回归初值
    - 连续重传次数置为 0
- 检查 _outstanding_segments, 看看哪些报文被 ack 了
    - 被 ack 的需要从 _outstanding_segments 中移除，并维护 _bytes_in_flight
- 计时器处理: 取消计时（没有 outstanding segment 了）或重启计时器（还有 segment 剩下）
- 维护 FIN ACKED 状态

### 怎么检查是否需要重传 segment
- 有一个 tick() 方法，应该是定时被调用的，在这个方法中进行重传检查
- 如果存在计时器并且超时了
    - 如果不是 Receiver window size 导致的（说明应该是传输的故障，而不是接收方的问题），则将 RTO 加倍，连续重传次数 + 1
    - 重传 sequence number 最小的 segment

### 总结计时器的生命周期
- timer 是针对所有 outstanding segments 的，并不是针对某个 segment 的
- 每次发送一个非空 segment 时(长度不为 0)
    - 如果 **没有正在运行的 timer**, 则启动一个 timer
- timer 超时 (在 tick() 中发现)
    - 相应操作: 重传，RTO 翻倍、记录连续重传次数
    - 重新启动一个 timer
- outstanding segments 被 ack 了
    - 相应操作: RTO 设为初值、从队列（集合）中移除、consecutive_retransmission 设为 0
    - 如果所有 segments 都被 ack, 则清除 timer; 否则重新启动一个 timer

# 一些非代码问题的处理
## 磁盘空间不够:
- 删除 /var/log 底下的文件，相对比较安全
    - 特别是 /var/log/journal 底下的 log, 占据了很大空间
```shell
sudo journalctl --vacuum-size=50M
```
- 这个命令删除 /var/log/journal 下 旧的 Log, 使得 log 总大小不超过 50M

## UTM 虚拟机环境搭建

### ssh 的连接
- 参考 https://medium.com/@lizrice/linux-vms-on-an-m1-based-mac-with-vscode-and-utm-d73e7cb06133, 设置端口映射
- 参考 https://docs.getutm.app/guides/ubuntu/#networking-is-unavailable, 网络设置
- 我将虚拟机的TCP端口映射到 127.0.0.1:2222 上
```shell
ssh cs144@127.0.0.1 -p 2222
```
- 然后就可以用 VSCode, 通过 ssh 连接到 VM 上了
### 设置一个共享文件夹
- CS144/shared
- 直接挂载在 ~/shared 位置

## gdb 调试工具
- 参考1: https://cs144.github.io/lab_faq.html
- 好像装不上，有空再看吧
- 国外源网络连不上，然后本机的代理不知道为啥也连不上，只能换成清华源
    - https://mirrors.tuna.tsinghua.edu.cn/help/ubuntu-ports/ 按照这里的说明修改配置文件: **注意，需要把配置文件中的 https 都改成 http**
    - 然后先跑一下 `sudo apt-get update`
    - 随后再运行 `sudo apt-get install gdb`
    - 本机的代理：在 clashX 文件中，"设置" -- “允许来自局域网的连接” 即可
- 然后就是找到对应的文件，打断点，然后根据图形界面去调试；左边会展示各种变量信息，等于不需要自己去手动 print 了
- launch.json 的内容
```json
{
    // Use IntelliSense to learn about possible attributes.
    // Hover to view descriptions of existing attributes.
    // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
    "version": "0.2.0",
    "configurations": [
        {
            "name": "sponge debug",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/tests/${fileBasenameNoExtension}",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ],
            "miDebuggerPath": "/usr/bin/gdb"
        }
    ]
}
```

## 关于单元测试
- [FAQ](https://cs144.github.io/lab_faq.html) 说可以自己写单元测试文件，但是我试了一下会报错
- 可以直接在已有的单元测试文件中加一些自己的测试，然后正常运行 `make check_lab{k}` 就行

