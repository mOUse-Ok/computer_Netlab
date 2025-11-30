#include <iostream>
#include <winsock2.h>  // Windows Socket API头文件
#include <ws2tcpip.h>  // 包含sockaddr_in6等结构体定义

#pragma comment(lib, "ws2_32.lib")  // 链接WS2_32.lib库

#define PORT 8888  // 服务端监听端口
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

    // 5. 接收和回显客户端消息
    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr;  // 用于存储客户端地址
    int clientAddrLen = sizeof(clientAddr);

    while (true) {
        // 接收客户端消息
        // recvfrom函数用于UDP接收，返回接收的字节数
        int bytesReceived = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0, 
                                     (sockaddr*)&clientAddr, &clientAddrLen);
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "recvfrom failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        // 将接收到的数据转换为字符串并输出
        buffer[bytesReceived] = '\0';  // 添加字符串结束符
        // 将客户端IP地址从网络字节序转换为字符串格式
        std::cout << "Received from " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) 
                  << ": " << buffer << std::endl;

        // 回显消息给客户端
        // sendto函数用于UDP发送，返回发送的字节数
        int bytesSent = sendto(serverSocket, buffer, bytesReceived, 0, 
                               (sockaddr*)&clientAddr, clientAddrLen);
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        std::cout << "Echoed message to client" << std::endl;
    }

    // 6. 清理资源（实际不会执行到这里，因为上面是无限循环）
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}