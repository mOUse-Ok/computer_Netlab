/**
 * ============================================================================
 * 服务端头文件：server.h
 * ============================================================================
 * 描述：包含服务端程序的所有函数声明、全局变量外部声明
 * 说明：
 *   - 将函数声明与实现分离，使项目结构更清晰
 *   - 便于代码维护和模块化开发
 * 
 * 作者：Lab2 Project
 * 日期：2025-12-10
 * ============================================================================
 */

#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "config.h"
#include "protocol.h"

// ============================================================================
// 全局变量声明（定义在server.cpp中）
// ============================================================================

/**
 * 全局接收窗口
 * 描述：用于管理流水线接收的滑动窗口状态
 */
extern RecvWindow g_recvWindow;

/**
 * 模拟丢包标志
 * 描述：用于测试SACK功能，设置为true时会丢弃特定序列号的包
 * 说明：默认开启一次性丢包用于触发客户端的快速重传测试
 */
extern bool g_simulateLoss;

/**
 * 要丢弃的序列号
 * 描述：配合g_simulateLoss使用，指定要丢弃的数据包序列号
 */
extern uint32_t g_lossSeq;


// ============================================================================
// 函数声明
// ============================================================================

/**
 * ===== 发送ACK/SACK响应 =====
 * 描述：发送确认包，支持累积确认和选择确认
 * 参数：
 *   serverSocket - 服务端套接字
 *   clientAddr   - 客户端地址
 *   addrLen      - 地址结构长度
 *   ackNum       - 确认号（期望接收的下一个序列号）
 *   serverSeq    - 服务端序列号
 *   useSACK      - 是否使用选择确认
 */
void sendACK(SOCKET serverSocket, sockaddr_in& clientAddr, int addrLen,
             uint32_t ackNum, uint32_t serverSeq, bool useSACK);

/**
 * ===== 流水线接收数据函数（支持SACK） =====
 * 描述：使用滑动窗口接收数据，支持选择确认和固定窗口流量控制
 * 参数：
 *   serverSocket - 服务端套接字
 *   clientAddr   - 客户端地址
 *   addrLen      - 地址结构长度
 *   baseSeq      - 起始序列号（期望接收的第一个包序列号）
 *   serverSeq    - 服务端序列号（用于发送ACK）
 *   finReceived  - 输出参数：是否收到FIN包
 *   finSeq       - 输出参数：FIN包的序列号
 * 返回值：接收到的总数据长度，失败返回-1
 */
int pipelineRecv(SOCKET serverSocket, sockaddr_in& clientAddr, int addrLen,
                 uint32_t baseSeq, uint32_t& serverSeq, bool& finReceived, uint32_t& finSeq);

/**
 * ===== 服务端处理三次握手 =====
 * 描述：处理客户端的连接请求，完成三次握手建立连接
 * 参数：
 *   serverSocket - 服务端套接字
 *   clientAddr   - 输出参数：客户端地址
 *   clientSeq    - 输出参数：客户端序列号
 *   serverSeq    - 输出参数：服务端序列号
 * 返回值：true表示连接建立成功，false表示失败
 */
bool acceptConnection(SOCKET serverSocket, sockaddr_in& clientAddr, 
                      uint32_t& clientSeq, uint32_t& serverSeq);

/**
 * ===== 服务端处理四次挥手（被动关闭） =====
 * 描述：处理客户端的关闭请求，完成四次挥手关闭连接
 * 参数：
 *   serverSocket - 服务端套接字
 *   clientAddr   - 客户端地址
 *   clientSeq    - 客户端序列号（FIN包的序列号）
 *   serverSeq    - 服务端序列号
 * 返回值：true表示关闭成功，false表示失败
 */
bool handleClose(SOCKET serverSocket, sockaddr_in& clientAddr, 
                 uint32_t clientSeq, uint32_t serverSeq);


// ============================================================================
// 服务端配置常量（从config.h引用）
// ============================================================================
// 注意：以下常量已在config.h中定义，这里仅作说明
// 
// PORT                - 服务端监听端口
// BUFFER_SIZE         - 缓冲区大小
// FIXED_WINDOW_SIZE   - 滑动窗口大小
// TIMEOUT_MS          - 超时时间
// SACK_TIMEOUT_MS     - SACK超时重传时间
// MAX_SACK_BLOCKS     - 最大SACK块数量


#endif // SERVER_H
