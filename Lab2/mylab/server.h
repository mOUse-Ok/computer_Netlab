/**
 * server.h - 服务端头文件
 * 包含服务端程序的函数声明和全局变量声明
 */

#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "config.h"
#include "protocol.h"

// 全局接收窗口：管理流水线接收的滑动窗口状态
extern RecvWindow g_recvWindow;

// 发送ACK/SACK响应：支持累积确认和选择确认
void sendACK(SOCKET serverSocket, sockaddr_in& clientAddr, int addrLen,
             uint32_t ackNum, uint32_t serverSeq, bool useSACK);

// 流水线接收数据（支持SACK）：使用滑动窗口接收数据
// outBuffer: 输出缓冲区，用于存储接收到的数据
// outBufferSize: 输出缓冲区大小
int pipelineRecv(SOCKET serverSocket, sockaddr_in& clientAddr, int addrLen,
                 uint32_t baseSeq, uint32_t& serverSeq, bool& finReceived, uint32_t& finSeq,
                 char* outBuffer, int outBufferSize);

// 服务端三次握手：处理客户端连接请求
bool acceptConnection(SOCKET serverSocket, sockaddr_in& clientAddr, 
                      uint32_t& clientSeq, uint32_t& serverSeq);

// 服务端四次挥手（被动关闭）：处理客户端关闭请求
bool handleClose(SOCKET serverSocket, sockaddr_in& clientAddr, 
                 uint32_t clientSeq, uint32_t serverSeq);

#endif // SERVER_H
