#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstdint>
#include <cstddef>
#include <ctime>

// Platform-specific socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2ipdef.h>
    #include <iphlpapi.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

#include "reliable_transport.h"
#include "packet.h"

// ==================== 连接状态枚举 ====================

/**
 * 连接状态定义（基于TCP状态机）
 * 
 * 状态转换图：
 * 
 * 客户端状态转换：
 *   CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
 * 
 * 服务器状态转换：
 *   CLOSED → LISTEN → SYN_RECEIVED → ESTABLISHED → CLOSE_WAIT → LAST_ACK → CLOSED
 *                                         ↑
 *                                         ↓
 *   客户端: FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT
 */
typedef enum {
    CLOSED = 0,          // 连接关闭状态，初始状态
    LISTEN = 1,          // 服务器监听状态，等待连接请求
    SYN_SENT = 2,        // 客户端已发送SYN，等待SYN-ACK
    SYN_RECEIVED = 3,    // 服务器已发送SYN-ACK，等待ACK
    ESTABLISHED = 4,     // 连接已建立，可以传输数据
    FIN_WAIT_1 = 5,      // 已发送FIN，等待ACK或FIN-ACK
    FIN_WAIT_2 = 6,      // 已收到FIN的ACK，等待对方的FIN
    TIME_WAIT = 7,       // 已收到FIN并发送ACK，等待超时
    CLOSE_WAIT = 8,      // 已收到FIN，等待上层关闭
    LAST_ACK = 9         // 已发送FIN，等待最后的ACK
} ConnectionState;

// ==================== 连接结构体 ====================

/**
 * 可靠传输协议连接结构
 * 
 * 包含连接的基本信息和状态管理
 */
typedef struct {
    // ===== 套接字和地址信息 =====
    int sockfd;                        // UDP套接字文件描述符
    struct sockaddr_in peer_addr;      // 对端地址（IP和端口）
    socklen_t peer_addr_len;           // 对端地址长度
    
    // ===== 连接状态 =====
    ConnectionState state;             // 当前连接状态
    
    // ===== 序列号和确认号 =====
    uint32_t seq_num;                  // 下一个要发送的序列号
    uint32_t ack_num;                  // 期望接收的下一个序列号
    uint32_t initial_seq_num;          // 初始序列号（用于调试）
    uint32_t peer_seq_num;             // 对端的初始序列号
    
    // ===== 流量控制 =====
    uint16_t window_size;              // 接收窗口大小（本地）
    uint16_t peer_window_size;         // 对端窗口大小
    
    // ===== 拥塞控制（RENO算法） =====
    uint32_t cwnd;                     // 拥塞窗口大小
    uint32_t ssthresh;                 // 慢启动阈值
    uint32_t inflight;                 // 已发送但未确认的字节数
    
    // ===== 超时和重传 =====
    time_t last_activity;              // 最后活动时间（秒）
    uint32_t rto;                      // 重传超时时间（毫秒）
    uint32_t rtt;                      // 往返时间估计（毫秒）
    uint32_t retransmit_count;         // 重传次数统计
    
    // ===== 统计信息 =====
    uint32_t bytes_sent;               // 已发送的总字节数
    uint32_t bytes_received;           // 已接收的总字节数
    uint32_t frames_sent;              // 已发送的帧数
    uint32_t frames_received;          // 已接收的帧数
} Connection;

// ==================== 连接管理函数 ====================

/**
 * 创建服务器连接
 * 
 * 初始化一个服务器连接，进入LISTEN状态
 * 
 * @param port 本地监听端口
 * @return 返回分配的连接指针，失败返回NULL
 */
Connection* create_server_connection(int port);

/**
 * 创建客户端连接
 * 
 * 初始化一个客户端连接，准备连接到服务器
 * 
 * @param server_ip 服务器IP地址
 * @param port 服务器端口
 * @return 返回分配的连接指针，失败返回NULL
 */
Connection* create_client_connection(const char* server_ip, int port);

/**
 * 服务器开始监听
 * 
 * 将连接状态改为LISTEN，准备接受客户端连接
 * 
 * @param conn 连接指针
 * @return 成功返回true，失败返回false
 */
