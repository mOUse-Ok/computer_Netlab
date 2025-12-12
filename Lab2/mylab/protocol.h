#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <ctime>
#include <cstring>
#include "config.h"  // 引入配置文件，所有可配置参数集中在config.h中管理

// ===== 连接状态枚举 =====
// 描述：用于跟踪连接的当前状态，模拟TCP状态机
enum ConnectionState {
    CLOSED,        // 关闭状态：初始状态，无连接
    SYN_SENT,      // 客户端：已发送SYN，等待SYN+ACK
    SYN_RCVD,      // 服务端：已接收SYN，已发送SYN+ACK，等待ACK
    ESTABLISHED,   // 连接已建立：可以进行数据传输
    FIN_WAIT_1,    // 主动关闭方：已发送FIN，等待ACK
    FIN_WAIT_2,    // 主动关闭方：已收到FIN的ACK，等待对方FIN
    TIME_WAIT,     // 主动关闭方：已发送最后的ACK，等待2MSL
    CLOSE_WAIT,    // 被动关闭方：已收到FIN，已发送ACK，等待本地关闭
    LAST_ACK       // 被动关闭方：已发送FIN，等待最后的ACK
};

// ===== RENO 拥塞控制阶段枚举 =====
// 描述：用于跟踪 TCP RENO 拥塞控制算法的当前阶段
enum RenoPhase {
    SLOW_START,           // 慢启动阶段：cwnd 指数增长
    CONGESTION_AVOIDANCE, // 拥塞避免阶段：cwnd 线性增长
    FAST_RECOVERY         // 快速恢复阶段：收到重复 ACK 后的恢复过程
};

// ===== 标志位定义 =====
// 描述：用于标识数据包类型，可以组合使用（如 FLAG_SYN | FLAG_ACK）
#define FLAG_SYN  0x01  // 同步标志（0000 0001）：用于建立连接
#define FLAG_ACK  0x02  // 确认标志（0000 0010）：用于确认收到数据
#define FLAG_FIN  0x04  // 结束标志（0000 0100）：用于关闭连接
#define FLAG_SACK 0x08  // 选择确认标志（0000 1000）：用于选择确认功能

// ===== 协议常量说明 =====
// 注意：以下常量已移动到 config.h 中进行统一管理，便于调试和测试
// 
// 协议基础参数（config.h）：
//   HEADER_SIZE         - 协议头大小：20字节
//   MAX_PACKET_SIZE     - 最大数据包大小
//   MAX_DATA_SIZE       - 最大数据负载大小
//   DEFAULT_WINDOW_SIZE - 默认窗口大小
// 
// 超时与重传参数（config.h）：
//   TIMEOUT_MS          - 连接超时时间
//   MAX_RETRIES         - 最大重传次数
//   TIME_WAIT_MS        - TIME_WAIT等待时间
//   HEARTBEAT_INTERVAL_MS - 心跳间隔
//   CONNECTION_TIMEOUT_MS - 连接空闲超时
//   SACK_TIMEOUT_MS     - SACK超时重传时间
// 
// RENO拥塞控制参数（config.h）：
//   INITIAL_CWND        - 初始拥塞窗口
//   INITIAL_SSTHRESH    - 初始慢启动阈值
//   MIN_SSTHRESH        - 最小慢启动阈值
//   DUP_ACK_THRESHOLD   - 重复ACK阈值
// 
// 滑动窗口参数（config.h）：
//   FIXED_WINDOW_SIZE   - 固定窗口大小
//   MSS                 - 最大报文段大小
//   MAX_SACK_BLOCKS     - 最大SACK块数量

// ===== 发送端窗口状态结构体 =====
// 描述：管理发送端滑动窗口，跟踪已发送和已确认的数据包
// 用于流水线发送和选择确认功能
struct SendWindow {
    uint32_t base;                              // 窗口左边界（已确认的最大序列号+1，即第一个未确认的包）
    uint32_t next_seq;                          // 下一个要发送的序列号
    uint8_t is_sent[FIXED_WINDOW_SIZE];         // 标记窗口内包是否已发送（0=未发送，1=已发送）
    uint8_t is_ack[FIXED_WINDOW_SIZE];          // 标记窗口内包是否已确认（0=未确认，1=已确认）
    char data_buf[FIXED_WINDOW_SIZE][MSS];      // 窗口内包的数据缓存（用于重传）
    int data_len[FIXED_WINDOW_SIZE];            // 窗口内包的实际数据长度
    clock_t send_time[FIXED_WINDOW_SIZE];       // 每个包的发送时间（用于超时判断）
    
