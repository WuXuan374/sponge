# Lab0
- telnet: open a reliable byte stream
    - 支持 http/smtp 等服务
    - a client program that makes outgoing connections to programs running on other computers
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
Address(host, "http")
```