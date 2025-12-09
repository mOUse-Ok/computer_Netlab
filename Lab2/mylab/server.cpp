#include <iostream>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义
#include "protocol.h"  // 引入自定义协议头文件

#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库

#define PORT 8888  // 服务端监听端口
#define BUFFER_SIZE 1024  // 缓冲区大小

// ===== 全局接收窗口 =====
// 描述：用于管理流水线接收的滑动窗口状态
RecvWindow g_recvWindow;

// ===== 模拟丢包标志 =====
// 描述：用于测试SACK功能，设置为true时会丢弃特定序列号的包
bool g_simulateLoss = false;
uint32_t g_lossSeq;  // 要丢弃的序列号

// ===== 发送ACK/SACK响应 =====
// 描述：发送确认包，支持累积确认和选择确认
// 参数：
//   serverSocket - 服务端套接字
//   clientAddr   - 客户端地址
//   addrLen      - 地址结构长度
//   ackNum       - 确认号（期望接收的下一个序列号）
//   serverSeq    - 服务端序列号
//   useSACK      - 是否使用选择确认
void sendACK(SOCKET serverSocket, sockaddr_in& clientAddr, int addrLen,
             uint32_t ackNum, uint32_t serverSeq, bool useSACK) {
    Packet ackPacket;
    ackPacket.header.seq = serverSeq;
    ackPacket.header.ack = ackNum;
    ackPacket.header.flag = FLAG_ACK;
    ackPacket.header.win = FIXED_WINDOW_SIZE;  // 携带固定窗口大小（流量控制）
    
    // 如果使用SACK，生成并携带选择确认信息
    if (useSACK) {
        ackPacket.header.flag |= FLAG_SACK;  // 设置SACK标志
        
        // 生成SACK信息
        SACKInfo sackInfo;
        sackInfo.count = g_recvWindow.generateSACK(sackInfo.sack_blocks, MAX_SACK_BLOCKS);
        
        // 将SACK信息序列化到数据部分
        char sackData[64];
        int sackLen = sackInfo.serialize(sackData);
        ackPacket.setData(sackData, sackLen);
        
        std::cout << "[Send] ACK+SACK packet ack=" << ackNum << ", SACK blocks=[";
        for (int i = 0; i < sackInfo.count; i++) {
            if (i > 0) std::cout << ",";
            std::cout << sackInfo.sack_blocks[i];
        }
        std::cout << "], win=" << FIXED_WINDOW_SIZE << std::endl;
    } else {
        ackPacket.header.len = 0;
        ackPacket.dataLen = 0;
        ackPacket.header.calculateChecksum(ackPacket.data, 0);
        std::cout << "[Send] ACK packet ack=" << ackNum << ", win=" << FIXED_WINDOW_SIZE << std::endl;
    }
    
    char sendBuffer[MAX_PACKET_SIZE];
    ackPacket.serialize(sendBuffer);
    sendto(serverSocket, sendBuffer, ackPacket.getTotalLen(), 0,
          (sockaddr*)&clientAddr, addrLen);
}