    // ===== RENO 拥塞控制相关字段 =====
    // 描述：实现 TCP RENO 拥塞控制算法的核心参数
    uint32_t cwnd;                              // 拥塞窗口大小（单位：包数）
    uint32_t ssthresh;                          // 慢启动阈值（单位：包数）
    uint32_t dup_ack_count;                     // 重复 ACK 计数器（用于检测丢包）
    uint32_t last_ack;                          // 上一次收到的 ACK 序列号（用于检测重复 ACK）
    RenoPhase reno_phase;                       // 当前 RENO 阶段
    
    // ===== 统计信息字段 =====
    uint32_t total_packets_sent;                // 发送的总包数（含重传）
    uint32_t total_retransmissions;             // 重传的总包数
    clock_t transmission_start_time;            // 传输开始时间
    int total_bytes_sent;                       // 发送的总字节数（不含协议头）
    
    // ===== 默认构造函数 =====
    // 描述：初始化发送窗口所有字段为默认值，包括 RENO 拥塞控制参数
    SendWindow() : base(0), next_seq(0), cwnd(INITIAL_CWND), ssthresh(INITIAL_SSTHRESH),
                   dup_ack_count(0), last_ack(0), reno_phase(SLOW_START),
                   total_packets_sent(0), total_retransmissions(0), transmission_start_time(0), total_bytes_sent(0) {
        // 初始化所有数组为0
        memset(is_sent, 0, sizeof(is_sent));
        memset(is_ack, 0, sizeof(is_ack));
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(send_time, 0, sizeof(send_time));
    }
    
    // ===== 重置窗口 =====
    // 描述：重置发送窗口到初始状态，包括 RENO 拥塞控制参数
    // 参数：initial_seq - 初始序列号
    void reset(uint32_t initial_seq) {
        base = initial_seq;
        next_seq = initial_seq;
        memset(is_sent, 0, sizeof(is_sent));
        memset(is_ack, 0, sizeof(is_ack));
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(send_time, 0, sizeof(send_time));
        
        // 重置 RENO 拥塞控制参数到初始状态
        cwnd = INITIAL_CWND;
        ssthresh = INITIAL_SSTHRESH;
        dup_ack_count = 0;
        last_ack = initial_seq;
        reno_phase = SLOW_START;
        
        // 重置统计信息
        total_packets_sent = 0;
        total_retransmissions = 0;
        transmission_start_time = clock();
        total_bytes_sent = 0;
    }
    
    // ===== 获取有效发送窗口大小 =====
    // 描述：返回实际可用的发送窗口大小（拥塞窗口 cwnd 与固定窗口的最小值）
    // 说明：遵循 "拥塞控制优先于流量控制" 原则
    // 返回值：有效窗口大小（单位：包数）
    uint32_t getEffectiveWindow() const {
        // 返回拥塞窗口和固定窗口的最小值
        return (cwnd < FIXED_WINDOW_SIZE) ? cwnd : FIXED_WINDOW_SIZE;
    }
    
    // ===== 检查窗口是否可发送新包 =====
    // 描述：判断当前是否可以发送新的数据包（考虑 RENO 拥塞窗口限制）
    // 返回值：true表示可以发送，false表示窗口已满
    bool canSend() const {
        // 使用有效窗口大小（拥塞窗口和固定窗口的最小值）
        uint32_t effectiveWindow = getEffectiveWindow();
        return (next_seq < base + effectiveWindow);
    }
    
    // ===== 获取窗口内的索引 =====
    // 描述：根据序列号获取在窗口数组中的索引位置
    // 使用绝对索引（seq % FIXED_WINDOW_SIZE），避免窗口滑动后索引错乱
    // 参数：seq - 序列号
    // 返回值：窗口内的索引（0 到 FIXED_WINDOW_SIZE-1）
    int getIndex(uint32_t seq) const {
        return seq % FIXED_WINDOW_SIZE;
    }
    
    // ===== 滑动窗口 =====
    // 描述：当收到连续的ACK时，滑动窗口到新的位置
    // 说明：找到连续已确认的最大序列号，然后滑动base到该位置
    void slideWindow() {
        // 从base开始，找到连续已确认的包
        while (is_ack[getIndex(base)] && base < next_seq) {
            // 清除当前位置的状态
            int idx = getIndex(base);
            is_sent[idx] = 0;
            is_ack[idx] = 0;
            data_len[idx] = 0;
            base++;  // 窗口向前滑动
        }
    }
    
