/**
 * ============================================================================
 * 客户端头文件：client.h
 * ============================================================================
 * 描述：包含客户端程序的所有函数声明、全局变量外部声明
 * 说明：
 *   - 将函数声明与实现分离，使项目结构更清晰
 *   - 便于代码维护和模块化开发
 * 
 * 作者：Lab2 Project
 * 日期：2025-12-10
 * ============================================================================
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include "config.h"
#include "protocol.h"

// ============================================================================
// 全局变量声明（定义在client.cpp中）
// ============================================================================

/**
 * 全局发送窗口
 * 描述：用于管理流水线发送的滑动窗口状态
 */
extern SendWindow g_sendWindow;


// ============================================================================
// 函数声明
// ============================================================================

/**
 * ===== 流水线发送数据函数（支持SACK） =====
 * 描述：使用流水线方式发送大量数据，支持选择确认和固定窗口流量控制
 * 参数：
 *   clientSocket - 客户端套接字
 *   serverAddr   - 服务端地址
 *   data         - 要发送的数据
 *   dataLen      - 数据长度
 *   baseSeq      - 起始序列号
 * 返回值：true表示发送成功，false表示发送失败
 */
bool pipelineSend(SOCKET clientSocket, sockaddr_in& serverAddr, 
                  const char* data, int dataLen, uint32_t baseSeq);

/**
 * ===== 三次握手函数：建立连接 =====
 * 描述：客户端发起连接请求，完成三次握手建立TCP连接
 * 参数：
 *   clientSocket - 客户端套接字
 *   serverAddr   - 服务端地址
 *   clientSeq    - 输出参数：客户端序列号（握手后更新）
 *   serverSeq    - 输出参数：服务端序列号
 * 返回值：true表示连接成功，false表示连接失败
 */
bool handshake(SOCKET clientSocket, sockaddr_in& serverAddr, 
               uint32_t& clientSeq, uint32_t& serverSeq);

/**
 * ===== 四次挥手函数：关闭连接 =====
 * 描述：客户端主动发起关闭请求，完成四次挥手关闭连接
 * 参数：
 *   clientSocket - 客户端套接字
 *   serverAddr   - 服务端地址
 *   clientSeq    - 客户端序列号
 *   serverSeq    - 服务端序列号
 * 返回值：true表示关闭成功，false表示关闭失败
 */
bool closeConnection(SOCKET clientSocket, sockaddr_in& serverAddr, 
                     uint32_t clientSeq, uint32_t serverSeq);


// ============================================================================
// 客户端配置常量（从config.h引用）
// ============================================================================
// 注意：以下常量已在config.h中定义，这里仅作说明
// 
// SERVER_PORT         - 服务端端口
// SERVER_IP           - 服务端IP地址
// BUFFER_SIZE         - 缓冲区大小
// FIXED_WINDOW_SIZE   - 滑动窗口大小
// TIMEOUT_MS          - 超时时间
// MAX_RETRIES         - 最大重传次数
// TIME_WAIT_MS        - TIME_WAIT状态等待时间
// SACK_TIMEOUT_MS     - SACK超时重传时间
// 
// RENO拥塞控制参数：
// INITIAL_CWND        - 初始拥塞窗口大小
// INITIAL_SSTHRESH    - 初始慢启动阈值
// MIN_SSTHRESH        - 最小慢启动阈值
// DUP_ACK_THRESHOLD   - 重复ACK阈值（快速重传触发条件）


#endif // CLIENT_H