// ===== 流水线接收数据函数（支持SACK） =====
// 描述：使用滑动窗口接收数据，支持选择确认和固定窗口流量控制
// 参数：
//   serverSocket - 服务端套接字
//   clientAddr   - 客户端地址
//   addrLen      - 地址结构长度
//   baseSeq      - 起始序列号（期望接收的第一个包序列号）
//   serverSeq    - 服务端序列号（用于发送ACK）
//   finReceived  - 输出参数：是否收到FIN包
//   finSeq       - 输出参数：FIN包的序列号
// 返回值：接收到的总数据长度，失败返回-1
int pipelineRecv(SOCKET serverSocket, sockaddr_in& clientAddr, int addrLen,
                 uint32_t baseSeq, uint32_t& serverSeq, bool& finReceived, uint32_t& finSeq) {
    // 初始化接收窗口
    g_recvWindow.reset(baseSeq);
    
    // 初始化FIN标志
    finReceived = false;
    finSeq = 0;
    
    // 存储接收到的完整数据
    static char receivedData[8192];
    int totalReceived = 0;
    
    std::cout << "\n[Pipeline Receive] Starting to receive data, window size=" << FIXED_WINDOW_SIZE 
              << ", starting sequence number=" << baseSeq << std::endl;
    
    // 设置接收超时
    int timeout = 5000;  // 5秒超时
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    int idleCount = 0;  // 空闲计数器
    int maxIdleCount = 3;  // 最大空闲次数
    
    while (idleCount < maxIdleCount) {
        char recvBuffer[MAX_PACKET_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        
        int bytesReceived = recvfrom(serverSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                     (sockaddr*)&fromAddr, &fromAddrLen);
        
        if (bytesReceived == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                idleCount++;
                std::cout << "[Timeout] Waiting for data packet timeout (" << idleCount << "/" << maxIdleCount << ")" << std::endl;
                continue;
            }
            std::cerr << "[Error] Receive failed: " << WSAGetLastError() << std::endl;
            return -1;
        }
        
        idleCount = 0;  // 重置空闲计数器
        
        // 解析接收到的包
        Packet recvPacket;
        if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
            std::cout << "[Error] Packet checksum failed, discarded" << std::endl;
            continue;
        }
        
        // 检查是否为FIN包（客户端请求关闭连接）
        if (recvPacket.header.flag & FLAG_FIN) {
            std::cout << "[Receive] FIN packet seq=" << recvPacket.header.seq << std::endl;
            // 设置FIN标志并返回
            finReceived = true;
            finSeq = recvPacket.header.seq;
            return totalReceived;
        }
        
        uint32_t recvSeq = recvPacket.header.seq;
        
        // ===== 模拟丢包（用于测试SACK） =====
        if (g_simulateLoss && recvSeq == g_lossSeq) {
            std::cout << "[Simulate Loss] Discard seq=" << recvSeq << " data packet" << std::endl;
            g_simulateLoss = false;  // 只丢弃一次
            continue;
        }
        
        // 检查序列号是否在窗口范围内
        if (g_recvWindow.inWindow(recvSeq)) {
            int idx = g_recvWindow.getIndex(recvSeq);
            
            // 检查是否是重复包
            if (g_recvWindow.is_received[idx]) {
                std::cout << "[Duplicate] Received duplicate packet seq=" << recvSeq << ", sending ACK" << std::endl;
            } else {
                // 缓存数据包
                memcpy(g_recvWindow.data_buf[idx], recvPacket.data, recvPacket.dataLen);
                g_recvWindow.data_len[idx] = recvPacket.dataLen;
                g_recvWindow.is_received[idx] = 1;
                
                std::cout << "[Receive] Data packet seq=" << recvSeq 
                         << ", length=" << recvPacket.dataLen
                         << ", window[" << g_recvWindow.base << "," 
                         << (g_recvWindow.base + FIXED_WINDOW_SIZE - 1) << "]";
                
                // 显示数据内容（如果是可打印字符）
                if (recvPacket.dataLen > 0 && recvPacket.dataLen < 100) {
                    char tempBuf[128];
                    memcpy(tempBuf, recvPacket.data, recvPacket.dataLen);
                    tempBuf[recvPacket.dataLen] = '\0';
                    std::cout << ", content: " << tempBuf;
                }
                std::cout << std::endl;
            }
            
            // 尝试滑动窗口并取出连续数据
            uint32_t oldBase = g_recvWindow.base;
            int dataLen = g_recvWindow.slideAndGetData(receivedData + totalReceived, 
                                                       sizeof(receivedData) - totalReceived);
            totalReceived += dataLen;
            
            if (g_recvWindow.base > oldBase) {
                std::cout << "[Window Slide] base: " << oldBase << " -> " << g_recvWindow.base << std::endl;
            }
            
            // 检查是否需要发送SACK（窗口内有非连续的已接收包）
            bool needSACK = false;
            for (uint32_t seq = g_recvWindow.base; seq < g_recvWindow.base + FIXED_WINDOW_SIZE; seq++) {
                int checkIdx = g_recvWindow.getIndex(seq);
                if (g_recvWindow.is_received[checkIdx] && seq > g_recvWindow.base) {
                    // 存在非连续的已接收包，需要SACK
                    needSACK = true;
                    break;
                }
            }
            
            // 发送ACK/SACK
            sendACK(serverSocket, clientAddr, addrLen, g_recvWindow.base, serverSeq, needSACK);
            
        } else if (recvSeq < g_recvWindow.base) {
            // 收到旧包（序列号小于窗口base），说明之前的ACK可能丢失，重发ACK
            std::cout << "[Old Packet] seq=" << recvSeq << " < base=" << g_recvWindow.base 
                     << ", resending ACK" << std::endl;
            sendACK(serverSocket, clientAddr, addrLen, g_recvWindow.base, serverSeq, false);
        } else {
            // 序列号超出窗口范围，丢弃（流量控制）
            std::cout << "[Out of Window] seq=" << recvSeq << " out of window range, discarded" << std::endl;
        }
    }
    
        std::cout << "[Pipeline Receive] Reception completed, received " << totalReceived << " bytes of data" << std::endl;
    return totalReceived;
}

