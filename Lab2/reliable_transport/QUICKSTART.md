# 快速开始指南

## 项目概述

可靠传输协议（Reliable Transport Protocol, RTP）是基于UDP的可靠数据传输实现，包含：
- TCP风格的三次握手和四次挥手
- 流量控制（窗口管理）
- 拥塞控制（RENO算法）
- 数据校验（RFC 791互联网校验和）
- 超时重传机制

## 快速开始

### 1. 构建项目

```bash
cd /path/to/reliable_transport
make
```

生成的可执行文件：`bin/reliable_transport`

### 2. 运行程序

#### 服务器模式
```bash
./bin/reliable_transport -s -p 8888 -out received.dat
```

#### 客户端模式（另一个终端）
```bash
./bin/reliable_transport -c -i 127.0.0.1 -p 8888 -in input.dat
```

### 3. 查看帮助
```bash
./bin/reliable_transport -h
```

## 完整命令行选项

```
-s, --server              以服务器模式运行
-c, --client              以客户端模式运行（默认）
-i, --server-ip <IP>      服务器IP地址（仅客户端模式，默认127.0.0.1）
-p, --port <PORT>         端口号（默认8888）
-in, --input <FILE>       输入文件名（客户端模式）
-out, --output <FILE>     输出文件名（服务器模式）
-w, --window <SIZE>       窗口大小（默认8）
-h, --help                显示帮助信息
```

## 例子

### 例子1：本地文件传输
```bash
# 服务器（终端1）
./bin/reliable_transport -s -p 8888 -out /tmp/received.dat

# 客户端（终端2）
./bin/reliable_transport -c -i 127.0.0.1 -p 8888 -in /tmp/input.dat
```

### 例子2：自定义窗口大小
```bash
# 窗口大小为16
./bin/reliable_transport -s -p 9999 -out output.dat -w 16
./bin/reliable_transport -c -i 127.0.0.1 -p 9999 -in input.dat -w 16
```

### 例子3：不同主机传输
```bash
# 在主机A运行服务器
./bin/reliable_transport -s -p 8888 -out received.dat

# 在主机B运行客户端（将A.B.C.D替换为实际IP）
./bin/reliable_transport -c -i A.B.C.D -p 8888 -in input.dat
```

## 使用Makefile快速操作

```bash
# 构建
make                        # 构建项目
make DEBUG=1 all           # 调试模式构建
make clean                 # 清理

# 运行
make run_server            # 运行服务器（测试文件自动生成）
make run_client            # 运行客户端

# 测试
make test                  # 运行功能测试
make perf_test            # 运行性能测试

# 信息
make help                  # 显示Makefile帮助
make info                  # 显示构建配置
```

## 文件结构

```
reliable_transport/
├── src/                        # 源文件
│   ├── main.cpp               # 主程序
│   ├── packet.cpp             # 帧定义
│   ├── checksum.cpp           # 校验和
│   ├── connection.cpp         # 连接管理
│   ├── window.cpp             # 窗口管理
│   ├── congestion.cpp         # 拥塞控制
│   └── utils.cpp              # 工具函数
├── include/                    # 头文件
│   ├── packet.h
│   ├── checksum.h
│   ├── connection.h
│   ├── window.h
│   ├── congestion.h
│   ├── utils.h
│   └── reliable_transport.h
├── bin/                        # 生成的可执行文件
├── obj/                        # 编译的目标文件
├── data/                       # 测试数据
├── tests/                      # 测试脚本
│   ├── test.sh                # 功能测试
│   ├── performance_test.sh    # 性能测试
│   └── README.md              # 测试文档
├── Makefile                    # 构建脚本
└── README.md                   # 项目说明
```

## 输出日志示例

### 服务器启动
```
========================================
  Reliable Transport Protocol (UDP)
  Laboratory 2 - File Transfer
========================================

[INFO] ========== Reliable Transport Protocol initialized ==========
[INFO] Configuration: WINDOW_SIZE=8, MAX_PACKET_SIZE=1024, TIMEOUT=1000ms
[INFO] 
========== SERVER MODE ==========
[INFO] Listening on port: 8888
[INFO] Output file: received.dat
[INFO] Window size: 8
[INFO] UDP socket created: fd=3
[INFO] Socket bound to port 8888
[INFO] Socket timeout set to 1000 ms
[INFO] File opened for writing: received.dat
[INFO] Server: Waiting for client connection...
```

### 客户端连接
```
[INFO] Packet received: 13 bytes from 127.0.0.1:50000
[INFO] Packet sent: 16 bytes to 127.0.0.1:8888
[INFO] Packet received: 14 bytes from 127.0.0.1:8888
[INFO] Packet sent: 14 bytes to 127.0.0.1:8888
[INFO] Client: Handshake complete, starting file transmission
[INFO] Client: Sent DATA packet seq=...
```

### 传输完成
```
========== 传输统计信息 ==========
总传输字节数:     102400 bytes
传输总耗时:       2345 ms (2.35 s)
总包数:          101 packets
重传包数:        2 packets
平均传输速率:     0.35 Mbps
包丢失率:        1.98% (2/101)
平均包大小:      1024 bytes
===================================
```

## 常见问题

### Q: 如何测试大文件传输？

A: 使用dd命令创建测试文件：
```bash
dd if=/dev/zero of=large_file.dat bs=1M count=100  # 创建100MB文件
./bin/reliable_transport -c -i 127.0.0.1 -p 8888 -in large_file.dat
```

### Q: 如何监控网络流量？

A: 使用tcpdump查看UDP包：
```bash
# 需要管理员权限
sudo tcpdump -i lo 'udp port 8888'
```

### Q: 性能不理想，如何优化？

A: 
1. 运行性能测试找到最优窗口大小：`make perf_test`
2. 调整超时时间（修改`TIMEOUT_MS`）
3. 增加接收缓冲区大小
4. 在高负载时使用更大的窗口

### Q: 如何调试程序？

A: 以调试模式构建并使用gdb：
```bash
make DEBUG=1 clean all
gdb ./bin/reliable_transport
(gdb) run -s -p 8888 -out received.dat
```

### Q: 支持Windows吗？

A: 代码中有跨平台兼容性代码，但仅在Linux/Unix上测试。
Windows支持需要修改：
- 网络相关头文件（使用Winsock2）
- 文件I/O操作
- 时间函数

## 性能指标

典型性能（在本地环回接口上）：
- 小文件（10KB）：传输时间 < 100ms
- 中等文件（100KB）：传输时间 100-500ms
- 大文件（1MB）：传输时间 1-5s
- 吞吐量：1-10 Mbps（取决于窗口大小）

## 协议细节

### 帧结构
```
帧头部（14字节）：
  [seq_num(4B)][ack_num(4B)][window_size(2B)][frame_type(1B)][data_len(2B)][checksum(1B)]

帧类型：
  0: SYN (同步)
  1: SYN_ACK (同步确认)
  2: ACK (确认)
  3: FIN (结束)
  4: FIN_ACK (结束确认)
  5: DATA (数据)
```

### 连接状态机
```
客户端：CLOSED -> SYN_SENT -> ESTABLISHED -> FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT -> CLOSED

服务器：CLOSED -> LISTEN -> SYN_RECEIVED -> ESTABLISHED -> CLOSE_WAIT -> LAST_ACK -> CLOSED
```

## 参考资源

- TCP协议：RFC 793
- UDP协议：RFC 768
- 互联网校验和：RFC 791
- RENO拥塞控制：RFC 2581

## 许可证

MIT License

## 联系方式

如有问题或建议，请参考项目文档或联系维护者。
