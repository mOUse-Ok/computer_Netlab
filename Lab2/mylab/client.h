/**
 * client.h - 客户端头文件
 * 包含客户端程序的函数声明和全局变量声明
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "config.h"
#include "protocol.h"

// 全局发送窗口：管理流水线发送的滑动窗口状态
extern SendWindow g_sendWindow;

// 流水线发送数据（支持SACK和RENO拥塞控制）
bool pipelineSend(SOCKET clientSocket, sockaddr_in& serverAddr, 
                  const char* data, int dataLen, uint32_t baseSeq);

// 三次握手：建立连接
bool handshake(SOCKET clientSocket, sockaddr_in& serverAddr, 
               uint32_t& clientSeq, uint32_t& serverSeq);

// 四次挥手：关闭连接
bool closeConnection(SOCKET clientSocket, sockaddr_in& serverAddr, 
                     uint32_t clientSeq, uint32_t serverSeq);


#endif // CLIENT_H
