#include "reliable_transport.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <windows.h>

int main() {
    // Set console code page to UTF-8
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    
    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        wprintf(L"[ERROR][错误] WSAStartup失败: %d\n", WSAGetLastError());
        return 1;
    }
    
    ReliableTransport client;
    
    if (!client.createSocket()) {
        wprintf(L"[ERROR][错误] 创建套接字失败\n");
        WSACleanup();
        return 1;
    }
    
    wprintf(L"[DEBUG][调试] 套接字创建成功\n");
    
    // 绑定本地地址以接收响应
    // 使用INADDR_ANY允许在任何接口上接收
    if (!client.bind("", 0)) {
        wprintf(L"[ERROR][错误] 绑定失败\n");
        WSACleanup();
        return 1;
    }
    
    wprintf(L"[DEBUG][调试] 绑定成功\n");
    
    wprintf(L"\n=== 客户端已启动 ===\n\n");
    wprintf(L"[APP][应用] 正在连接到服务器 127.0.0.1:9999...\n");
    
    if (!client.connect("127.0.0.1", 9999)) {
        wprintf(L"[ERROR][错误] 连接失败\n");
        WSACleanup();
        return 1;
    }
    
    wprintf(L"\n=== 连接已建立 ===\n\n");
    
    // Send data
    const char* message = "Hello from Client";
    int send_len = client.sendData(message, (int)strlen(message));
    wprintf(L"[APP][应用] 已发送 %d 字节\n", send_len);
    
    Sleep(1000);
    
    // 接收响应
    char buffer[2048];
    int recv_len = client.recvData(buffer, sizeof(buffer) - 1);
    if (recv_len > 0) {
        buffer[recv_len] = '\0';
        wprintf(L"\n[APP][应用] 接收的数据: %hs\n", buffer);
    }
    
    Sleep(1000);
    
    // 关闭连接
    wprintf(L"\n[APP][应用] 正在关闭连接...\n");
    if (client.closeConnection()) {
        wprintf(L"[APP][应用] 连接已关闭\n");
    }
    
    WSACleanup();
    // 防止双击时立即关闭:等待用户
    wprintf(L"\n按Enter键退出...\n");
    getchar();
    return 0;
}
