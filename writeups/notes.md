# 说明
- 精简之前的笔记，既包括理论，也包括实现

# c++ 语言知识
- 如果要把一个 object 设置为 const, 则不能访问这个 object 的任何非 const 方法
- wrapper type: a type that contains an inner type, but provides a different set of functions/operators

# lab0
## 整体工作
- 基于操作系统已经实现的 TCP 服务，写一个 stream socket, 读取一个网页上的信息
- 实现一个 in-memory reliable byte stream
    - in-memory: 单机的，暂时不考虑节点之间的数据传输
    - 是 TCP 服务的一个简单版本抽象: 向管道的一端写入数据；可以从另一端读取数据
    - flow-controlled: 有容量限制
### in-memory reliable byte stream
- 在 TCP 中的应用:
    - 对于发送端，应用层是向 byte stream 中写数据；传输层是从中读数据；接收方就反过来
    - 数据在网络中的传输也可以抽象成 byte stream, 只不过是不可靠的，需要通过 TCP 的机制来弥补这一问题
## 代码细节
### 基于已实现的 TCP 服务，写一个 stream socket
- 很简单，发送端把套接字绑定到 host 的地址上；写数据，然后读数据即可
- bind():  associates the socket with its local address, 所以服务器会调用 bind()
- connect(): connect to a remote [server] address, 所以客户端调用 connect()
### in-memory reliable byte stream
- 使用 string 作为 byte stream
    - append(): 对应 push
    - substr, erase: 对应 peek 和 pop()

# lab1
## 整体工作
- lab1 - lab4 合起来实现一个 TCP 服务
- TCP: providing a pair of reliable byte streams using unreliable datagrams
- lab1 主要实现的是一个 stream reassembler
    - 真实世界中，丢包、延迟等现象都会发生，这导致 segments 很可能无法按序达到；这个 lab 会根据数据的顺序把数据重组起来
    - 显然是 TCP 中一个很重要的功能，能够帮助接收方给出正确的 ack
- reassember 有一个容量限制，不能无限制地存储没有被重组的数据（TCP 中，接收方不能无限制地存储没有被 ack 的包）
## 设计思路
- 接收 string 时, 先统一把 string 写入 set 中，set 是根据 string 的 start_index 来排序的 `push_string_to_set`
    - 如果容量已满，需要调用 `remove_from_set`
    - 注意写入的字符串不应该存在 overlap, `get_non_overlap_range`
- 然后我们再去检查 set 中是否存在能够被重组的 string (用到了 set 是有序的性质)，如有则进行重组
    - 遍历 set, 直到没有字符串可以被重组
- 分成两步走，感觉是比较清晰的
## 代码细节
### 接收 string, 先存到一个数据结构中
- 用的是 set, 原因
    - 能够遍历（便于寻找能够被重组的 string）
    - 能够排序（优先查找序号较小的 string）
### `push_string_to_set`
- 首先是和之前已经存储的字符串不应该有 overlap(起始和终止序号的角度)
    - 加入 set 前就这么做，能够节省空间
- 对于字符串下标的检查
    - `start_idx - _assembled_end_index >= _capacity`, 这种情况，该字符串是不可能被重组的，不应该放到 set 中, 否则会占据空间
- 容量不足，则 `remove_from_set`
- 字符串对应部分（去掉了 overlap）, 写入 set
### `get_non_overlap_range`
- 一方面和其他未被重组的字符串比较
- 另一方面也和已经被重组的序号比较，小于这个序号的，直接丢弃
### `remove_from_set`
- 一个关键点，如果空间不够，应该优先删除 set 中的哪部分？
    - 我认为是 start_index 越大的，越应该删除（注意这里已经没有 overlap 了），因为这种字符串被重组的概率比较低
    - 所以就应该从后往前遍历 set, 去删除字符串（直到 set 中字符串的 start_index 比要插入的更小）
### `assemble_string_from_set`
- 到了重组数据阶段，遍历前面的 set, 看看哪些字符串可以被重组
- set 已经排序了，因此如果发现当前字符串的序号过大，就不需要往下遍历了
- 比较麻烦的地方: 重组后的数据时写入 buffer 中，buffer 也有容量限制，股可能出现一个 string 只有部分能够被重组的情况，这时候把 string 的剩下部分，再写入 set

# lab2 TCP Receiver
- TCP 采用的是 Sliding Window 策略；允许缓存一些没有被 ack 的 segments (由于丢包或者延迟导致的); 目标是减少重传次数（只重传丢的包）同时提高速度（可以连续发多个包，不需要等收到上一个包的 ack 了再发下一个包）
## 整体工作
- Sliding Window 的重要逻辑，其实 stream reassembler 已经实现了
- receiver 主要完成和 Sender 之间的信息交互
    - 接收数据
    - 维护 ack
    - 维护 window size (flow control)
    - 实现数据 index 到 ack 的转换