// ===== 服务端处理三次握手 =====
// 返回值：true表示连接建立成功，false表示失败
bool acceptConnection(SOCKET serverSocket, sockaddr_in& clientAddr, uint32_t& clientSeq, uint32_t& serverSeq) {
    ConnectionState state = CLOSED;
    std::cout << "\n[Three-way Handshake] Waiting for client connection..." << std::endl;
    
    // 第一次握手：接收客户端的SYN包
    char recvBuffer[MAX_PACKET_SIZE];
    int clientAddrLen = sizeof(clientAddr);
    
    int bytesReceived = recvfrom(serverSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                 (sockaddr*)&clientAddr, &clientAddrLen);
    
    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "[Error] Failed to receive SYN packet: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    Packet recvPacket;
    if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
        std::cout << "[Error] Packet checksum failed" << std::endl;
        return false;
    }
    
    // 检查是否为SYN包
    if (recvPacket.header.flag & FLAG_SYN) {
        clientSeq = recvPacket.header.seq;
        std::cout << "[Received] SYN packet (seq=" << clientSeq << ") from " 
                 << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;
        
        state = SYN_RCVD;
        std::cout << "[State Transition] CLOSED -> SYN_RCVD" << std::endl;
        
        // 第二次握手：发送SYN+ACK包
        serverSeq = generateInitialSeq();
        
        Packet synAckPacket;
        synAckPacket.header.seq = serverSeq;
        synAckPacket.header.ack = clientSeq + 1;
        synAckPacket.header.flag = FLAG_SYN | FLAG_ACK;
        synAckPacket.dataLen = 0;
        synAckPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
        synAckPacket.header.calculateChecksum(synAckPacket.data, 0);
        
        char sendBuffer[MAX_PACKET_SIZE];
        synAckPacket.serialize(sendBuffer);
        int bytesSent = sendto(serverSocket, sendBuffer, synAckPacket.getTotalLen(), 0,
                              (sockaddr*)&clientAddr, clientAddrLen);
        
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "[Error] Failed to send SYN+ACK packet: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "[Sent] SYN+ACK packet (seq=" << serverSeq << ", ack=" << synAckPacket.header.ack << ")" << std::endl;
        
        // 第三次握手：接收客户端的ACK包
        // 设置接收超时
        int timeout = TIMEOUT_MS;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        
        bytesReceived = recvfrom(serverSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                (sockaddr*)&clientAddr, &clientAddrLen);
        
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "[Timeout] Client ACK not received" << std::endl;
            return false;
        }
        
        if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
            std::cout << "[Error] Packet checksum failed" << std::endl;
            return false;
        }
        
        if ((recvPacket.header.flag & FLAG_ACK) && recvPacket.header.ack == serverSeq + 1) {
            std::cout << "[Received] ACK packet (ack=" << recvPacket.header.ack << ")" << std::endl;
            state = ESTABLISHED;
            std::cout << "[State Transition] SYN_RCVD -> ESTABLISHED" << std::endl;
            std::cout << "[Success] Connection established!\n" << std::endl;
            
            serverSeq++;  // 更新序列号
            clientSeq++;  // 更新客户端序列号
            return true;
        }
    }
    
    return false;
}

