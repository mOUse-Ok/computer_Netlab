#include <iostream>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义
#include "protocol.h"  // 引入自定义协议头文件

#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库

#define SERVER_PORT 8888  // 服务端端口
#define SERVER_IP "127.0.0.1"  // 服务端IP地址，这里使用本地回环地址
#define BUFFER_SIZE 1024  // 缓冲区大小

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

    // 5. 连接已建立，发送应用数据
    const char* message = "Hello, UDP Server!";
    std::cout << "Sending message to server: " << message << std::endl;

    Packet dataPacket;
    dataPacket.header.seq = clientSeq;
    dataPacket.header.ack = serverSeq;
    dataPacket.header.flag = FLAG_ACK;  // 数据包带ACK标志
    dataPacket.setData(message, strlen(message));
    
    char sendBuffer[MAX_PACKET_SIZE];
    dataPacket.serialize(sendBuffer);
    int bytesSent = sendto(clientSocket, sendBuffer, dataPacket.getTotalLen(), 0, 
                           (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "Failed to send data: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Sent " << bytesSent << " bytes to server" << std::endl;
    clientSeq++;  // Update sequence number

    // 6. 接收服务端的回显数据
    char recvBuffer[MAX_PACKET_SIZE];
    sockaddr_in fromAddr;
    int fromAddrLen = sizeof(fromAddr);
    
    // 设置接收超时
    int timeout = 5000;  // 5秒超时
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    int bytesReceived = recvfrom(clientSocket, recvBuffer, MAX_PACKET_SIZE, 0, 
                                 (sockaddr*)&fromAddr, &fromAddrLen);
    if (bytesReceived == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            std::cerr << "Receive timeout" << std::endl;
        } else {
            std::cerr << "Receive failed: " << WSAGetLastError() << std::endl;
        }
    } else {
        Packet recvPacket;
        if (recvPacket.deserialize(recvBuffer, bytesReceived)) {
            recvPacket.data[recvPacket.dataLen] = '\0';
            std::cout << "Received server echo: " << recvPacket.data << std::endl;
            std::cout << "Received " << recvPacket.dataLen << " bytes of data" << std::endl;
            serverSeq = recvPacket.header.seq + 1;  // Update server sequence number
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