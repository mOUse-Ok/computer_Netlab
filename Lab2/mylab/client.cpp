#include <iostream>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义
#include <windows.h>   // Windows API用于目录操作
#include "client.h"    
#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库
// ===== 测试文件目录路径 =====
const char* TESTFILE_DIR = "testfile";
// ===== 获取testfile目录下的所有文件名 =====
std::vector<std::string> getTestFiles() {
    std::vector<std::string> files;
    WIN32_FIND_DATAA findData;
    std::string searchPath = std::string(TESTFILE_DIR) + "\\*";
    
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // 跳过目录（包括. 和 ..）
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    return files;
}
// ===== 读取文件内容 =====
bool readFileContent(const std::string& filename, std::vector<char>& content) {
    std::string filepath = std::string(TESTFILE_DIR) + "\\" + filename;
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        std::cerr << "[Error] Cannot open file: " << filepath << std::endl;
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    content.resize(size);
    if (!file.read(content.data(), size)) {
        std::cerr << "[Error] Failed to read file: " << filepath << std::endl;
        return false;
    }
    
    std::cout << "[Info] Read file '" << filename << "', size: " << size << " bytes" << std::endl;
    return true;
}




// ===== 传输单个文件 =====
bool transferFile(SOCKET clientSocket, sockaddr_in& serverAddr, 
                  const std::string& filename, uint32_t& clientSeq) {
    std::vector<char> content;// 存储文件内容
    if (!readFileContent(filename, content)) {
        return false;
    }
    
    std::cout << "\n[Transfer] Starting transfer of '" << filename << "'..." << std::endl;
    
    // 使用pipelineSend发送文件内容
    bool result = pipelineSend(clientSocket, serverAddr, content.data(), content.size(), clientSeq);
    
    if (result) {
        std::cout << "[Transfer] File '" << filename << "' transferred successfully!" << std::endl;
        // 更新序列号
        clientSeq = g_sendWindow.next_seq;
    } else {
        std::cerr << "[Transfer] Failed to transfer file '" << filename << "'" << std::endl;
    }
    
    return result;
}



// ========================================================= 流水线发送 ==================================================//
// 全局发送窗口：管理流水线发送的滑动窗口状态
SendWindow g_sendWindow;