bool server_listen(Connection* conn);

/**
 * 客户端发起连接（三次握手第1步）
 * 
 * 发送SYN帧，状态转为SYN_SENT
 * 
 * @param conn 连接指针
 * @return 成功返回true，失败返回false
 */
bool client_connect(Connection* conn);

/**
 * 处理接收到的SYN帧（三次握手第2步）
 * 
 * 服务器接收SYN，发送SYN-ACK，状态转为SYN_RECEIVED
 * 
 * @param conn 连接指针
 * @param frame 接收到的帧
 * @return 成功返回true，失败返回false
 */
bool handle_syn(Connection* conn, const Frame* frame);

/**
 * 处理接收到的SYN-ACK帧（三次握手第2步）
 * 
 * 客户端接收SYN-ACK，发送ACK，状态转为ESTABLISHED
 * 
 * @param conn 连接指针
 * @param frame 接收到的帧
 * @return 成功返回true，失败返回false
 */
bool handle_syn_ack(Connection* conn, const Frame* frame);

/**
 * 处理接收到的ACK帧（三次握手第3步或常规确认）
 * 
 * 服务器接收ACK（在SYN_RECEIVED状态），状态转为ESTABLISHED
 * 或更新窗口和确认号
 * 
 * @param conn 连接指针
 * @param frame 接收到的帧
 * @return 成功返回true，失败返回false
 */
bool handle_ack(Connection* conn, const Frame* frame);

/**
 * 发送FIN帧启动关闭过程（四次挥手第1步）
 * 
 * 主动关闭连接，发送FIN帧，状态转为FIN_WAIT_1
 * 
 * @param conn 连接指针
 * @return 成功返回true，失败返回false
 */
bool send_fin(Connection* conn);

/**
 * 处理接收到的FIN帧（四次挥手）
 * 
 * 被动关闭连接，发送ACK，状态转为CLOSE_WAIT
 * 
 * @param conn 连接指针
 * @param frame 接收到的帧
 * @return 成功返回true，失败返回false
 */
bool handle_fin(Connection* conn, const Frame* frame);

/**
 * 处理接收到的FIN-ACK帧（四次挥手）
 * 
 * 接收同时包含FIN和ACK的帧
 * 
 * @param conn 连接指针
 * @param frame 接收到的帧
 * @return 成功返回true，失败返回false
 */
bool handle_fin_ack(Connection* conn, const Frame* frame);

/**
 * 关闭连接
 * 
 * 优雅关闭连接，释放资源
 * 
 * @param conn 连接指针
 * @return 成功返回true，失败返回false
 */
bool close_connection(Connection* conn);

/**
 * 更新连接状态
 * 
 * 状态转换时调用，包含合法性检查和日志记录
 * 
 * @param conn 连接指针
 * @param new_state 新状态
 * @return 转换合法返回true，否则返回false
 */
bool update_connection_state(Connection* conn, ConnectionState new_state);

/**
 * 获取连接状态的字符串表示
 * 
 * @param state 连接状态
 * @return 状态的字符串描述
 */
const char* connection_state_to_string(ConnectionState state);

/**
 * 检查状态转换是否合法
 * 
 * @param from_state 当前状态
 * @param to_state 目标状态
 * @return 合法返回true，否则返回false
 */
bool is_valid_state_transition(ConnectionState from_state, ConnectionState to_state);

/**
 * 获取连接统计信息
 * 
 * @param conn 连接指针
 * @param sent 已发送字节数指针
 * @param received 已接收字节数指针
 * @param retransmits 重传次数指针
 * @return 成功返回true，失败返回false
 */
bool connection_get_stats(const Connection* conn, uint32_t* sent, uint32_t* received, uint32_t* retransmits);

/**
 * 打印连接信息（调试用）
 * 
 * @param conn 连接指针
 */
void connection_print(const Connection* conn);

/**
 * 释放连接资源
 * 
 * @param conn 连接指针
 */
void connection_free(Connection* conn);

#endif // CONNECTION_H