    // ===== RENO 拥塞控制：处理新 ACK =====
    // 描述：根据 RENO 算法处理新收到的 ACK，更新拥塞窗口
    // 参数：ack_num - 收到的 ACK 序列号
    // 返回值：true 表示是新 ACK，false 表示是重复 ACK
    bool handleNewACK(uint32_t ack_num) {
        // 检查是否是新 ACK（确认了新的数据）
        if (ack_num > last_ack) {
            // 收到新 ACK，重置重复 ACK 计数器
            dup_ack_count = 0;
            last_ack = ack_num;
            
            // 根据当前阶段更新拥塞窗口
            if (reno_phase == SLOW_START) {
                // ===== 慢启动阶段：每收到 1 个新 ACK，cwnd += 1（指数增长） =====
                cwnd++;
                
                // 检查是否达到慢启动阈值，切换到拥塞避免阶段
                if (cwnd >= ssthresh) {
                    reno_phase = CONGESTION_AVOIDANCE;
                    std::cout << "[RENO] Phase transition: SLOW_START -> CONGESTION_AVOIDANCE (cwnd=" 
                             << cwnd << ", ssthresh=" << ssthresh << ")" << std::endl;
                } else {
                    std::cout << "[RENO] Slow Start: cwnd=" << cwnd 
                             << ", ssthresh=" << ssthresh << std::endl;
                }
            } else if (reno_phase == CONGESTION_AVOIDANCE) {
                // ===== 拥塞避免阶段：每收到 1 个新 ACK，cwnd += 1/cwnd（线性增长） =====
                // 使用加法增长：每收到 cwnd 个 ACK，cwnd 增加 1
                // 实现方式：使用静态计数器，累积到 cwnd 时增加窗口
                static uint32_t ack_count = 0;
                ack_count++;
                if (ack_count >= cwnd) {
                    cwnd++;
                    ack_count = 0;
                    std::cout << "[RENO] Congestion Avoidance: cwnd increased to " << cwnd << std::endl;
                }
            } else if (reno_phase == FAST_RECOVERY) {
                // ===== 快速恢复阶段：收到新 ACK，退出快速恢复，进入拥塞避免 =====
                cwnd = ssthresh;
                reno_phase = CONGESTION_AVOIDANCE;
                std::cout << "[RENO] Fast Recovery completed, transition to CONGESTION_AVOIDANCE (cwnd=" 
                         << cwnd << ", ssthresh=" << ssthresh << ")" << std::endl;
            }
            
            return true;  // 是新 ACK
        } else if (ack_num == last_ack) {
            // ===== 收到重复 ACK =====
            dup_ack_count++;
            std::cout << "[RENO] Duplicate ACK received (count=" << dup_ack_count 
                     << ", ack=" << ack_num << ")" << std::endl;
            
            // 检查是否达到快速重传阈值（3 个重复 ACK）
            if (dup_ack_count == DUP_ACK_THRESHOLD) {
                // ===== 触发快速重传和快速恢复 =====
                handleFastRetransmit();
            } else if (reno_phase == FAST_RECOVERY) {
                // ===== 快速恢复阶段：每收到 1 个重复 ACK，cwnd += 1 =====
                cwnd++;
                std::cout << "[RENO] Fast Recovery: cwnd inflated to " << cwnd << std::endl;
            }
            
            return false;  // 是重复 ACK
        }
        
        return false;
    }
    
    // ===== RENO 拥塞控制：处理快速重传 =====
    // 描述：收到 3 个重复 ACK 时触发，执行快速重传和快速恢复
    void handleFastRetransmit() {
        std::cout << "[RENO] Fast Retransmit triggered (3 duplicate ACKs)" << std::endl;
        
        // 1. 更新慢启动阈值：ssthresh = max(cwnd/2, 2)
        ssthresh = (cwnd / 2 > MIN_SSTHRESH) ? (cwnd / 2) : MIN_SSTHRESH;
        
        // 2. 设置拥塞窗口：cwnd = ssthresh
        cwnd = ssthresh;
        
        // 3. 进入快速恢复阶段
        reno_phase = FAST_RECOVERY;
        
        std::cout << "[RENO] Entering FAST_RECOVERY (cwnd=" << cwnd 
                 << ", ssthresh=" << ssthresh << ")" << std::endl;
        
        // 注意：实际的重传操作由调用者执行（在主发送循环中检测并重传丢失的包）
    }
    
    // ===== RENO 拥塞控制：处理超时 =====
    // 描述：发生超时时调用，重置拥塞窗口并进入慢启动
    void handleTimeout() {
        std::cout << "[RENO] Timeout detected" << std::endl;
        
        // 1. 更新慢启动阈值：ssthresh = max(cwnd/2, 2)
        ssthresh = (cwnd / 2 > MIN_SSTHRESH) ? (cwnd / 2) : MIN_SSTHRESH;
        
        // 2. 重置拥塞窗口：cwnd = 1
        cwnd = INITIAL_CWND;
        
        // 3. 重置重复 ACK 计数器
        dup_ack_count = 0;
        
        // 4. 进入慢启动阶段
        reno_phase = SLOW_START;
        
        std::cout << "[RENO] Timeout recovery: entering SLOW_START (cwnd=" << cwnd 
                 << ", ssthresh=" << ssthresh << ")" << std::endl;
    }
};

