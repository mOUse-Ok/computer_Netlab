#include <iostream>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义

#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库

#define SERVER_PORT 8888  // 服务端端口
#define SERVER_IP "127.0.0.1"  // 服务端IP地址，这里使用本地回环地址
#define BUFFER_SIZE 1024  // 缓冲区大小

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

    // 4. 准备发送的数据
    const char* message = "Hello, UDP Server!";
    std::cout << "Sending message to server: " << message << std::endl;

    // 5. 发送数据到服务端
    // sendto函数用于UDP发送，返回发送的字节数
    int bytesSent = sendto(clientSocket, message, strlen(message), 0, 
                           (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Sent " << bytesSent << " bytes to server" << std::endl;

    // 6. 接收服务端的回显数据
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;  // 用于存储发送方地址（服务端地址）
    int fromAddrLen = sizeof(fromAddr);
    
    // 设置接收超时（可选），避免无限等待
    int timeout = 5000;  // 5秒超时
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    // recvfrom函数用于UDP接收，返回接收的字节数
    int bytesReceived = recvfrom(clientSocket, buffer, BUFFER_SIZE, 0, 
                                 (sockaddr*)&fromAddr, &fromAddrLen);
    if (bytesReceived == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            std::cerr << "recvfrom timed out" << std::endl;
        } else {
            std::cerr << "recvfrom failed: " << WSAGetLastError() << std::endl;
        }
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // 7. 处理接收到的回显数据
    buffer[bytesReceived] = '\0';  // 添加字符串结束符
    // 将服务端IP地址从网络字节序转换为字符串格式
    std::cout << "Received echo from " << inet_ntoa(fromAddr.sin_addr) << ":" << ntohs(fromAddr.sin_port) 
              << ": " << buffer << std::endl;
    std::cout << "Received " << bytesReceived << " bytes" << std::endl;

    // 8. 清理资源
    closesocket(clientSocket);
    WSACleanup();

    std::cout << "Client finished" << std::endl;

    return 0;
}