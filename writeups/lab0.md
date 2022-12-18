# Lab0
- telnet: open a reliable byte stream
    - 支持 http/smtp 等服务
    - a client program that makes outgoing connections to programs running on other computers
## 调试和测试用例相关
- 虚拟机上要安装 gdb: `sudo apt-get install gdb`
    - 安装的时候需要翻墙（换清华源似乎不行，会报找不到 gdb 的错误）
    - **所以需要从虚拟机内部，能够访问本机的网络 （目前好像不行，后面再说吧）**
## C++ style guideline
- http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- https://en.cppreference.com/w/
## Listening and Connecting
- netcat 的启动命令稍有不同
```shell
nc -vlnp 9090
```
## 基于操作系统已有的支持，实现一个 TCP 服务
- reliable bidirectional byte stream
- 
### connect() 和 bind() 的关系
- 这两个函数都是 Socket 类的成员
- bind():  associates the socket with its local address, 所以服务器会调用 bind()
- connect(): connect to a remote [server] address, 所以客户端调用 connect()

### c++ 语言相关知识
- 如果要把一个 object 设置为 const, 则不能访问这个 object 的任何非 const 方法
```c++
// 创建一个具备 automatic storage duration 的对象，不用去操心这个对象的生命周期（离开了代码块之后，这个对象被自动回收）
Obj o1 ("Hi\n"); 

// dynamic storage duration, 需要写代码的人自己来控制生命周期（比如调用 delete 回收这个对象）
Obj* o2 = new Obj("Hi\n");
```
- size_t VS int
    - int: 有符号 integer type, 至少 16 bits 宽
    - size_t: 无符号 integer, with enough bytes to represent the size of any type

### in-memory reliable byte stream
- 