// ===== 服务端处理四次挥手（被动关闭） =====
// 返回值：true表示关闭成功，false表示失败
bool handleClose(SOCKET serverSocket, sockaddr_in& clientAddr, uint32_t clientSeq, uint32_t serverSeq) {
    ConnectionState state = ESTABLISHED;
    std::cout << "\n[Four-way Handshake] Received client close request..." << std::endl;
    
    // 第一次挥手已经在数据接收循环中收到FIN包
    // 这里直接从第二次挥手开始
    
    // Second handshake: Send ACK packet
    state = CLOSE_WAIT;
    std::cout << "[State Transition] ESTABLISHED -> CLOSE_WAIT" << std::endl;
    
    Packet ackPacket;
    ackPacket.header.seq = serverSeq;
    ackPacket.header.ack = clientSeq + 1;
    ackPacket.header.flag = FLAG_ACK;
    ackPacket.dataLen = 0;
    ackPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
    ackPacket.header.calculateChecksum(ackPacket.data, 0);
    
    char sendBuffer[MAX_PACKET_SIZE];
    ackPacket.serialize(sendBuffer);
    int bytesSent = sendto(serverSocket, sendBuffer, ackPacket.getTotalLen(), 0,
                          (sockaddr*)&clientAddr, sizeof(clientAddr));
    
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "[Error] Failed to send ACK packet: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    std::cout << "[Sent] ACK packet (ack=" << ackPacket.header.ack << ")" << std::endl;
    
    // 模拟处理剩余数据（这里暂停一小段时间）
    Sleep(500);
    
    // Third handshake: Send FIN packet
    state = LAST_ACK;
    std::cout << "[State Transition] CLOSE_WAIT -> LAST_ACK" << std::endl;
    
    Packet finPacket;
    finPacket.header.seq = serverSeq;
    finPacket.header.ack = clientSeq + 1;
    finPacket.header.flag = FLAG_FIN;
    finPacket.dataLen = 0;
    finPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
    finPacket.header.calculateChecksum(finPacket.data, 0);
    
    finPacket.serialize(sendBuffer);
    bytesSent = sendto(serverSocket, sendBuffer, finPacket.getTotalLen(), 0,
                      (sockaddr*)&clientAddr, sizeof(clientAddr));
    
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "[Error] Failed to send FIN packet: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    std::cout << "[Sent] FIN packet (seq=" << serverSeq << ")" << std::endl;
    
    // 第四次挥手：等待客户端的最后ACK
    int timeout = TIMEOUT_MS;
    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    char recvBuffer[MAX_PACKET_SIZE];
    int clientAddrLen = sizeof(clientAddr);
    int bytesReceived = recvfrom(serverSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                 (sockaddr*)&clientAddr, &clientAddrLen);
    
    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "[Timeout] Client final ACK not received" << std::endl;
        // Can close even if timeout, client will close after TIME_WAIT
        state = CLOSED;
        std::cout << "[State Transition] LAST_ACK -> CLOSED" << std::endl;
        return true;
    }
    
    Packet recvPacket;
    if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
        std::cout << "[Error] Packet checksum failed" << std::endl;
        return false;
    }
    
    if ((recvPacket.header.flag & FLAG_ACK) && recvPacket.header.ack == serverSeq + 1) {
        std::cout << "[Received] ACK packet (ack=" << recvPacket.header.ack << ")" << std::endl;
        state = CLOSED;
        std::cout << "[State Transition] LAST_ACK -> CLOSED" << std::endl;
        std::cout << "[Success] Connection closed!\n" << std::endl;
        return true;
    }
    
    return false;
}