// ===== 接收端窗口状态结构体 =====
// 描述：管理接收端滑动窗口，跟踪已接收的数据包
// 用于选择确认功能和数据重组
struct RecvWindow {
    uint32_t base;                              // 窗口左边界（期望接收的下一个序列号）
    char data_buf[FIXED_WINDOW_SIZE][MSS];      // 窗口内已接收的包（按序列号排序存储）
    int data_len[FIXED_WINDOW_SIZE];            // 窗口内已接收包的实际数据长度
    uint8_t is_received[FIXED_WINDOW_SIZE];     // 标记窗口内包是否已接收（0=未接收，1=已接收）
    
    // ===== 统计信息字段 =====
    uint32_t total_packets_received;            // 接收的总包数（含重复）
    uint32_t total_packets_dropped;             // 模拟丢弃的总包数
    clock_t transmission_start_time;            // 传输开始时间
    int total_bytes_received;                   // 接收的总字节数（不含协议头）
    
    // ===== 默认构造函数 =====
    // 描述：初始化接收窗口所有字段为默认值
    RecvWindow() : base(0), total_packets_received(0), total_packets_dropped(0),
                   transmission_start_time(0), total_bytes_received(0) {
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(is_received, 0, sizeof(is_received));
    }
    
    // ===== 重置窗口 =====
    // 描述：重置接收窗口到初始状态
    // 参数：initial_seq - 初始期望序列号
    void reset(uint32_t initial_seq) {
        base = initial_seq;
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(is_received, 0, sizeof(is_received));
        
        // 重置统计信息
        total_packets_received = 0;
        total_packets_dropped = 0;
        transmission_start_time = clock();
        total_bytes_received = 0;
    }
    
    // ===== 检查序列号是否在窗口范围内 =====
    // 描述：判断接收到的包序列号是否在当前接收窗口范围内
    // 参数：seq - 接收到的包的序列号
    // 返回值：true表示在窗口内，false表示不在
    bool inWindow(uint32_t seq) const {
        return (seq >= base && seq < base + FIXED_WINDOW_SIZE);
    }
    
    // ===== 获取窗口内的索引 =====
    // 描述：根据序列号获取在窗口数组中的索引位置
    // 使用绝对索引（seq % FIXED_WINDOW_SIZE），避免窗口滑动后索引错乱
    // 参数：seq - 序列号
    // 返回值：窗口内的索引（0 到 FIXED_WINDOW_SIZE-1）
    int getIndex(uint32_t seq) const {
        return seq % FIXED_WINDOW_SIZE;
    }
    
    // ===== 滑动窗口并取出连续数据 =====
    // 描述：从窗口开始位置取出连续已接收的数据，并滑动窗口
    // 参数：
    //   out_buf - 输出缓冲区，用于存储取出的数据
    //   max_len - 输出缓冲区最大长度
    // 返回值：取出的数据总长度
    int slideAndGetData(char* out_buf, int max_len) {
        int total_len = 0;
        // 从base开始，取出连续已接收的数据
        while (is_received[getIndex(base)]) {
            int idx = getIndex(base);
            // 检查输出缓冲区是否有足够空间
            if (total_len + data_len[idx] <= max_len) {
                memcpy(out_buf + total_len, data_buf[idx], data_len[idx]);
                total_len += data_len[idx];
            }
            // 清除当前位置的状态
            is_received[idx] = 0;
            data_len[idx] = 0;
            base++;  // 窗口向前滑动
        }
        return total_len;
    }
    
    // ===== 生成SACK信息 =====
    // 描述：生成选择确认信息，返回窗口内已接收的非连续序列号列表
    // 参数：
    //   sack_list - 输出数组，存储已接收的序列号
    //   max_count - 最大返回数量
    // 返回值：实际返回的序列号数量
    int generateSACK(uint32_t* sack_list, int max_count) const {
        int count = 0;
        // 遍历窗口，收集所有已接收的包序列号
        for (uint32_t seq = base; seq < base + FIXED_WINDOW_SIZE && count < max_count; seq++) {
            if (is_received[getIndex(seq)]) {
                sack_list[count++] = seq;
            }
        }
        return count;
    }
};