// 流水线发送数据（支持SACK和RENO拥塞控制）
bool pipelineSend(SOCKET clientSocket, sockaddr_in& serverAddr, 
                  const char* data, int dataLen, uint32_t baseSeq) {
    // 初始化发送窗口
    g_sendWindow.reset(baseSeq);
    
    int totalPackets = (dataLen + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;  // 计算总包数，这里+MDS-1是为了向上取整
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
        while (g_sendWindow.canSend() && dataOffset < dataLen) {   // 检查序号是否在窗口内，且还有数据还没发完
            int idx = g_sendWindow.getIndex(g_sendWindow.next_seq);// 获取窗口内索引
            
            // 计算当前包的数据长度
            int packetDataLen = (dataLen - dataOffset > MAX_DATA_SIZE) ? 
                                MAX_DATA_SIZE : (dataLen - dataOffset);//MSS或剩余数据长度
            
            // 将数据复制到发送窗口缓冲区（用于可能的重传）
            memcpy(g_sendWindow.data_buf[idx], data + dataOffset, packetDataLen);//参数：目标地址，源地址，复制长度，data是要传输文件的基地址
            g_sendWindow.data_len[idx] = packetDataLen;
            g_sendWindow.is_sent[idx] = 1;
            g_sendWindow.is_ack[idx] = 0;
            g_sendWindow.send_time[idx] = clock();  // 记录发送时间，用于计时器
            
            // 构造并发送数据包
            Packet dataPacket;
            dataPacket.header.seq = g_sendWindow.next_seq;//设置序列号
            dataPacket.header.ack = 0;//因为是发送数据包，ack字段恒为0
            dataPacket.header.flag = FLAG_ACK;  // 数据包通常都设置ACK标志，虽然发送数据包用不上
            dataPacket.header.win = FIXED_WINDOW_SIZE;  // 用不上，暂时设置为固定窗口大小
            dataPacket.setData(g_sendWindow.data_buf[idx], packetDataLen);//加载数据到数据包的负载部分
            
            char sendBuffer[MAX_PACKET_SIZE];
            dataPacket.serialize(sendBuffer);
            int bytesSent = sendto(clientSocket, sendBuffer, dataPacket.getTotalLen(), 0,
                                  (sockaddr*)&serverAddr, sizeof(serverAddr));//参数：套接字，发送数据缓冲区，数据长度，标志，目标地址，地址长度
            
            if (bytesSent == SOCKET_ERROR) {
                std::cerr << "[错误] 发送数据包失败: " << WSAGetLastError() << std::endl;
                return false;
            }
            
            // 更新统计信息
            g_sendWindow.total_packets_sent++;
            g_sendWindow.total_bytes_sent += packetDataLen;
            
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
            if (ackPacket.deserialize(recvBuffer, bytesReceived)) {//解析接收到的包
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
                                // 根据SACK信息标记已接收的包，避免超时重传
                                if (sackInfo.sack_blocks[i] >= g_sendWindow.base &&
                                    sackInfo.sack_blocks[i] < g_sendWindow.base + FIXED_WINDOW_SIZE) {
                                    int sackIdx = g_sendWindow.getIndex(sackInfo.sack_blocks[i]);
                                    // 只有未确认的包才增加sentPackets
                                    if (g_sendWindow.is_sent[sackIdx] && !g_sendWindow.is_ack[sackIdx]) {
                                        g_sendWindow.is_ack[sackIdx] = 1;
                                        sentPackets++;
                                    }
                                }
                            }
                            std::cout << "]" << std::endl;
                        }
                    }
                    std::cout << std::endl;
                    
                    // 标记所有序列号 < ack 的包为已确认
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
                        uint32_t lostSeq = g_sendWindow.last_ack;//丢失的序列号
                        if (lostSeq >= g_sendWindow.base && lostSeq < g_sendWindow.next_seq) {//确保丢失包在发送窗口内
                            int lostIdx = g_sendWindow.getIndex(lostSeq);//获取丢失包的窗口索引
                            
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
                            
                            // 更新重传统计
                            g_sendWindow.total_packets_sent++;
                            g_sendWindow.total_retransmissions++;
                            
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
                    // 超时处理，进入慢启动
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
                    
                    // 更新重传统计
                    g_sendWindow.total_packets_sent++;
                    g_sendWindow.total_retransmissions++;
                    
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
// ========================================================= 流水线发送 ==================================================//




// ========================================================== 连接管理 ==================================================//
// 三次握手：建立连接
bool handshake(SOCKET clientSocket, sockaddr_in& serverAddr, uint32_t& clientSeq, uint32_t& serverSeq) {
    ConnectionState state = CLOSED;//连接状态，定义在client.h中
    int retries = 0;  // 重传次数
    
    // 生成客户端初始序列号
    clientSeq = generateInitialSeq();
    std::cout << "\n[Three-way Handshake] Starting connection establishment..." << std::endl;
    
    // First handshake: Send SYN packet
    // 第一次握手：客户端发送SYN包
    state = SYN_SENT;//把连接状态改为SYN_SENT
    std::cout << "[State Transition] CLOSED -> SYN_SENT" << std::endl;
    
    Packet synPacket;//构造SYN包
    synPacket.header.seq = clientSeq;//设置序列号
    synPacket.header.ack = 0;//初始ACK为0，表示这不是确认包
    synPacket.header.flag = FLAG_SYN; // 设置SYN标志，作用是告诉接收方这是一个连接请求包
    synPacket.dataLen = 0;//数据长度为0，因为SYN包不携带数据
    synPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
    synPacket.header.calculateChecksum(synPacket.data, 0); // 计算校验和
    
    while (retries < MAX_RETRIES) {//MAX_RETRIES定义在config.h中，表示建立、关闭连接时的最大重传次数
        // 发送SYN包
        char sendBuffer[MAX_PACKET_SIZE];//定义发送缓冲区
        synPacket.serialize(sendBuffer);//序列化SYN包到发送缓冲区
        int bytesSent = sendto(clientSocket, sendBuffer, synPacket.getTotalLen(), 0,
                              (sockaddr*)&serverAddr, sizeof(serverAddr));//发送SYN包，返回发送的字节数
        if (bytesSent == SOCKET_ERROR) {//发送失败
            std::cerr << "[Error] Failed to send SYN packet: " << WSAGetLastError() << std::endl;
            return false;
        }
        
        std::cout << "[Sent] SYN packet (seq=" << clientSeq << ", retry count=" << retries << ")" << std::endl;
        
        // 设置接收超时
        // 握手阶段的超时重传机制
        int timeout = TIMEOUT_MS;//设置超时时间，TIMEOUT_MS定义在config.h中，表示超时时间
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));//设置套接字选项，指定接收超时时间，这五个参数分别是：套接字描述符、级别（SOL_SOCKET表示套接字级别）、选项名称（SO_RCVTIMEO表示接收超时选项）、指向超时值的指针、超时值的大小
        
        // 等待接收SYN+ACK包
        char recvBuffer[MAX_PACKET_SIZE];
        sockaddr_in fromAddr;//定义发送方地址结构体
        int fromAddrLen = sizeof(fromAddr);//定义地址长度
        
        int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                     (sockaddr*)&fromAddr, &fromAddrLen);//接收数据包，返回接收的字节数
        
        if (bytesReceived == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {//接收超时导致的错误
                retries++;
                std::cout << "[Timeout] SYN+ACK not received, retransmitting SYN (attempt " << retries << ")" << std::endl;
                continue;
            } else {
                std::cerr << "[Error] Receive failed: " << WSAGetLastError() << std::endl;
                return false;
            }
        }
        
        // 解析收到的包
        Packet recvPacket;//定义接收包对象，调用deserialize方法可以将接收到的字节流反序列化为Packet对象
        if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {//解析失败
            // 校验和验证失败
            std::cout << "[Error] Packet checksum failed, discarded" << std::endl;
            continue;
        }
        
        // 第二次握手：检查是否为SYN+ACK包
        if ((recvPacket.header.flag & FLAG_SYN) && (recvPacket.header.flag & FLAG_ACK)) {//检查标志位是否同时包含SYN和ACK
            if (recvPacket.header.ack == clientSeq + 1) {//确认号正确
                serverSeq = recvPacket.header.seq;//记录服务器的初始序列号
                std::cout << "[Received] SYN+ACK packet (seq=" << serverSeq 
                         << ", ack=" << recvPacket.header.ack << ")" << std::endl;
                
                // 第三次握手：发送ACK包
                Packet ackPacket;//第一次握手发送的包叫synPacket，第二次握手收到的包叫recvPacket，第三次握手发送的包叫ackPacket
                ackPacket.header.seq = clientSeq + 1;
                ackPacket.header.ack = serverSeq + 1;
                ackPacket.header.flag = FLAG_ACK; // 设置ACK标志，表示这是一个确认包
                ackPacket.dataLen = 0;
                ackPacket.header.len = 0;  // 同步设置协议头中的数据长度字段
                ackPacket.header.calculateChecksum(ackPacket.data, 0);//计算校验和，并设置到包头中
                
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

// 四次挥手：关闭连接。本来也可以使用两次挥手来关闭连接，但为了确保server端也能正确关闭连接，还是使用四次挥手
bool closeConnection(SOCKET clientSocket, sockaddr_in& serverAddr, uint32_t clientSeq, uint32_t serverSeq) {
    ConnectionState state = ESTABLISHED;
    std::cout << "\n[Four-way Handshake] Starting connection closure..." << std::endl;
    
    // First handshake: Client sends FIN packet
    // 第一次挥手：客户端发送FIN包
    state = FIN_WAIT_1;
    std::cout << "[State Transition] ESTABLISHED -> FIN_WAIT_1" << std::endl;
    
    Packet finPacket;
    finPacket.header.seq = clientSeq;
    finPacket.header.ack = serverSeq;
    finPacket.header.flag = FLAG_FIN; // 设置FIN标志，表示这是一个终止连接的包
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
    
    // 挥手阶段的超时机制
    // // 设置接收超时
    int timeout = TIMEOUT_MS;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));//设置套接字选项，和握手阶段一样
    
    // 第二次挥手：等待服务端的ACK
    char recvBuffer[MAX_PACKET_SIZE];
    sockaddr_in fromAddr;//定义发送方地址结构体，用于接收数据包的来源信息
    int fromAddrLen = sizeof(fromAddr);
    
    int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0,
                                 (sockaddr*)&fromAddr, &fromAddrLen);//接收数据包，返回接收的字节数
    
    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "[Timeout] Server ACK not received" << std::endl;
        return false;
    }
    
    Packet recvPacket;
    if (!recvPacket.deserialize(recvBuffer, bytesReceived)) {
        // 校验和验证失败
        std::cout << "[Error] Packet checksum failed" << std::endl;
        return false;
    }
    
    if ((recvPacket.header.flag & FLAG_ACK) && recvPacket.header.ack == clientSeq + 1) {
        // 正确收到ACK包
        std::cout << "[Received] ACK packet (ack=" << recvPacket.header.ack << ")" << std::endl;
        state = FIN_WAIT_2;
        std::cout << "[State Transition] FIN_WAIT_1 -> FIN_WAIT_2" << std::endl;
    } else {
        std::cerr << "[Error] Received unexpected ACK packet" << std::endl;
        return false;
    }
    
    // 第三次挥手：等待服务端的FIN包
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
        // 第四次挥手：客户端发送最后的ACK包
        Packet finalAckPacket;
        finalAckPacket.header.seq = clientSeq + 1;
        finalAckPacket.header.ack = recvPacket.header.seq + 1;
        finalAckPacket.header.flag = FLAG_ACK; // 设置ACK标志
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
        state = TIME_WAIT;//进入TIME_WAIT状态
        std::cout << "[State Transition] FIN_WAIT_2 -> TIME_WAIT" << std::endl;
        
        // TIME_WAIT等待2MSL，确保服务端收到最后的ACK
        std::cout << "[Waiting] TIME_WAIT state, waiting for " << TIME_WAIT_MS << "ms..." << std::endl;
        Sleep(TIME_WAIT_MS);
        
        state = CLOSED;//关闭连接，其实在收到server的第二次挥手的ACK包后，客户端就可以关闭连接了，但我们四次挥手是确保双方都关闭连接，所以client要发送完最后一个ACK包后，进入TIME_WAIT状态，等待一段时间后再关闭连接
        std::cout << "[State Transition] TIME_WAIT -> CLOSED" << std::endl;
        std::cout << "[Success] Connection closed!\n" << std::endl;
        
        // 输出统计报告（客户端只统计发送信息）
        std::cout << "\n========== Client Transmission Statistics ==========" << std::endl;
        std::cout << "Total Packets Sent (incl. retrans): " << g_sendWindow.total_packets_sent << std::endl;
        std::cout << "Total Retransmissions: " << g_sendWindow.total_retransmissions << std::endl;
        std::cout << "====================================================\n" << std::endl;
        
        return true;
    }
    
    return false;
}
// ========================================================== 连接管理 ==================================================//