int main() {
    // 1. 初始化Winsock库
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);  // 请求版本2.2的Winsock
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // 2. 创建UDP套接字
    // AF_INET: IPv4地址族
    // SOCK_DGRAM: UDP数据报套接字
    // IPPROTO_UDP: UDP协议
    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();  // 清理Winsock资源
        return 1;
    }

    // 3. 设置服务端地址结构体
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));  // 清零结构体
    serverAddr.sin_family = AF_INET;  // IPv4地址族
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // 绑定到所有可用网卡
    serverAddr.sin_port = htons(PORT);  // 端口号，htons将主机字节序转换为网络字节序

    // 4. 绑定套接字到指定地址和端口
    result = bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);  // 关闭套接字
        WSACleanup();  // 清理Winsock资源
        return 1;
    }

    std::cout << "UDP Server is running on port " << PORT << std::endl;
    std::cout << "Waiting for client messages..." << std::endl;

    // 5. 处理客户端连接
    sockaddr_in clientAddr;  // 用于存储客户端地址
    uint32_t clientSeq = 0;  // 客户端序列号
    uint32_t serverSeq = 0;  // 服务端序列号
    
    // 执行三次握手，建立连接
    if (!acceptConnection(serverSocket, clientAddr, clientSeq, serverSeq)) {
        std::cerr << "Connection establishment failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 6. 连接已建立，使用流水线接收数据（支持SACK）
    std::cout << "\n===== Pipeline Receive Mode (window size=" << FIXED_WINDOW_SIZE << ") =====" << std::endl;
    
    // 丢包模拟
     g_lossSeq = clientSeq + 2;  // 丢弃第2个包
    
    // 使用流水线方式接收数据
    int clientAddrLen = sizeof(clientAddr);
    bool finReceived = false;
    uint32_t finSeq = 0;
    int receivedLen = pipelineRecv(serverSocket, clientAddr, clientAddrLen, clientSeq, serverSeq, finReceived, finSeq);
    
    if (receivedLen > 0) {
        std::cout << "\n[Summary] Successfully received " << receivedLen << " bytes of data" << std::endl;
    } else if (receivedLen < 0) {
        std::cerr << "[Error] Data reception failed" << std::endl;
    }
    
    // 如果收到了FIN包，直接处理四次挥手
    if (finReceived) {
        clientSeq = finSeq;
        if (handleClose(serverSocket, clientAddr, clientSeq, serverSeq)) {
            std::cout << "[Success] Connection closed successfully" << std::endl;
        }
    } else {
        // 等待客户端关闭连接或超时
        ConnectionState state = ESTABLISHED;
        char recvBuffer[MAX_PACKET_SIZE];
        
        // 设置非阻塞接收超时
        int timeout = 10000;  // 10秒超时
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        
        bool connectionActive = true;
        while (connectionActive) {
            // 接收客户端消息
            int bytesReceived = recvfrom(serverSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                         (sockaddr*)&clientAddr, &clientAddrLen);
            
            if (bytesReceived == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAETIMEDOUT) {
                    std::cout << "[Timeout] Connection idle timeout, client may have disconnected" << std::endl;
                    break;
                }
                std::cerr << "[Error] Receive failed: " << WSAGetLastError() << std::endl;
                continue;
            }
            
            // 解析接收到的包
            Packet recvPacket;
            if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
                std::cout << "[Error] Packet checksum failed, discarded" << std::endl;
                continue;
            }
            
            // 检查是否为FIN包（客户端请求关闭）
            if (recvPacket.header.flag & FLAG_FIN) {
                std::cout << "[Receive] FIN packet seq=" << recvPacket.header.seq << std::endl;
                clientSeq = recvPacket.header.seq;
                
                // 处理四次挥手
                if (handleClose(serverSocket, clientAddr, clientSeq, serverSeq)) {
                    connectionActive = false;
                }
                break;
            }
        }
    }

    // 7. 清理资源
    closesocket(serverSocket);
    WSACleanup();

    std::cout << "Server program ended" << std::endl;

    return 0;
}