## 代码细节
### 64-bit data indexes --> 32-bit seqnos
- 目的就是节省空间, TCP segment 的空间很宝贵
- seqno, absolute seqno, (data) index; TCP 传输的是 absolute seqno
    - absolute seqno 也会溢出啊？但是连接中出现差了一轮的（2^32）的 seqno 是非常罕见的，可以忽略溢出的影响
- Why absolute seqno? 允许TCP 连接任意选择一个初始的 seqno, 避免被伪造一个 tcp segment
### TCP Receiver
#### 整体状态
- 如果连接已经关闭，或者连接还没建立，则拒收报文
    - 连接已经关闭（注意条件并不是收到 FIN 报文）: _reassembler.stream_out().input_ended()
    - 连接还没建立: 还没有收到过 SYN, 并且当前报文带有 SYN 标记
- 接收报文并检查
    - 首次收到 SYN 报文: 建立连接（用一个布尔值记录状态）；记录 ISN
    - 首次收到 FIN 报文: 记录状态（布尔值）；eof = true (写入 reassembler 的时候，会修改 _eof_index, 告知底层的 byte stream 何时关闭管道)
- 接下来就是将报文中的数据写入 reassembler, 注意带上 eof 标记
#### seqno 偏移
- 这里的偏移，其主要原因是: SYN flag 和 FIN flag 各自占据了一个 sequence number; 导致 seqno 的数量和数据长度之间存在偏移
- 理解: 第一个 sequence number 被 SYN 占据；最后一个 sequence number 被 FIN 占据
- 那么对于 ISN, 应当是起始数据的位置（而不是 SYN 的位置），因此是 seqno+1
- 同样的，对于带有 SYN 符号的报文，数据的起始位置是 seqno + 1
- ackno 就根据 seqno 相应地生成，唯一的特例:
    - 当 input_ended(), 也就是所有数据都重组好了，ackno 应该是数据的终止位置 + 1 (因为发送端 FIN 符号占据了一个 sequence number)

#### Window size
- reassembler 可以存储的数据；那就是整体的容量 - buffer 中已经使用的容量（还没有被读取的数据）

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

# lab4
- 实现 TCP Connection
## 动机和内容
- 还是首先得回顾一下 sender 和 receiver
- sender:
    - 通过一个 ByteStream 和应用层连接: 应用层向 ByteStream 写入数据，Sender 从 ByteStream 中读取数据，并且发送给另一方
- receiver:
    - 也是通过一个 ByteStream 和应用层连接；但是这个 ByteStream 实现了流重组的功能; 应用层从 ByteStream 中读取重组好的数据
### 什么是 Connection
- Connection 考虑的是两个 TCP peer 之间的连接，及其相关状态的维护
- 每个 peer 都既有sender, 又有 receiver
    - 因为 TCP 是一个双向连接（全双工？），两个 peer 都可以发送和接收数据
- connection 有哪些状态需要维护？
    - 连接的建立和关闭（整体状态）
    - 一些高层的拥塞控制（?）机制，比如太久没有收到回复，应该终止连接
    - sender 和 receiver 同时完成的任务: 对于 peer, 收到一个 segment 之后，receiver 和 sender 都有相应的动作；相应地，发送 segment 时，sender 和 receiver 的信息（比如 receiver 的 ackno）都要带上
    -

# 一些非代码问题的处理
## 磁盘空间不够:
- 删除 /var/log 底下的文件，相对比较安全
    - 特别是 /var/log/journal 底下的 log, 占据了很大空间
```shell
sudo journalctl --vacuum-size=10M
```
- 这个命令删除 /var/log/journal 下 旧的 Log, 使得 log 总大小不超过 10M
- 列出各文件夹所占据空间 -- 命令 `du`
### 还可以删除
- ~/.cache/vscode-cpptools 占据了非常大的空间(2.9G)
- 这是C/C++ 插件的缓存文件存储的地址
- 修改 VSCode 中 setting 的下述属性，可以限制最大使用空间
    - 限制之后，会自动删除超出这个限制的缓存文件
```json
C_Cpp.intelliSenseCacheSize
```

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
### 跑的太慢了
- 原因还是自己有些东西没实现，导致 test 失败，就会很慢
- workaround 是在 tests.cmake 中修改 timeout
### lab4 的单元测试内容很多，终端里头看不到错误信息
- 把运行结果重定向到一个文件
```shell
SomeCommand > SomeFile.txt  
SomeCommand >> SomeFile.txt // 如果要追加内容
command | tee output.txt // 建议用这个， terminal 上面仍然看得到
```
