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
    
    ReliableTransport server;
    
    if (!server.createSocket()) {
        wprintf(L"[ERROR][错误] 创建套接字失败\n");
        WSACleanup();
        return 1;
    }
    
    wprintf(L"[DEBUG][调试] 套接字创建成功\n");
    
    if (!server.listen(9999)) {
        wprintf(L"[ERROR][错误] 监听失败\n");
        WSACleanup();
        return 1;
    }
    
    wprintf(L"[DEBUG][调试] 绑定和监听成功\n");
    
    wprintf(L"\n=== 服务器已启动，监听端口 9999 ===\n\n");
    
    if (!server.accept()) {
        wprintf(L"[ERROR][错误] 接受连接失败\n");
        WSACleanup();
        return 1;
    }
    
    wprintf(L"\n=== 连接已建立 ===\n\n");
    
    // 接收数据
    char buffer[2048];
    int recv_len = server.recvData(buffer, sizeof(buffer) - 1);
    if (recv_len > 0) {
        buffer[recv_len] = '\0';
        wprintf(L"\n[APP][应用] 接收的数据: %hs\n", buffer);
    }
    
    // 发送响应
    const char* response = "Hello from Server";
    int send_len = server.sendData(response, (int)strlen(response));
    wprintf(L"[APP][应用] 已发送 %d 字节\n", send_len);
    
    Sleep(1000);
    
    // 关闭连接
    if (server.getState() == ESTABLISHED) {
        wprintf(L"\n[APP][应用] 等待关闭...\n");
        Sleep(1000);

        // 等待并处理远端可能的关闭请求（处理 FIN）
        if (server.getState() == ESTABLISHED) {
            printf("\n等待客户端的关闭 (检查 FIN)...\n");
            // 让 recvData 有机会接收 FIN 报文并处理
            char closebuf[256];
            server.recvData(closebuf, sizeof(closebuf)-1);
                Sleep(500);
            }
        }

        WSACleanup();
        wprintf(L"[APP][应用] 服务器已关闭\n");
        // 防止双击时立即关闭:等待用户
        wprintf(L"\n按Enter键退出...\n");
        getchar();
        return 0;
    }
