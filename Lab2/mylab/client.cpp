#include <iostream>
#include <cstdio>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义
#include "protocol.h"  // 引入自定义协议头文件


#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库

#define SERVER_PORT 8888  // 服务端端口
#define SERVER_IP "127.0.0.1"  // 服务端IP地址，这里使用本地回环地址
#define BUFFER_SIZE 1024  // 缓冲区大小

// ===== 全局发送窗口 =====
// 描述：用于管理流水线发送的滑动窗口状态
SendWindow g_sendWindow;

// ===== 流水线发送数据函数（支持SACK） =====
// 描述：使用流水线方式发送大量数据，支持选择确认和固定窗口流量控制
// 参数：
//   clientSocket - 客户端套接字
//   serverAddr   - 服务端地址
//   data         - 要发送的数据
//   dataLen      - 数据长度
//   baseSeq      - 起始序列号
// 返回值：true表示发送成功，false表示发送失败
bool pipelineSend(SOCKET clientSocket, sockaddr_in& serverAddr, 
                  const char* data, int dataLen, uint32_t baseSeq) {
    // 初始化发送窗口
    g_sendWindow.reset(baseSeq);
    
    int totalPackets = (dataLen + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;  // 计算总包数
    int sentPackets = 0;    // 已完成发送（已确认）的包数
    int dataOffset = 0;     // 数据偏移量
    
    std::cout << "\n[Pipeline Send] Starting to send data, total length=" << dataLen 
              << ", total packets=" << totalPackets 
              << ", initial window size=" << FIXED_WINDOW_SIZE << std::endl;
    std::cout << "[RENO] Initial state: cwnd=" << g_sendWindow.cwnd 
              << ", ssthresh=" << g_sendWindow.ssthresh 
              << ", phase=" << getRenoPhaseName(g_sendWindow.reno_phase) << std::endl;
    
    // 设置非阻塞接收超时（用于接收ACK）
    int timeout = 100;  // 100ms短超时，用于轮询
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    while (sentPackets < totalPackets) {
        // ===== 步骤1：发送窗口内所有可发送的包（流水线发送，受 RENO 拥塞窗口限制） =====
        while (g_sendWindow.canSend() && dataOffset < dataLen) {
            int idx = g_sendWindow.getIndex(g_sendWindow.next_seq);
            
            // 计算当前包的数据长度
            int packetDataLen = (dataLen - dataOffset > MAX_DATA_SIZE) ? 
                                MAX_DATA_SIZE : (dataLen - dataOffset);
            
            // 将数据复制到发送窗口缓冲区（用于可能的重传）
            memcpy(g_sendWindow.data_buf[idx], data + dataOffset, packetDataLen);
            g_sendWindow.data_len[idx] = packetDataLen;
            g_sendWindow.is_sent[idx] = 1;
            g_sendWindow.is_ack[idx] = 0;
            g_sendWindow.send_time[idx] = clock();  // 记录发送时间
            
            // 构造并发送数据包
            Packet dataPacket;
            dataPacket.header.seq = g_sendWindow.next_seq;
            dataPacket.header.ack = 0;
            dataPacket.header.flag = FLAG_ACK;  // 数据包带ACK标志
            dataPacket.header.win = FIXED_WINDOW_SIZE;  // 携带窗口大小（流量控制）
            dataPacket.setData(g_sendWindow.data_buf[idx], packetDataLen);
            
            char sendBuffer[MAX_PACKET_SIZE];
            dataPacket.serialize(sendBuffer);
            int bytesSent = sendto(clientSocket, sendBuffer, dataPacket.getTotalLen(), 0,
                                  (sockaddr*)&serverAddr, sizeof(serverAddr));
            
            if (bytesSent == SOCKET_ERROR) {
                std::cerr << "[错误] 发送数据包失败: " << WSAGetLastError() << std::endl;
                return false;
            }
            
            std::cout << "[Send] Data packet seq=" << g_sendWindow.next_seq 
                     << ", length=" << packetDataLen 
                     << ", window[" << g_sendWindow.base << "," 
                     << (g_sendWindow.base + g_sendWindow.getEffectiveWindow() - 1) << "]"
                     << ", cwnd=" << g_sendWindow.cwnd << std::endl;
            
            dataOffset += packetDataLen;
            g_sendWindow.next_seq++;
        }
        
        // ===== 步骤2：接收ACK/SACK并处理（整合 RENO 拥塞控制） =====
        char recvBuffer[MAX_PACKET_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        
        int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                     (sockaddr*)&fromAddr, &fromAddrLen);
        
        if (bytesReceived > 0) {
            Packet ackPacket;
            if (ackPacket.deserialize(recvBuffer, bytesReceived)) {
                // 检查是否为ACK包
                if (ackPacket.header.flag & FLAG_ACK) {
                    std::cout << "[Receive] ACK packet ack=" << ackPacket.header.ack;
                    
                    // ===== RENO 拥塞控制：处理 ACK =====
                    bool isNewACK = g_sendWindow.handleNewACK(ackPacket.header.ack);
                    
                    // 检查是否带有SACK标志
                    if (ackPacket.header.flag & FLAG_SACK) {
                        // 解析SACK信息
                        SACKInfo sackInfo;
                        if (ackPacket.dataLen > 0 && 
                            sackInfo.deserialize(ackPacket.data, ackPacket.dataLen)) {
                            std::cout << ", SACK blocks=[";
                            for (int i = 0; i < sackInfo.count; i++) {
                                if (i > 0) std::cout << ",";
                                std::cout << sackInfo.sack_blocks[i];
                                // 标记SACK中的序列号为已确认
                                if (sackInfo.sack_blocks[i] >= g_sendWindow.base &&
                                    sackInfo.sack_blocks[i] < g_sendWindow.base + FIXED_WINDOW_SIZE) {
                                    int sackIdx = g_sendWindow.getIndex(sackInfo.sack_blocks[i]);
                                    g_sendWindow.is_ack[sackIdx] = 1;
                                }
                            }
                            std::cout << "]" << std::endl;
                        }
                    }
                    std::cout << std::endl;
                    
                    // 累积确认：标记所有序列号 < ack 的包为已确认
                    for (uint32_t seq = g_sendWindow.base; seq < ackPacket.header.ack; seq++) {
                        if (seq < g_sendWindow.base + FIXED_WINDOW_SIZE) {
                            int idx = g_sendWindow.getIndex(seq);
                            if (g_sendWindow.is_sent[idx] && !g_sendWindow.is_ack[idx]) {
                                g_sendWindow.is_ack[idx] = 1;
                                sentPackets++;
                            }
                        }
                    }
                    
                    // 滑动窗口
                    uint32_t oldBase = g_sendWindow.base;
                    g_sendWindow.slideWindow();
                    if (g_sendWindow.base > oldBase) {
                        std::cout << "[Window Slide] base: " << oldBase << " -> " << g_sendWindow.base << std::endl;
                    }
                    
                    // ===== RENO 快速重传处理 =====
                    // 如果检测到快速重传（3个重复ACK），立即重传丢失的包
                    if (g_sendWindow.dup_ack_count == DUP_ACK_THRESHOLD && 
                        g_sendWindow.reno_phase == FAST_RECOVERY) {
                        // 重传丢失的包（base位置的包）
                        uint32_t lostSeq = g_sendWindow.last_ack;
                        if (lostSeq >= g_sendWindow.base && lostSeq < g_sendWindow.next_seq) {
                            int lostIdx = g_sendWindow.getIndex(lostSeq);
                            
                            std::cout << "[RENO] Fast Retransmit: retransmitting seq=" << lostSeq << std::endl;
                            
                            Packet retxPacket;
                            retxPacket.header.seq = lostSeq;
                            retxPacket.header.ack = 0;
                            retxPacket.header.flag = FLAG_ACK;
                            retxPacket.header.win = FIXED_WINDOW_SIZE;
                            retxPacket.setData(g_sendWindow.data_buf[lostIdx], g_sendWindow.data_len[lostIdx]);
                            
                            char sendBuffer[MAX_PACKET_SIZE];
                            retxPacket.serialize(sendBuffer);
                            sendto(clientSocket, sendBuffer, retxPacket.getTotalLen(), 0,
                                  (sockaddr*)&serverAddr, sizeof(serverAddr));
                            
                            g_sendWindow.send_time[lostIdx] = clock();  // 更新发送时间
                        }
                    }
                }
            }
        }
        
        // ===== 步骤3：检查超时并重传（选择性重传，整合 RENO 超时处理） =====
        clock_t currentTime = clock();
        bool hasTimeout = false;  // 标记是否发生超时
        
        for (uint32_t seq = g_sendWindow.base; seq < g_sendWindow.next_seq; seq++) {
            int idx = g_sendWindow.getIndex(seq);
            // 检查是否已发送但未确认且超时
            if (g_sendWindow.is_sent[idx] && !g_sendWindow.is_ack[idx]) {
                double elapsedMs = (double)(currentTime - g_sendWindow.send_time[idx]) * 1000.0 / CLOCKS_PER_SEC;
                if (elapsedMs > SACK_TIMEOUT_MS) {
                    // ===== RENO 拥塞控制：超时处理 =====
                    if (!hasTimeout) {
                        // 第一个超时包触发 RENO 超时处理
                        g_sendWindow.handleTimeout();
                        hasTimeout = true;
                    }
                    
                    // 超时重传该包
                    std::cout << "[Timeout Retransmit] seq=" << seq << ", elapsed " << (int)elapsedMs << "ms" << std::endl;
                    
                    Packet retxPacket;
                    retxPacket.header.seq = seq;
                    retxPacket.header.ack = 0;
                    retxPacket.header.flag = FLAG_ACK;
                    retxPacket.header.win = FIXED_WINDOW_SIZE;
                    retxPacket.setData(g_sendWindow.data_buf[idx], g_sendWindow.data_len[idx]);
                    
                    char sendBuffer[MAX_PACKET_SIZE];
                    retxPacket.serialize(sendBuffer);
                    sendto(clientSocket, sendBuffer, retxPacket.getTotalLen(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
                    
                    g_sendWindow.send_time[idx] = clock();  // 重置发送时间
                }
            }
        }
    }
    
    std::cout << "[Pipeline Send] Data transmission completed, sent " << totalPackets << " packets" << std::endl;
    std::cout << "[RENO] Final state: cwnd=" << g_sendWindow.cwnd 
              << ", ssthresh=" << g_sendWindow.ssthresh 
              << ", phase=" << getRenoPhaseName(g_sendWindow.reno_phase) << std::endl;
    return true;
}

// ===== 三次握手函数：建立连接 =====
// 返回值：true表示连接成功，false表示连接失败
bool handshake(SOCKET clientSocket, sockaddr_in& serverAddr, uint32_t& clientSeq, uint32_t& serverSeq) {
    ConnectionState state = CLOSED;
    int retries = 0;  // 重传次数
    
    // 生成客户端初始序列号
    clientSeq = generateInitialSeq();
    std::cout << "\n[Three-way Handshake] Starting connection establishment..." << std::endl;
    
    // First handshake: Send SYN packet
    state = SYN_SENT;
    std::cout << "[State Transition] CLOSED -> SYN_SENT" << std::endl;
    
    Packet synPacket;
    synPacket.header.seq = clientSeq;
    synPacket.header.ack = 0;
    synPacket.header.flag = FLAG_SYN;
    synPacket.dataLen = 0;
    synPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
    synPacket.header.calculateChecksum(synPacket.data, 0);
    
    while (retries < MAX_RETRIES) {
        // 发送SYN包
        char sendBuffer[MAX_PACKET_SIZE];
        synPacket.serialize(sendBuffer);
        int bytesSent = sendto(clientSocket, sendBuffer, synPacket.getTotalLen(), 0,
                              (sockaddr*)&serverAddr, sizeof(serverAddr));
        
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "[Error] Failed to send SYN packet: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "[Sent] SYN packet (seq=" << clientSeq << ", retry count=" << retries << ")" << std::endl;
        
        // 设置接收超时
        int timeout = TIMEOUT_MS;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        
        // 等待接收SYN+ACK包
        char recvBuffer[MAX_PACKET_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        
        int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                     (sockaddr*)&fromAddr, &fromAddrLen);
        
        if (bytesReceived == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                retries++;
                std::cout << "[Timeout] SYN+ACK not received, retransmitting SYN (attempt " << retries << ")" << std::endl;
                continue;
            } else {
                std::cerr << "[Error] Receive failed: " << WSAGetLastError() << std::endl;
                return false;
            }
        }
        
        // 解析收到的包
        Packet recvPacket;
        if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
            std::cout << "[Error] Packet checksum failed, discarded" << std::endl;
            continue;
        }
        
        // 第二次握手：检查是否为SYN+ACK包
        if ((recvPacket.header.flag & FLAG_SYN) && (recvPacket.header.flag & FLAG_ACK)) {
            if (recvPacket.header.ack == clientSeq + 1) {
                serverSeq = recvPacket.header.seq;
                std::cout << "[Received] SYN+ACK packet (seq=" << serverSeq 
                         << ", ack=" << recvPacket.header.ack << ")" << std::endl;
                
                // 第三次握手：发送ACK包
                Packet ackPacket;
                ackPacket.header.seq = clientSeq + 1;
                ackPacket.header.ack = serverSeq + 1;
                ackPacket.header.flag = FLAG_ACK;
                ackPacket.dataLen = 0;
                ackPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
                ackPacket.header.calculateChecksum(ackPacket.data, 0);
                
                ackPacket.serialize(sendBuffer);
                bytesSent = sendto(clientSocket, sendBuffer, ackPacket.getTotalLen(), 0,
                                  (sockaddr*)&serverAddr, sizeof(serverAddr));
                
                if (bytesSent == SOCKET_ERROR) {
                    std::cerr << "[Error] Failed to send ACK packet: " << WSAGetLastError() << std::endl;
                return false;
                }
                
                std::cout << "[Sent] ACK packet (seq=" << ackPacket.header.seq 
                         << ", ack=" << ackPacket.header.ack << ")" << std::endl;
                std::cout << "[State Transition] SYN_SENT -> ESTABLISHED" << std::endl;
                std::cout << "[Success] Connection established!\n" << std::endl;
                
                clientSeq++;  // 更新序列号
                return true;
            }
        }
    }
    
    std::cerr << "[Failed] Connection establishment failed, maximum retries reached" << std::endl;
    return false;
}

// ===== 四次挥手函数：关闭连接 =====
// 返回值：true表示关闭成功，false表示关闭失败
bool closeConnection(SOCKET clientSocket, sockaddr_in& serverAddr, uint32_t clientSeq, uint32_t serverSeq) {
    ConnectionState state = ESTABLISHED;
    std::cout << "\n[Four-way Handshake] Starting connection closure..." << std::endl;
    
    // First handshake: Client sends FIN packet
    state = FIN_WAIT_1;
    std::cout << "[State Transition] ESTABLISHED -> FIN_WAIT_1" << std::endl;
    
    Packet finPacket;
    finPacket.header.seq = clientSeq;
    finPacket.header.ack = serverSeq;
    finPacket.header.flag = FLAG_FIN;
    finPacket.dataLen = 0;
    finPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
    finPacket.header.calculateChecksum(finPacket.data, 0);
    
    char sendBuffer[MAX_PACKET_SIZE];
    finPacket.serialize(sendBuffer);
    int bytesSent = sendto(clientSocket, sendBuffer, finPacket.getTotalLen(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
    
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "[Error] Failed to send FIN packet: " << WSAGetLastError() << std::endl;
        return false;
    }
    
    std::cout << "[Sent] FIN packet (seq=" << clientSeq << ")" << std::endl;
    
    // 设置接收超时
    int timeout = TIMEOUT_MS;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    // 第二次挥手：等待服务端的ACK
    char recvBuffer[MAX_PACKET_SIZE];
    sockaddr_in fromAddr;
    int fromAddrLen = sizeof(fromAddr);
    
    int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                 (sockaddr*)&fromAddr, &fromAddrLen);
    
    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "[Timeout] Server ACK not received" << std::endl;
        return false;
    }
    
    Packet recvPacket;
    if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
        std::cout << "[Error] Packet checksum failed" << std::endl;
        return false;
    }
    
    if ((recvPacket.header.flag & FLAG_ACK) && recvPacket.header.ack == clientSeq + 1) {
        std::cout << "[Received] ACK packet (ack=" << recvPacket.header.ack << ")" << std::endl;
        state = FIN_WAIT_2;
        std::cout << "[State Transition] FIN_WAIT_1 -> FIN_WAIT_2" << std::endl;
    } else {
        std::cerr << "[Error] Received unexpected ACK packet" << std::endl;
        return false;
    }
    
    // Third handshake: Wait for server's FIN packet
    bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                            (sockaddr*)&fromAddr, &fromAddrLen);
    
    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "[Timeout] Server FIN not received" << std::endl;
        return false;
    }
    
    if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
        std::cout << "[Error] Packet checksum failed" << std::endl;
        return false;
    }
    
    if (recvPacket.header.flag & FLAG_FIN) {
        std::cout << "[Received] FIN packet (seq=" << recvPacket.header.seq << ")" << std::endl;
        
        // Fourth handshake: Send final ACK
        Packet finalAckPacket;
        finalAckPacket.header.seq = clientSeq + 1;
        finalAckPacket.header.ack = recvPacket.header.seq + 1;
        finalAckPacket.header.flag = FLAG_ACK;
        finalAckPacket.dataLen = 0;
        finalAckPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
        finalAckPacket.header.calculateChecksum(finalAckPacket.data, 0);
        
        finalAckPacket.serialize(sendBuffer);
        bytesSent = sendto(clientSocket, sendBuffer, finalAckPacket.getTotalLen(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
        
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "[Error] Failed to send final ACK: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "[Sent] ACK packet (ack=" << finalAckPacket.header.ack << ")" << std::endl;
        state = TIME_WAIT;
        std::cout << "[State Transition] FIN_WAIT_2 -> TIME_WAIT" << std::endl;
        
        // Wait for 2*MSL (TIME_WAIT state)
        std::cout << "[Waiting] TIME_WAIT state, waiting for " << TIME_WAIT_MS << "ms..." << std::endl;
        Sleep(TIME_WAIT_MS);
        
        state = CLOSED;
        std::cout << "[State Transition] TIME_WAIT -> CLOSED" << std::endl;
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
    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();  // 清理Winsock资源
        return 1;
    }

    // 3. 设置服务端地址结构体
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));  // 清零结构体
    serverAddr.sin_family = AF_INET;  // IPv4地址族
    // 将服务端IP地址从字符串转换为网络字节序
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_port = htons(SERVER_PORT);  // 端口号，htons将主机字节序转换为网络字节序

    // 4. 执行三次握手，建立连接
    uint32_t clientSeq = 0;  // 客户端序列号
    uint32_t serverSeq = 0;  // 服务端序列号
    
    if (!handshake(clientSocket, serverAddr, clientSeq, serverSeq)) {
        std::cerr << "Connection establishment failed!" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // 5. 连接已建立，使用流水线发送应用数据
    // 创建测试数据：每个消息单独作为一个包发送，以测试流水线和SACK
    // 为了让每个消息成为独立的包，我们需要将MAX_DATA_SIZE设置得较小，
    // 或者发送更多更大的数据。这里我们构造6个大数据块，每个块约200字节，
    // 模拟需要发送6个数据包的场景
    
    // 创建6个测试数据包，每个约250字节
    char testPackets[6][300];
    int packetLens[6];
    
    for (int i = 0; i < 6; i++) {
        // 填充每个包的数据
        snprintf(testPackets[i], sizeof(testPackets[i]), 
                "=== Packet %d ===\n"
                "This is test packet number %d for pipeline transmission testing.\n"
                "Testing SACK (Selective Acknowledgment) and fixed window flow control.\n"
                "Window size = %d, this packet should be sent and acknowledged correctly.\n"
                "Padding data to make packet larger: ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
                "End of packet %d.\n",
                i + 1, i + 1, FIXED_WINDOW_SIZE, i + 1);
        packetLens[i] = strlen(testPackets[i]);
    }
    
    std::cout << "\n===== Testing Pipeline Send (window size=" << FIXED_WINDOW_SIZE << ") =====" << std::endl;
    std::cout << "Preparing to send 6 separate packets for pipeline testing" << std::endl;
    std::cout << "[RENO] Testing RENO congestion control algorithm" << std::endl;
    
    // 逐包发送，每个包独立发送以测试流水线机制
    // 初始化发送窗口
    g_sendWindow.reset(clientSeq);
    
    int totalPackets = 6;
    int sentPackets = 0;    // 已完成发送（已确认）的包数
    int nextPacketToSend = 0;  // 下一个要发送的包索引
    
    std::cout << "\n[Pipeline Send] Starting to send " << totalPackets << " packets" << std::endl;
    std::cout << "[RENO] Initial state: cwnd=" << g_sendWindow.cwnd 
              << ", ssthresh=" << g_sendWindow.ssthresh 
              << ", phase=" << getRenoPhaseName(g_sendWindow.reno_phase) << std::endl;
    
    // 设置非阻塞接收超时（用于接收ACK）
    int timeout = 100;  // 100ms短超时，用于轮询
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    while (sentPackets < totalPackets) {
        // ===== 步骤1：发送窗口内所有可发送的包（流水线发送） =====
        while (g_sendWindow.canSend() && nextPacketToSend < totalPackets) {
            int idx = g_sendWindow.getIndex(g_sendWindow.next_seq);
            
            // 获取当前要发送的数据
            int packetDataLen = packetLens[nextPacketToSend];
            
            // 将数据复制到发送窗口缓冲区（用于可能的重传）
            memcpy(g_sendWindow.data_buf[idx], testPackets[nextPacketToSend], packetDataLen);
            g_sendWindow.data_len[idx] = packetDataLen;
            g_sendWindow.is_sent[idx] = 1;
            g_sendWindow.is_ack[idx] = 0;
            g_sendWindow.send_time[idx] = clock();  // 记录发送时间
            
            // 构造并发送数据包
            Packet dataPacket;
            dataPacket.header.seq = g_sendWindow.next_seq;
            dataPacket.header.ack = 0;
            dataPacket.header.flag = FLAG_ACK;  // 数据包带ACK标志
            dataPacket.header.win = FIXED_WINDOW_SIZE;  // 携带窗口大小（流量控制）
            dataPacket.setData(g_sendWindow.data_buf[idx], packetDataLen);
            
            char sendBuffer[MAX_PACKET_SIZE];
            dataPacket.serialize(sendBuffer);
            int bytesSent = sendto(clientSocket, sendBuffer, dataPacket.getTotalLen(), 0,
                                  (sockaddr*)&serverAddr, sizeof(serverAddr));
            
            if (bytesSent == SOCKET_ERROR) {
                std::cerr << "[Error] Failed to send data packet: " << WSAGetLastError() << std::endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }
            
            std::cout << "[Send] Data packet seq=" << g_sendWindow.next_seq 
                     << ", length=" << packetDataLen 
                     << ", window[" << g_sendWindow.base << "," 
                     << (g_sendWindow.base + g_sendWindow.getEffectiveWindow() - 1) << "]"
                     << ", cwnd=" << g_sendWindow.cwnd << std::endl;
            
            nextPacketToSend++;
            g_sendWindow.next_seq++;
        }
        
        // ===== 步骤2：接收ACK/SACK并处理（整合 RENO 拥塞控制） =====
        char recvBuffer[MAX_PACKET_SIZE];
        sockaddr_in fromAddr;
        int fromAddrLen = sizeof(fromAddr);
        
        int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                     (sockaddr*)&fromAddr, &fromAddrLen);
        
        if (bytesReceived > 0) {
            Packet ackPacket;
            if (ackPacket.deserialize(recvBuffer, bytesReceived)) {
                // 检查是否为ACK包
                if (ackPacket.header.flag & FLAG_ACK) {
                    std::cout << "[Receive] ACK packet ack=" << ackPacket.header.ack;
                    
                    // ===== RENO 拥塞控制：处理 ACK =====
                    bool isNewACK = g_sendWindow.handleNewACK(ackPacket.header.ack);
                    
                    // 检查是否带有SACK标志
                    if (ackPacket.header.flag & FLAG_SACK) {
                        // 解析SACK信息
                        SACKInfo sackInfo;
                        if (ackPacket.dataLen > 0 && 
                            sackInfo.deserialize(ackPacket.data, ackPacket.dataLen)) {
                            std::cout << ", SACK blocks=[";
                            for (int i = 0; i < sackInfo.count; i++) {
                                if (i > 0) std::cout << ",";
                                std::cout << sackInfo.sack_blocks[i];
                                // 标记SACK中的序列号为已确认
                                if (sackInfo.sack_blocks[i] >= g_sendWindow.base &&
                                    sackInfo.sack_blocks[i] < g_sendWindow.base + FIXED_WINDOW_SIZE) {
                                    int sackIdx = g_sendWindow.getIndex(sackInfo.sack_blocks[i]);
                                    g_sendWindow.is_ack[sackIdx] = 1;
                                }
                            }
                            std::cout << "]";
                        }
                    }
                    std::cout << std::endl;
                    
                    // 累积确认：标记所有序列号 < ack 的包为已确认，并滑动窗口
                    uint32_t ackNum = ackPacket.header.ack;
                    while (g_sendWindow.base < ackNum && g_sendWindow.base < g_sendWindow.next_seq) {
                        int idx = g_sendWindow.getIndex(g_sendWindow.base);
                        // 只要base向前移动，就表示一个包被确认
                        // 注意：如果包之前被SACK标记过，is_ack已经是1，不重复计数
                        if (!g_sendWindow.is_ack[idx]) {
                            sentPackets++;
                            std::cout << "[Confirmed] seq=" << g_sendWindow.base << ", sentPackets=" << sentPackets << "/" << totalPackets << std::endl;
                        } else {
                            // 被SACK过的包，现在被累积确认
                            sentPackets++;
                            std::cout << "[SACK->Confirmed] seq=" << g_sendWindow.base << ", sentPackets=" << sentPackets << "/" << totalPackets << std::endl;
                        }
                        // 清除当前位置状态并滑动窗口
                        g_sendWindow.is_sent[idx] = 0;
                        g_sendWindow.is_ack[idx] = 0;
                        g_sendWindow.data_len[idx] = 0;
                        g_sendWindow.base++;
                    }
                    std::cout << "[Window Slide] base -> " << g_sendWindow.base << std::endl;
                    
                    // ===== RENO 快速重传处理 =====
                    // 如果检测到快速重传（3个重复ACK），立即重传丢失的包
                    if (g_sendWindow.dup_ack_count == DUP_ACK_THRESHOLD && 
                        g_sendWindow.reno_phase == FAST_RECOVERY) {
                        // 重传丢失的包（last_ack位置的包）
                        uint32_t lostSeq = g_sendWindow.last_ack;
                        if (lostSeq >= g_sendWindow.base && lostSeq < g_sendWindow.next_seq) {
                            int lostIdx = g_sendWindow.getIndex(lostSeq);
                            
                            std::cout << "[RENO] Fast Retransmit: retransmitting seq=" << lostSeq << std::endl;
                            
                            Packet retxPacket;
                            retxPacket.header.seq = lostSeq;
                            retxPacket.header.ack = 0;
                            retxPacket.header.flag = FLAG_ACK;
                            retxPacket.header.win = FIXED_WINDOW_SIZE;
                            retxPacket.setData(g_sendWindow.data_buf[lostIdx], g_sendWindow.data_len[lostIdx]);
                            
                            char sendBuffer[MAX_PACKET_SIZE];
                            retxPacket.serialize(sendBuffer);
                            sendto(clientSocket, sendBuffer, retxPacket.getTotalLen(), 0,
                                  (sockaddr*)&serverAddr, sizeof(serverAddr));
                            
                            g_sendWindow.send_time[lostIdx] = clock();  // 更新发送时间
                        }
                    }
                }
            }
        }
        
        // ===== 步骤3：检查超时并重传（选择性重传，整合 RENO 超时处理） =====
        clock_t currentTime = clock();
        bool hasTimeout = false;  // 标记是否发生超时
        
        for (uint32_t seq = g_sendWindow.base; seq < g_sendWindow.next_seq; seq++) {
            int idx = g_sendWindow.getIndex(seq);
            // 检查是否已发送但未确认且超时
            if (g_sendWindow.is_sent[idx] && !g_sendWindow.is_ack[idx]) {
                double elapsedMs = (double)(currentTime - g_sendWindow.send_time[idx]) * 1000.0 / CLOCKS_PER_SEC;
                if (elapsedMs > SACK_TIMEOUT_MS) {
                    // ===== RENO 拥塞控制：超时处理 =====
                    if (!hasTimeout) {
                        // 第一个超时包触发 RENO 超时处理
                        g_sendWindow.handleTimeout();
                        hasTimeout = true;
                    }
                    
                    // 超时重传该包
                    std::cout << "[Timeout Retransmit] seq=" << seq << ", elapsed " << (int)elapsedMs << "ms" << std::endl;
                    
                    Packet retxPacket;
                    retxPacket.header.seq = seq;
                    retxPacket.header.ack = 0;
                    retxPacket.header.flag = FLAG_ACK;
                    retxPacket.header.win = FIXED_WINDOW_SIZE;
                    retxPacket.setData(g_sendWindow.data_buf[idx], g_sendWindow.data_len[idx]);
                    
                    char sendBuffer[MAX_PACKET_SIZE];
                    retxPacket.serialize(sendBuffer);
                    sendto(clientSocket, sendBuffer, retxPacket.getTotalLen(), 0,
                          (sockaddr*)&serverAddr, sizeof(serverAddr));
                    
                    g_sendWindow.send_time[idx] = clock();  // 重置发送时间
                }
            }
        }
    }
    
    std::cout << "[Pipeline Send] Data transmission completed, sent " << totalPackets << " packets" << std::endl;
    std::cout << "[RENO] Final state: cwnd=" << g_sendWindow.cwnd 
              << ", ssthresh=" << g_sendWindow.ssthresh 
              << ", phase=" << getRenoPhaseName(g_sendWindow.reno_phase) << std::endl;
    
    // 更新序列号
    clientSeq = g_sendWindow.next_seq;
    
    // 等待服务端处理完成
    Sleep(1000);
    
    // 6. 接收服务端的确认响应
    char finalRecvBuffer[MAX_PACKET_SIZE];
    sockaddr_in finalFromAddr;
    int finalFromAddrLen = sizeof(finalFromAddr);
    
    // 设置接收超时
    timeout = 5000;  // 5秒超时
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    std::cout << "\nWaiting for server's final confirmation..." << std::endl;
    int finalBytesReceived = recvfrom(clientSocket, finalRecvBuffer, MAX_PACKET_SIZE, 0, 
                                 (sockaddr*)&finalFromAddr, &finalFromAddrLen);
    if (finalBytesReceived == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            std::cout << "Receive timeout (server may have finished processing)" << std::endl;
        } else {
            std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
        }
    } else {
        Packet finalRecvPacket;
        if (finalRecvPacket.deserialize(finalRecvBuffer, finalBytesReceived)) {
            if (finalRecvPacket.dataLen > 0) {
                finalRecvPacket.data[finalRecvPacket.dataLen] = '\0';
                std::cout << "Received server response: " << finalRecvPacket.data << std::endl;
            }
            serverSeq = finalRecvPacket.header.seq + 1;
        } else {
            std::cout << "Packet checksum failed" << std::endl;
        }
    }

    // 7. Perform four-way handshake to close connection
    if (!closeConnection(clientSocket, serverAddr, clientSeq, serverSeq)) {
        std::cerr << "Connection closure process encountered an exception" << std::endl;
    }

    // 8. 清理资源
    closesocket(clientSocket);
    WSACleanup();

    std::cout << "Client program ended" << std::endl;

    return 0;
}