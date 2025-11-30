#include "connection.h"
#include "utils.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <time.h>

// Platform-specific headers
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

// ==================== 日志宏定义 ====================

#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// ==================== 连接状态转换表 ====================

/**
 * 合法的连接状态转换
 * 从状态 → 到状态集合
 */
static const int valid_transitions[10][10] = {
    // CLOSED
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    // LISTEN
    {0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    // SYN_SENT
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    // SYN_RECEIVED
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
    // ESTABLISHED
    {0, 0, 0, 0, 0, 1, 0, 0, 1, 0},
    // FIN_WAIT_1
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0},
    // FIN_WAIT_2
    {1, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    // TIME_WAIT
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    // CLOSE_WAIT
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    // LAST_ACK
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// ==================== 状态字符串转换 ====================

/**
 * 获取连接状态的字符串表示
 */
const char* connection_state_to_string(ConnectionState state)
{
    switch (state) {
        case CLOSED:
            return "CLOSED";
        case LISTEN:
            return "LISTEN";
        case SYN_SENT:
            return "SYN_SENT";
        case SYN_RECEIVED:
            return "SYN_RECEIVED";
        case ESTABLISHED:
            return "ESTABLISHED";
        case FIN_WAIT_1:
            return "FIN_WAIT_1";
        case FIN_WAIT_2:
            return "FIN_WAIT_2";
        case TIME_WAIT:
            return "TIME_WAIT";
        case CLOSE_WAIT:
            return "CLOSE_WAIT";
        case LAST_ACK:
            return "LAST_ACK";
        default:
            return "UNKNOWN";
    }
}

// ==================== 状态转换验证 ====================

/**
 * 检查状态转换是否合法
 * 
 * @param from_state 当前状态
 * @param to_state 目标状态
 * @return 合法返回true，否则返回false
 */
bool is_valid_state_transition(ConnectionState from_state, ConnectionState to_state)
{
    // 检查状态值范围
    if (from_state < 0 || from_state > 9 || to_state < 0 || to_state > 9) {
        return false;
    }

    return valid_transitions[from_state][to_state] != 0;
}

// ==================== 更新连接状态 ====================

/**
 * 更新连接状态并进行合法性检查
 * 
 * @param conn 连接指针
 * @param new_state 新状态
 * @return 转换合法返回true，否则返回false
 */
bool update_connection_state(Connection* conn, ConnectionState new_state)
{
    if (conn == NULL) {
        LOG_ERROR("Connection pointer is NULL");
        return false;
    }

    // 检查状态转换是否合法
    if (!is_valid_state_transition(conn->state, new_state)) {
        LOG_WARN("Invalid state transition: %s → %s",
                 connection_state_to_string(conn->state),
                 connection_state_to_string(new_state));
        return false;
    }

    // 记录状态转换
    LOG_INFO("State transition: %s → %s",
             connection_state_to_string(conn->state),
             connection_state_to_string(new_state));

    conn->state = new_state;
    conn->last_activity = time(NULL);

    return true;
}

// ==================== 连接创建 ====================

/**
 * 创建服务器连接
 */
Connection* create_server_connection(int port)
{
    // 分配连接结构体
    Connection* conn = (Connection*)malloc(sizeof(Connection));
    if (conn == NULL) {
        LOG_ERROR("Failed to allocate connection structure");
        return NULL;
    }

    // 初始化连接字段
    memset(conn, 0, sizeof(Connection));

    // 设置套接字和地址信息
    conn->sockfd = -1;  // 实际套接字创建在UDP层
    conn->peer_addr_len = sizeof(struct sockaddr_in);
    memset(&conn->peer_addr, 0, sizeof(struct sockaddr_in));

    // 设置连接状态
    conn->state = CLOSED;

    // 初始化序列号（服务器初始序列号）
    conn->seq_num = generate_random_seq();
    conn->initial_seq_num = conn->seq_num;
    conn->peer_seq_num = 0;

    // 初始化窗口大小
    conn->window_size = WINDOW_SIZE;
    conn->peer_window_size = 0;

    // 初始化拥塞控制（RENO算法）
    conn->cwnd = 1 * MAX_DATA_LENGTH;      // 初始拥塞窗口 = 1 MSS
    conn->ssthresh = 65535;                 // 初始阈值（很大）
    conn->inflight = 0;

    // 初始化超时和重传
    conn->rto = TIMEOUT_MS;
    conn->rtt = TIMEOUT_MS;
    conn->retransmit_count = 0;
    conn->last_activity = time(NULL);

    // 初始化统计信息
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
    conn->frames_sent = 0;
    conn->frames_received = 0;

    LOG_INFO("Server connection created, port=%d, initial_seq=%u", port, conn->seq_num);

    return conn;
}

/**
 * 创建客户端连接
 */
Connection* create_client_connection(const char* server_ip, int port)
{
    // 输入验证
    if (server_ip == NULL) {
        LOG_ERROR("Server IP address is NULL");
        return NULL;
    }

    // 分配连接结构体
    Connection* conn = (Connection*)malloc(sizeof(Connection));
    if (conn == NULL) {
        LOG_ERROR("Failed to allocate connection structure");
        return NULL;
    }

    // 初始化连接字段
    memset(conn, 0, sizeof(Connection));

    // 设置套接字和地址信息
    conn->sockfd = -1;  // 实际套接字创建在UDP层
    conn->peer_addr_len = sizeof(struct sockaddr_in);
    memset(&conn->peer_addr, 0, sizeof(struct sockaddr_in));
    conn->peer_addr.sin_family = AF_INET;
    conn->peer_addr.sin_port = htons(port);
    conn->peer_addr.sin_addr.s_addr = inet_addr(server_ip);

    // 设置连接状态
    conn->state = CLOSED;

    // 初始化序列号（客户端初始序列号）
    conn->seq_num = generate_random_seq();
    conn->initial_seq_num = conn->seq_num;
    conn->peer_seq_num = 0;

    // 初始化窗口大小
    conn->window_size = WINDOW_SIZE;
    conn->peer_window_size = 0;

    // 初始化拥塞控制（RENO算法）
    conn->cwnd = 1 * MAX_DATA_LENGTH;      // 初始拥塞窗口 = 1 MSS
    conn->ssthresh = 65535;                 // 初始阈值（很大）
    conn->inflight = 0;

    // 初始化超时和重传
    conn->rto = TIMEOUT_MS;
    conn->rtt = TIMEOUT_MS;
    conn->retransmit_count = 0;
    conn->last_activity = time(NULL);

    // 初始化统计信息
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
    conn->frames_sent = 0;
    conn->frames_received = 0;

    LOG_INFO("Client connection created, server=%s:%d, initial_seq=%u", server_ip, port, conn->seq_num);

    return conn;
}

// ==================== 三次握手：服务器侧 ====================

/**
 * 服务器开始监听
 */
bool server_listen(Connection* conn)
{
    if (conn == NULL) {
        LOG_ERROR("Connection pointer is NULL");
        return false;
    }

    // 状态转换：CLOSED → LISTEN
    if (!update_connection_state(conn, LISTEN)) {
        LOG_ERROR("Failed to transition to LISTEN state");
        return false;
    }

    LOG_INFO("Server listening for incoming connections");
    return true;
}

/**
 * 处理接收到的SYN帧（服务器接收连接请求）
 * 
 * 三次握手第2步：
 * 1. 接收客户端的SYN帧
 * 2. 提取客户端的初始序列号
 * 3. 发送SYN-ACK响应
 * 4. 状态转为SYN_RECEIVED
 */
bool handle_syn(Connection* conn, const Frame* frame)
{
    if (conn == NULL || frame == NULL) {
        LOG_ERROR("Connection or frame pointer is NULL");
        return false;
    }

    // 检查当前状态
    if (conn->state != LISTEN) {
        LOG_WARN("Received SYN in non-LISTEN state: %s", connection_state_to_string(conn->state));
        return false;
    }

    // 检查帧类型
    if (frame->frame_type != SYN) {
        LOG_ERROR("Expected SYN frame, got %s", frame_type_to_string((FrameType)frame->frame_type));
        return false;
    }

    // 保存客户端的初始序列号
    conn->peer_seq_num = frame->seq_num;
    conn->ack_num = frame->seq_num + 1;  // 期望接收的下一个序列号
    conn->peer_window_size = frame->window_size;

    LOG_INFO("Received SYN from client: seq=%u, window=%u", frame->seq_num, frame->window_size);

    // 状态转换：LISTEN → SYN_RECEIVED
    if (!update_connection_state(conn, SYN_RECEIVED)) {
        LOG_ERROR("Failed to transition to SYN_RECEIVED state");
        return false;
    }

    // 构造SYN-ACK响应帧
    Frame response;
    response = create_frame(conn->seq_num, conn->ack_num, conn->window_size,
                            SYN_ACK, NULL, 0);

    LOG_INFO("Sending SYN-ACK: seq=%u, ack=%u, window=%u", response.seq_num, response.ack_num, response.window_size);

    // 这里应该发送响应帧（实现在UDP层）
    conn->frames_sent++;

    return true;
}

// ==================== 三次握手：客户端侧 ====================

/**
 * 客户端发起连接
 * 
 * 三次握手第1步：
 * 1. 发送SYN帧
 * 2. 状态转为SYN_SENT
 * 3. 等待SYN-ACK响应
 */
bool client_connect(Connection* conn)
{
    if (conn == NULL) {
        LOG_ERROR("Connection pointer is NULL");
        return false;
    }

    // 检查当前状态
    if (conn->state != CLOSED) {
        LOG_WARN("Connection is not in CLOSED state: %s", connection_state_to_string(conn->state));
        return false;
    }

    // 状态转换：CLOSED → SYN_SENT
    if (!update_connection_state(conn, SYN_SENT)) {
        LOG_ERROR("Failed to transition to SYN_SENT state");
        return false;
    }

    // 构造SYN帧
    Frame syn_frame;
    syn_frame = create_frame(conn->seq_num, 0, conn->window_size,
                             SYN, NULL, 0);

    LOG_INFO("Sending SYN: seq=%u, window=%u", syn_frame.seq_num, syn_frame.window_size);

    // 这里应该发送SYN帧（实现在UDP层）
    conn->frames_sent++;
    conn->last_activity = time(NULL);

    return true;
}

/**
 * 处理接收到的SYN-ACK帧（客户端接收服务器响应）
 * 
 * 三次握手第2步：
 * 1. 接收服务器的SYN-ACK帧
 * 2. 提取服务器的初始序列号和窗口大小
 * 3. 发送ACK确认
 * 4. 状态转为ESTABLISHED
 */
bool handle_syn_ack(Connection* conn, const Frame* frame)
{
    if (conn == NULL || frame == NULL) {
        LOG_ERROR("Connection or frame pointer is NULL");
        return false;
    }

    // 检查当前状态
    if (conn->state != SYN_SENT) {
        LOG_WARN("Received SYN-ACK in non-SYN_SENT state: %s", connection_state_to_string(conn->state));
        return false;
    }

    // 检查帧类型
    if (frame->frame_type != SYN_ACK) {
        LOG_ERROR("Expected SYN-ACK frame, got %s", frame_type_to_string((FrameType)frame->frame_type));
        return false;
    }

    // 验证ACK号（应该是我们发送的序列号+1）
    if (frame->ack_num != conn->seq_num + 1) {
        LOG_WARN("ACK number mismatch: expected=%u, got=%u", conn->seq_num + 1, frame->ack_num);
        return false;
    }

    // 保存服务器的初始序列号
    conn->peer_seq_num = frame->seq_num;
    conn->ack_num = frame->seq_num + 1;  // 期望接收的下一个序列号
    conn->peer_window_size = frame->window_size;

    LOG_INFO("Received SYN-ACK from server: seq=%u, ack=%u, window=%u", 
             frame->seq_num, frame->ack_num, frame->window_size);

    // 状态转换：SYN_SENT → ESTABLISHED
    if (!update_connection_state(conn, ESTABLISHED)) {
        LOG_ERROR("Failed to transition to ESTABLISHED state");
        return false;
    }

    // 构造ACK响应帧
    Frame ack_frame;
    ack_frame = create_frame(conn->seq_num, conn->ack_num, conn->window_size,
                             ACK, NULL, 0);

    LOG_INFO("Sending ACK: seq=%u, ack=%u", ack_frame.seq_num, ack_frame.ack_num);

    // 这里应该发送ACK帧（实现在UDP层）
    conn->frames_sent++;

    return true;
}

/**
 * 处理接收到的ACK帧（服务器接收最后的ACK）
 * 
 * 三次握手第3步或常规数据确认：
 * 1. 在SYN_RECEIVED状态接收ACK→转为ESTABLISHED
 * 2. 在ESTABLISHED状态接收ACK→更新窗口
 */
bool handle_ack(Connection* conn, const Frame* frame)
{
    if (conn == NULL || frame == NULL) {
        LOG_ERROR("Connection or frame pointer is NULL");
        return false;
    }

    // 检查帧类型
    if (frame->frame_type != ACK) {
        LOG_ERROR("Expected ACK frame, got %s", frame_type_to_string((FrameType)frame->frame_type));
        return false;
    }

    // 状态相关处理
    if (conn->state == SYN_RECEIVED) {
        // 三次握手第3步：服务器收到ACK，建立连接
        
        // 验证ACK号（应该是我们发送的序列号+1）
        if (frame->ack_num != conn->seq_num + 1) {
            LOG_WARN("ACK number mismatch in SYN_RECEIVED: expected=%u, got=%u", 
                     conn->seq_num + 1, frame->ack_num);
            return false;
        }

        LOG_INFO("Received ACK from client: seq=%u, ack=%u", frame->seq_num, frame->ack_num);

        // 状态转换：SYN_RECEIVED → ESTABLISHED
        if (!update_connection_state(conn, ESTABLISHED)) {
            LOG_ERROR("Failed to transition to ESTABLISHED state");
            return false;
        }

        conn->peer_window_size = frame->window_size;

    } else if (conn->state == ESTABLISHED) {
        // 常规数据确认或连接状态帧
        
        // 更新ACK号和窗口大小
        conn->ack_num = frame->seq_num + 1;
        conn->peer_window_size = frame->window_size;

        LOG_INFO("Received ACK in ESTABLISHED: seq=%u, ack=%u, window=%u",
                 frame->seq_num, frame->ack_num, frame->window_size);

    } else {
        LOG_WARN("Received ACK in unexpected state: %s", connection_state_to_string(conn->state));
        return false;
    }

    conn->frames_received++;
    return true;
}

// ==================== 四次挥手 ====================

/**
 * 发送FIN帧启动关闭（主动关闭）
 * 
 * 四次挥手第1步：
 * 1. 发送FIN帧
 * 2. 状态转为FIN_WAIT_1
 */
bool send_fin(Connection* conn)
{
    if (conn == NULL) {
        LOG_ERROR("Connection pointer is NULL");
        return false;
    }

    // 检查当前状态
    if (conn->state != ESTABLISHED) {
        LOG_WARN("Connection not in ESTABLISHED state: %s", connection_state_to_string(conn->state));
        return false;
    }

    // 状态转换：ESTABLISHED → FIN_WAIT_1
    if (!update_connection_state(conn, FIN_WAIT_1)) {
        LOG_ERROR("Failed to transition to FIN_WAIT_1 state");
        return false;
    }

    // 构造FIN帧
    Frame fin_frame;
    fin_frame = create_frame(conn->seq_num, conn->ack_num, conn->window_size,
                             FIN, NULL, 0);

    LOG_INFO("Sending FIN: seq=%u, ack=%u", fin_frame.seq_num, fin_frame.ack_num);

    // 这里应该发送FIN帧（实现在UDP层）
    conn->frames_sent++;
    conn->seq_num++;  // FIN消耗一个序列号

    return true;
}

/**
 * 处理接收到的FIN帧（被动关闭）
 * 
 * 四次挥手第2步：
 * 1. 从ESTABLISHED状态接收FIN
 * 2. 发送ACK
 * 3. 状态转为CLOSE_WAIT
 */
bool handle_fin(Connection* conn, const Frame* frame)
{
    if (conn == NULL || frame == NULL) {
        LOG_ERROR("Connection or frame pointer is NULL");
        return false;
    }

    // 检查帧类型
    if (frame->frame_type != FIN) {
        LOG_ERROR("Expected FIN frame, got %s", frame_type_to_string((FrameType)frame->frame_type));
        return false;
    }

    // 检查当前状态
    if (conn->state == ESTABLISHED) {
        // 被动关闭：收到FIN，进入CLOSE_WAIT
        
        conn->ack_num = frame->seq_num + 1;  // 确认FIN

        LOG_INFO("Received FIN from peer: seq=%u", frame->seq_num);

        // 构造ACK响应
        Frame ack_frame;
        ack_frame = create_frame(conn->seq_num, conn->ack_num, conn->window_size,
                                 ACK, NULL, 0);

        LOG_INFO("Sending ACK: seq=%u, ack=%u", ack_frame.seq_num, ack_frame.ack_num);

        // 这里应该发送ACK帧（实现在UDP层）
        conn->frames_sent++;

        // 状态转换：ESTABLISHED → CLOSE_WAIT
        if (!update_connection_state(conn, CLOSE_WAIT)) {
            LOG_ERROR("Failed to transition to CLOSE_WAIT state");
            return false;
        }

    } else if (conn->state == FIN_WAIT_1) {
        // 主动关闭期间收到FIN：同时关闭，进入TIME_WAIT
        
        conn->ack_num = frame->seq_num + 1;  // 确认FIN

        LOG_INFO("Received FIN while in FIN_WAIT_1: seq=%u", frame->seq_num);

        // 构造ACK响应
        Frame ack_frame;
        ack_frame = create_frame(conn->seq_num, conn->ack_num, conn->window_size,
                                 ACK, NULL, 0);

        LOG_INFO("Sending ACK: seq=%u, ack=%u", ack_frame.seq_num, ack_frame.ack_num);

        // 这里应该发送ACK帧（实现在UDP层）
        conn->frames_sent++;

        // 状态转换：FIN_WAIT_1 → TIME_WAIT
        if (!update_connection_state(conn, TIME_WAIT)) {
            LOG_ERROR("Failed to transition to TIME_WAIT state");
            return false;
        }

    } else {
        LOG_WARN("Received FIN in unexpected state: %s", connection_state_to_string(conn->state));
        return false;
    }

    conn->frames_received++;
    return true;
}

/**
 * 处理接收到的FIN-ACK帧
 * 
 * 某些情况下可能同时接收FIN和ACK
 */
bool handle_fin_ack(Connection* conn, const Frame* frame)
{
    if (conn == NULL || frame == NULL) {
        LOG_ERROR("Connection or frame pointer is NULL");
        return false;
    }

    // 这是FIN-ACK帧的特殊处理
    // 一般情况下分别处理FIN和ACK
    
    LOG_WARN("FIN-ACK frame handling not fully implemented");
    return false;
}

/**
 * 关闭连接并释放资源
 */
bool close_connection(Connection* conn)
{
    if (conn == NULL) {
        LOG_ERROR("Connection pointer is NULL");
        return false;
    }

    // 根据当前状态进行不同处理
    switch (conn->state) {
        case ESTABLISHED:
            // 主动关闭
            send_fin(conn);
            break;

        case CLOSE_WAIT:
            // 被动关闭，现在发送FIN
            {
                Frame fin_frame;
                fin_frame = create_frame(conn->seq_num, conn->ack_num, conn->window_size,
                                         FIN, NULL, 0);
                LOG_INFO("Sending FIN from CLOSE_WAIT: seq=%u", fin_frame.seq_num);
                conn->frames_sent++;
                conn->seq_num++;
                update_connection_state(conn, LAST_ACK);
            }
            break;

        case TIME_WAIT:
            // 等待超时后再关闭
            update_connection_state(conn, CLOSED);
            break;

        case LAST_ACK:
            // 等待最后的ACK
            break;

        default:
            if (conn->state != CLOSED) {
                update_connection_state(conn, CLOSED);
            }
            break;
    }

    LOG_INFO("Connection closed");
    return true;
}

// ==================== 连接信息和统计 ====================

/**
 * 获取连接统计信息
 */
bool connection_get_stats(const Connection* conn, uint32_t* sent, uint32_t* received, uint32_t* retransmits)
{
    if (conn == NULL || sent == NULL || received == NULL || retransmits == NULL) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    *sent = conn->bytes_sent;
    *received = conn->bytes_received;
    *retransmits = conn->retransmit_count;

    return true;
}

/**
 * 打印连接信息（调试用）
 */
void connection_print(const Connection* conn)
{
    if (conn == NULL) {
        printf("Error: connection pointer is NULL\n");
        return;
    }

    printf("========== Connection Information ==========\n");
    printf("State:             %s\n", connection_state_to_string(conn->state));
    printf("Local Seq:         %u\n", conn->seq_num);
    printf("Remote Seq:        %u\n", conn->peer_seq_num);
    printf("ACK Number:        %u\n", conn->ack_num);
    printf("Window Size:       %u\n", conn->window_size);
    printf("Peer Window:       %u\n", conn->peer_window_size);
    printf("CWND:              %u\n", conn->cwnd);
    printf("SSTHRESH:          %u\n", conn->ssthresh);
    printf("RTT:               %u ms\n", conn->rtt);
    printf("RTO:               %u ms\n", conn->rto);
    printf("Bytes Sent:        %u\n", conn->bytes_sent);
    printf("Bytes Received:    %u\n", conn->bytes_received);
    printf("Frames Sent:       %u\n", conn->frames_sent);
    printf("Frames Received:   %u\n", conn->frames_received);
    printf("Retransmissions:   %u\n", conn->retransmit_count);
    printf("===========================================\n");
}

/**
 * 释放连接资源
 */
void connection_free(Connection* conn)
{
    if (conn == NULL) {
        return;
    }

    // 关闭连接
    if (conn->state != CLOSED) {
        close_connection(conn);
    }

    // 关闭套接字
    if (conn->sockfd >= 0) {
        // close(conn->sockfd);  // 实际实现
        conn->sockfd = -1;
    }

    // 释放内存
    free(conn);

    LOG_INFO("Connection freed");
}
