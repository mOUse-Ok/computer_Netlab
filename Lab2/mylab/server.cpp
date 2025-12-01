#include <iostream>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义
#include "protocol.h"  // 引入自定义协议头文件

#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库

#define PORT 8888  // 服务端监听端口
#define BUFFER_SIZE 1024  // 缓冲区大小

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
        std::cout << "[错误] 数据包校验失败" << std::endl;
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

    // 6. 连接已建立，接收和处理数据
    ConnectionState state = ESTABLISHED;
    char recvBuffer[MAX_PACKET_SIZE];
    int clientAddrLen = sizeof(clientAddr);
    
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
            std::cout << "[Received] FIN packet (seq=" << recvPacket.header.seq << ")" << std::endl;
            clientSeq = recvPacket.header.seq;
            
            // Handle four-way handshake
            if (handleClose(serverSocket, clientAddr, clientSeq, serverSeq)) {
                connectionActive = false;
            }
            break;
        }
        
        // 处理普通数据包
        if (recvPacket.dataLen > 0) {
            recvPacket.data[recvPacket.dataLen] = '\0';
            std::cout << "[Received] Data packet (seq=" << recvPacket.header.seq 
                     << ", data length=" << recvPacket.dataLen << ")" << std::endl;
            std::cout << "Data content: " << recvPacket.data << std::endl;
            
            // 回显消息给客户端
            Packet echoPacket;
            echoPacket.header.seq = serverSeq;
            echoPacket.header.ack = recvPacket.header.seq + 1;
            echoPacket.header.flag = FLAG_ACK;
            echoPacket.setData(recvPacket.data, recvPacket.dataLen);
            
            char sendBuffer[MAX_PACKET_SIZE];
            echoPacket.serialize(sendBuffer);
            int bytesSent = sendto(serverSocket, sendBuffer, echoPacket.getTotalLen(), 0,
                                  (sockaddr*)&clientAddr, clientAddrLen);
            
            if (bytesSent == SOCKET_ERROR) {
                std::cerr << "[Error] Failed to send echo data: " << WSAGetLastError() << std::endl;
            } else {
                std::cout << "[Sent] Echo packet (seq=" << serverSeq << ", length=" << echoPacket.dataLen << ")" << std::endl;
                serverSeq++;
            }
        }
    }

    // 7. 清理资源
    closesocket(serverSocket);
    WSACleanup();

    std::cout << "Server program ended" << std::endl;

    return 0;
}