int main() {
    // 输出重定向：同时输出到终端和文件
    std::ofstream logFile("client.txt");
    std::streambuf* coutBuf = std::cout.rdbuf();
    std::streambuf* cerrBuf = std::cerr.rdbuf();
    
    // 创建一个同时输出到终端和文件的流缓冲
    class TeeStreamBuf : public std::streambuf {
    public:
        TeeStreamBuf(std::streambuf* sb1, std::streambuf* sb2) : sb1(sb1), sb2(sb2) {}
    protected:
        virtual int overflow(int c) override {
            if (c == EOF) return !EOF;
            if (sb1->sputc(c) == EOF || sb2->sputc(c) == EOF) return EOF;
            return c;
        }
        virtual int sync() override {
            if (sb1->pubsync() || sb2->pubsync()) return -1;
            return 0;
        }
    private:
        std::streambuf* sb1;
        std::streambuf* sb2;
    };
    
    TeeStreamBuf teeBuf(coutBuf, logFile.rdbuf());
    TeeStreamBuf teeErrBuf(cerrBuf, logFile.rdbuf());
    std::cout.rdbuf(&teeBuf);
    std::cerr.rdbuf(&teeErrBuf);

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
    
    // 增加 Socket 发送和接收缓冲区大小，防止高速传输时缓冲区溢出
    int bufSize = 1024 * 1024;  // 1MB 缓冲区
    if (setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(bufSize)) == SOCKET_ERROR) {
        std::cerr << "Warning: Failed to set send buffer size: " << WSAGetLastError() << std::endl;
    }
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize)) == SOCKET_ERROR) {
        std::cerr << "Warning: Failed to set receive buffer size: " << WSAGetLastError() << std::endl;
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

    // 5. 连接已建立，进入单文件传输模式
    std::cout << "\n===== Single File Transfer Mode (window size=" << FIXED_WINDOW_SIZE << ") =====" << std::endl;
    std::cout << "[RENO] RENO congestion control enabled" << std::endl;
    
    bool transferSuccess = false;
    
    // 获取testfile目录下的文件列表
    std::vector<std::string> files = getTestFiles();
    
    // 打印文件列表
    std::cout << "\n========== testfile Directory Files ==========" << std::endl;
    if (files.empty()) {
        std::cout << "  (No files found)" << std::endl;
    } else {
        for (size_t i = 0; i < files.size(); i++) {
            std::cout << "  [" << (i + 1) << "] " << files[i] << std::endl;
        }
    }
    std::cout << "===============================================" << std::endl;
    std::cout << "Please enter the filename to transfer: ";
    
    // 读取用户输入
    std::string input;
    std::getline(std::cin, input);
    
    // 去除首尾空格
    size_t start = input.find_first_not_of(" \t");
    size_t end = input.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        input = input.substr(start, end - start + 1);
    } else {
        input.clear();
    }
    
    // 检查用户输入
    if (input.empty()) {
        std::cout << "[Error] Empty input, exiting..." << std::endl;
    } else {
        // 检查输入的文件名是否存在
        bool found = false;
        for (const auto& file : files) {
            if (file == input) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cout << "[Error] File '" << input << "' not found in testfile directory." << std::endl;
        } else {
            // 传输指定文件
            transferSuccess = transferFile(clientSocket, serverAddr, input, clientSeq);
            
            // 传输完成后稍作等待
            Sleep(500);
        }
    }
    
    std::cout << "\n[Summary] File transfer " << (transferSuccess ? "succeeded" : "failed or skipped") << std::endl;

    // 6. 执行四次挥手关闭连接
    if (!closeConnection(clientSocket, serverAddr, clientSeq, serverSeq)) {
        std::cerr << "Connection closure process encountered an exception" << std::endl;
    }

    // 7. 清理资源
    closesocket(clientSocket);
    WSACleanup();

    std::cout << "Client program ended" << std::endl;

    return 0;
}