// ===== SACK数据结构 =====
// 描述：用于在ACK包中携带选择确认信息
// 包含已接收的非连续序列号列表
struct SACKInfo {
    uint32_t sack_blocks[MAX_SACK_BLOCKS];  // 已接收的序列号列表
    int count;                               // 有效序列号数量
    
    // ===== 默认构造函数 =====
    SACKInfo() : count(0) {
        memset(sack_blocks, 0, sizeof(sack_blocks));
    }
    
    // ===== 序列化SACK信息到缓冲区 =====
    // 描述：将SACK信息序列化为字节流，用于网络传输
    // 参数：buffer - 目标缓冲区
    // 返回值：序列化后的字节数
    int serialize(char* buffer) const {
        // 格式：[count(1字节)] [seq1(4字节)] [seq2(4字节)] ...
        buffer[0] = (char)count;
        int offset = 1;
        for (int i = 0; i < count; i++) {
            memcpy(buffer + offset, &sack_blocks[i], sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }
        return offset;
    }
    
    // ===== 从缓冲区反序列化SACK信息 =====
    // 描述：从接收到的字节流中解析SACK信息
    // 参数：
    //   buffer - 源缓冲区
    //   bufLen - 缓冲区长度
    // 返回值：true表示解析成功，false表示失败
    bool deserialize(const char* buffer, int bufLen) {
        if (bufLen < 1) return false;
        count = (uint8_t)buffer[0];
        if (count > MAX_SACK_BLOCKS) count = MAX_SACK_BLOCKS;
        if (bufLen < 1 + count * (int)sizeof(uint32_t)) return false;
        int offset = 1;
        for (int i = 0; i < count; i++) {
            memcpy(&sack_blocks[i], buffer + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
        }
        return true;
    }
    
    // ===== 检查序列号是否在SACK列表中 =====
    // 描述：判断某个序列号是否已被选择确认
    // 参数：seq - 要检查的序列号
    // 返回值：true表示在列表中，false表示不在
    bool contains(uint32_t seq) const {
        for (int i = 0; i < count; i++) {
            if (sack_blocks[i] == seq) return true;
        }
        return false;
    }
};

// ===== RFC 1071 校验和计算函数 =====
// 描述：采用 RFC 1071 标准计算校验和，适用于网络数据校验
// 参数：
//   data - 指向要计算校验和的数据的指针
//   len  - 数据长度（字节数）
// 返回值：16位校验和（1的补码）
// 算法说明：
//   1. 将数据按16位（2字节）分组进行累加
//   2. 如果数据长度为1字节，最后一个字节单独处理
//   3. 累加过程中，如果有进位（超过16位），将进位加回低16位
//   4. 最后对结果取反（1的补码）
inline uint16_t calculate_checksum(const void* data, int len) {
    // 累加和，使用32位以便处理进位
    uint32_t sum = 0;
    // 将数据指针转换为字节指针，逐字节处理以避免对齐问题
    const uint8_t* ptr = (const uint8_t*)data;
    
    // 按16位（2字节）为单位进行累加
    while (len > 1) {
        // 将两个字节组合成一个16位值（网络字节序：高字节在前）
        uint16_t word = ((uint16_t)ptr[0] << 8) | ptr[1];
        sum += word;   // 累加当前16位值
        ptr += 2;      // 移动指针
        len -= 2;      // 剩余长度减2
    }
    
    // 如果长度为奇数，处理最后一个字节
    // 将其视为高8位，低8位补0
    if (len == 1) {
        sum += (uint16_t)(*ptr) << 8;
    }
    
    // 将高16位的进位加到低16位
    // 可能产生新的进位，所以用while循环处理
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // 取反得到1的补码，返回16位校验和
    return (uint16_t)(~sum);
}

// ===== UDP协议头结构体（完整版，20字节对齐）=====
// 描述：定义完整的协议头格式，用于可靠数据传输
// 字段布局（总共20字节）：
//   - seq（序列号）：     4字节（32位）- 发送数据的起始序列号
//   - ack（确认号）：     4字节（32位）- 期望接收的下一个序列号
//   - flag（标志位）：    1字节（8位） - SYN/ACK/FIN/SACK
//   - win（窗口大小）：   2字节（16位）- 滑动窗口大小
//   - checksum（校验和）：2字节（16位）- 覆盖协议头+数据的校验和
//   - len（数据长度）：   2字节（16位）- 后续数据段的字节数
//   - reserved（保留）：  5字节       - 保留字段，用于将来扩展
// 注意：使用#pragma pack确保结构体内存紧凑对齐，避免编译器填充字节
#pragma pack(push, 1)
struct UDPHeader {
    uint32_t seq;         // 序列号：发送数据的起始序列号（32位）
    uint32_t ack;         // 确认号：期望接收的下一个序列号（32位）
    uint8_t  flag;        // 标志位：SYN(0x01)/ACK(0x02)/FIN(0x04)/SACK(0x08)（8位）
    uint16_t win;         // 窗口大小：用于流量控制（16位）
    uint16_t checksum;    // 校验和：覆盖协议头+数据，采用RFC 1071标准（16位）
    uint16_t len;         // 数据长度：后续数据段的字节数（16位）
    uint8_t  reserved[5]; // 保留字段：用于将来扩展，凑足20字节对齐
    
    // ===== 默认构造函数 =====
    // 描述：初始化协议头所有字段为默认值
    UDPHeader() : seq(0), ack(0), flag(0), win(DEFAULT_WINDOW_SIZE), 
                  checksum(0), len(0) {
        memset(reserved, 0, sizeof(reserved));  // 保留字段清零
    }
    
    // ===== 带参数的构造函数 =====
    // 描述：使用指定的序列号、确认号和标志位初始化协议头
    // 参数：
    //   s - 序列号
    //   a - 确认号
    //   f - 标志位
    UDPHeader(uint32_t s, uint32_t a, uint8_t f) 
        : seq(s), ack(a), flag(f), win(DEFAULT_WINDOW_SIZE), 
          checksum(0), len(0) {
        memset(reserved, 0, sizeof(reserved));  // 保留字段清零
    }
    
    // ===== 计算并填充校验和 =====
    // 描述：计算协议头+数据的校验和，并填入checksum字段
    // 算法：采用RFC 1071标准
    //   1. 先将checksum字段设为0
    //   2. 将协议头和数据复制到临时缓冲区
    //   3. 对整个缓冲区计算校验和
    //   4. 将计算结果填入checksum字段
    // 参数：
    //   data    - 数据部分的指针
    //   dataLen - 数据长度（字节数）
    void calculateChecksum(const char* data, int dataLen) {
        // 步骤1：将checksum字段设为0（校验和计算不包括自身）
        checksum = 0;
        
        // 步骤2：创建临时缓冲区，存放协议头+数据
        int totalLen = HEADER_SIZE + dataLen;
        char* buffer = new char[totalLen];
        
        // 步骤3：将协议头复制到缓冲区前部
        memcpy(buffer, this, HEADER_SIZE);
        
        // 步骤4：将数据复制到缓冲区后部
        if (dataLen > 0 && data != nullptr) {
            memcpy(buffer + HEADER_SIZE, data, dataLen);
        }
        
        // 步骤5：调用RFC 1071算法计算校验和
        checksum = calculate_checksum(buffer, totalLen);
        
        // 步骤6：释放临时缓冲区
        delete[] buffer;
    }
    
    // ===== 验证校验和 =====
    // 描述：验证接收到的数据包是否完整无损
    // 算法：采用RFC 1071标准验证
    //   对整个"协议头+数据"重新计算校验和
    //   如果结果为0，则校验通过；否则数据包损坏
    // 参数：
    //   data    - 数据部分的指针
    //   dataLen - 数据长度（字节数）
    // 返回值：true表示校验通过，false表示数据包损坏
    bool verifyChecksum(const char* data, int dataLen) const {
        // 步骤1：创建临时缓冲区，存放协议头+数据
        int totalLen = HEADER_SIZE + dataLen;
        char* buffer = new char[totalLen];
        
        // 步骤2：将协议头复制到缓冲区（包含原始checksum）
        memcpy(buffer, this, HEADER_SIZE);
        
        // 步骤3：将数据复制到缓冲区
        if (dataLen > 0 && data != nullptr) {
            memcpy(buffer + HEADER_SIZE, data, dataLen);
        }
        
        // 步骤4：对整个缓冲区计算校验和
        // RFC 1071特性：如果数据完整，重新计算的校验和应为0
        uint16_t result = calculate_checksum(buffer, totalLen);
        
        // 步骤5：释放临时缓冲区
        delete[] buffer;
        
        // 步骤6：校验和为0表示数据完整
        return (result == 0);
    }
};
#pragma pack(pop)

// ===== 数据包结构体 =====
// 描述：封装完整的数据包，包含协议头和数据负载
struct Packet {
    UDPHeader header;                // 协议头（20字节）
    char data[MAX_DATA_SIZE];        // 数据负载缓冲区
    int dataLen;                     // 实际数据长度（内部使用，不发送）
    
    // ===== 默认构造函数 =====
    // 描述：初始化数据包，清空数据缓冲区
    Packet() : dataLen(0) {
        memset(data, 0, MAX_DATA_SIZE);
    }
    
    // ===== 设置数据负载 =====
    // 描述：设置数据包的数据部分，并自动更新协议头的len字段和校验和
    // 参数：
    //   buf - 数据源指针
    //   len - 数据长度
    void setData(const char* buf, int len) {
        // 确保数据长度不超过最大负载大小
        dataLen = (len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : len;
        // 复制数据到缓冲区
        memcpy(data, buf, dataLen);
        // 更新协议头中的数据长度字段
        header.len = (uint16_t)dataLen;
        // 计算并填充校验和
        header.calculateChecksum(data, dataLen);
    }
    
    // ===== 获取数据包总长度 =====
    // 描述：返回协议头+数据的总长度
    // 返回值：数据包总长度（字节）
    int getTotalLen() const {
        return HEADER_SIZE + dataLen;
    }
    
    // ===== 序列化为字节流 =====
    // 描述：将数据包转换为字节流，用于网络发送
    // 参数：
    //   buffer - 目标缓冲区（需预分配足够空间）
    void serialize(char* buffer) const {
        // 先复制协议头
        memcpy(buffer, &header, HEADER_SIZE);
        // 再复制数据部分
        memcpy(buffer + HEADER_SIZE, data, dataLen);
    }
    
    // ===== 从字节流反序列化 =====
    // 描述：从接收到的字节流中解析出数据包，并验证校验和
    // 参数：
    //   buffer - 接收到的字节流
    //   bufLen - 字节流长度
    // 返回值：true表示解析成功且校验和正确，false表示失败或数据损坏
    bool deserialize(const char* buffer, int bufLen) {
        // 检查缓冲区长度是否足够包含协议头
        if (bufLen < HEADER_SIZE) {
            return false;
        }
        // 解析协议头
        memcpy(&header, buffer, HEADER_SIZE);
        // 计算数据部分长度（可以从header.len或bufLen-HEADER_SIZE获取）
        // 这里使用header.len字段，更可靠
        dataLen = header.len;
        // 安全检查：确保数据长度合理
        if (dataLen > MAX_DATA_SIZE || dataLen > bufLen - HEADER_SIZE) {
            return false;
        }
        // 复制数据部分
        if (dataLen > 0) {
            memcpy(data, buffer + HEADER_SIZE, dataLen);
        }
        // 验证校验和，确保数据完整性
        return header.verifyChecksum(data, dataLen);
    }
};

// ===== 辅助函数 =====

// ===== 生成初始序列号 =====
// 描述：生成初始序列号，用于连接建立
// 返回值：32位序列号（固定从0开始）
inline uint32_t generateInitialSeq() {
    return 0;
}

// ===== 获取当前时间戳 =====
// 描述：获取当前时间的毫秒表示
// 返回值：当前时间戳（毫秒）
inline long long getCurrentTimeMs() {
    return (long long)time(NULL) * 1000;
}

// ===== 获取状态名称 =====
// 描述：将连接状态枚举值转换为可读的字符串，用于调试输出
// 参数：
//   state - 连接状态枚举值
// 返回值：状态名称字符串
inline const char* getStateName(ConnectionState state) {
    switch (state) {
        case CLOSED:       return "CLOSED";
        case SYN_SENT:     return "SYN_SENT";
        case SYN_RCVD:     return "SYN_RCVD";
        case ESTABLISHED:  return "ESTABLISHED";
        case FIN_WAIT_1:   return "FIN_WAIT_1";
        case FIN_WAIT_2:   return "FIN_WAIT_2";
        case TIME_WAIT:    return "TIME_WAIT";
        case CLOSE_WAIT:   return "CLOSE_WAIT";
        case LAST_ACK:     return "LAST_ACK";
        default:           return "UNKNOWN";
    }
}

// ===== 获取标志位名称 =====
// 描述：将标志位转换为可读的字符串，用于调试输出
// 参数：
//   flag - 标志位值
// 返回值：标志位名称字符串（静态缓冲区）
inline const char* getFlagName(uint8_t flag) {
    static char flagStr[32];
    flagStr[0] = '\0';
    if (flag & FLAG_SYN)  strcat(flagStr, "SYN ");
    if (flag & FLAG_ACK)  strcat(flagStr, "ACK ");
    if (flag & FLAG_FIN)  strcat(flagStr, "FIN ");
    if (flag & FLAG_SACK) strcat(flagStr, "SACK ");
    if (flagStr[0] == '\0') strcpy(flagStr, "NONE");
    return flagStr;
}

// ===== 获取 RENO 阶段名称 =====
// 描述：将 RENO 阶段枚举值转换为可读的字符串，用于调试输出
// 参数：
//   phase - RENO 阶段枚举值
// 返回值：阶段名称字符串
inline const char* getRenoPhaseName(RenoPhase phase) {
    switch (phase) {
        case SLOW_START:           return "SLOW_START";
        case CONGESTION_AVOIDANCE: return "CONGESTION_AVOIDANCE";
        case FAST_RECOVERY:        return "FAST_RECOVERY";
        default:                   return "UNKNOWN";
    }
}

// ===== 数据包发送工具函数 =====
// 描述：封装数据包发送过程，自动填充协议头、计算校验和、发送完整数据包
// 参数：
//   sockfd    - 套接字文件描述符
//   dest_addr - 目标地址结构体指针
//   addr_len  - 地址结构体长度
//   data      - 要发送的数据指针（可为NULL表示无数据）
//   data_len  - 数据长度（字节）
//   seq       - 序列号
//   ack       - 确认号
//   flag      - 标志位
// 返回值：发送的字节数，失败返回-1
inline int send_packet(int sockfd, const struct sockaddr* dest_addr, int addr_len,
                       const void* data, int data_len, 
                       uint32_t seq, uint32_t ack, uint8_t flag) {
    // 步骤1：创建数据包对象
    Packet packet;
    
    // 步骤2：填充协议头字段
    packet.header.seq = seq;           // 设置序列号
    packet.header.ack = ack;           // 设置确认号
    packet.header.flag = flag;         // 设置标志位
    packet.header.win = DEFAULT_WINDOW_SIZE;  // 设置窗口大小
    
    // 步骤3：填充数据部分
    if (data != NULL && data_len > 0) {
        // 确保数据长度不超过最大负载大小
        packet.dataLen = (data_len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : data_len;
        memcpy(packet.data, data, packet.dataLen);
    } else {
        packet.dataLen = 0;
    }
    
    // 步骤4：更新协议头中的数据长度字段
    packet.header.len = (uint16_t)packet.dataLen;
    
    // 步骤5：计算校验和（覆盖协议头+数据）
    packet.header.calculateChecksum(packet.data, packet.dataLen);
    
    // 步骤6：序列化数据包到发送缓冲区
    char sendBuffer[MAX_PACKET_SIZE];
    packet.serialize(sendBuffer);
    
    // 步骤7：发送数据包
    int bytesSent = sendto(sockfd, sendBuffer, packet.getTotalLen(), 0, 
                           dest_addr, addr_len);
    
    return bytesSent;
}

// ===== 数据包接收工具函数 =====
// 描述：接收数据包，验证校验和，解析协议头和数据
// 参数：
//   sockfd     - 套接字文件描述符
//   src_addr   - 用于存储发送方地址的结构体指针
//   addr_len   - 地址结构体长度指针（输入/输出参数）
//   buf        - 用于存储接收数据的缓冲区指针
//   buf_len    - 缓冲区大小
//   header_out - 用于输出解析后的协议头
// 返回值：
//   成功时返回数据部分的长度（可为0），
//   校验和失败返回-2，
//   其他错误返回-1
inline int recv_packet(int sockfd, struct sockaddr* src_addr, int* addr_len,
                       void* buf, int buf_len, UDPHeader* header_out) {
    // 步骤1：创建接收缓冲区
    char recvBuffer[MAX_PACKET_SIZE];
    
    // 步骤2：接收数据包
    int bytesReceived = recvfrom(sockfd, recvBuffer, MAX_PACKET_SIZE, 0, 
                                 src_addr, addr_len);
    
    // 步骤3：检查接收是否成功
    if (bytesReceived <= 0) {
        return -1;  // 接收失败
    }
    
    // 步骤4：检查是否至少包含完整协议头
    if (bytesReceived < HEADER_SIZE) {
        return -1;  // 数据包不完整
    }
    
    // 步骤5：创建数据包对象并反序列化
    Packet packet;
    if (!packet.deserialize(recvBuffer, bytesReceived)) {
        // 校验和验证失败，数据包损坏
        return -2;
    }
    
    // 步骤6：输出解析后的协议头
    if (header_out != NULL) {
        memcpy(header_out, &packet.header, sizeof(UDPHeader));
    }
    
    // 步骤7：将数据复制到用户缓冲区
    int copyLen = 0;
    if (packet.dataLen > 0 && buf != NULL && buf_len > 0) {
        copyLen = (packet.dataLen > buf_len) ? buf_len : packet.dataLen;
        memcpy(buf, packet.data, copyLen);
    }
    
    // 步骤8：返回数据部分的长度
    return copyLen;
}

#endif // PROTOCOL_H
