#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <ctime>
#include <cstring>
#include "config.h"  // 引入配置文件，所有可配置参数集中在config.h中管理

// ===== 连接状态枚举 =====
// 描述：用于跟踪连接的当前状态
// 连接管理：TCP状态机状态定义
enum ConnectionState {
    CLOSED,        // 关闭状态：初始状态，无连接
    SYN_SENT,      // 客户端：已发送SYN，等待SYN+ACK（三次握手第一步后）
    SYN_RCVD,      // 服务端：已接收SYN，已发送SYN+ACK，等待ACK（三次握手第二步后）
    ESTABLISHED,   // 连接已建立：可以进行数据传输（三次握手完成后）
    FIN_WAIT_1,    // 主动关闭方：已发送FIN，等待ACK（四次挥手第一步后）
    FIN_WAIT_2,    // 主动关闭方：已收到FIN的ACK，等待对方FIN（四次挥手第二步后）
    TIME_WAIT,     // 主动关闭方：已发送最后的ACK，等待2MSL（四次挥手第四步后）
    CLOSE_WAIT,    // 被动关闭方：已收到FIN，已发送ACK，等待本地关闭（四次挥手第二步后）
    LAST_ACK       // 被动关闭方：已发送FIN，等待最后的ACK（四次挥手第三步后）
};

// ===== RENO 拥塞控制阶段枚举 =====
// 描述：用于跟踪 RENO 拥塞控制算法的当前阶段
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

// ===== 发送端窗口状态结构体 =====
// 描述：管理发送端滑动窗口，跟踪已发送和已确认的数据包
// 用于流水线发送和选择确认功能
struct SendWindow {
    // 滑动窗口：窗口基础状态
    uint32_t base;                              // 窗口左边界（已确认的最大序列号+1，即第一个未确认的包）
    uint32_t next_seq;                          // 下一个要发送的序列号
    uint8_t is_sent[FIXED_WINDOW_SIZE];         // 标记窗口内包是否已发送（0=未发送，1=已发送）
    uint8_t is_ack[FIXED_WINDOW_SIZE];          // 标记窗口内包是否已确认（0=未确认，1=已确认，用于SACK）
    char data_buf[FIXED_WINDOW_SIZE][MSS];      // 窗口内包的数据缓存（用于重传机制）
    int data_len[FIXED_WINDOW_SIZE];            // 窗口内包的实际数据长度
    clock_t send_time[FIXED_WINDOW_SIZE];       // 计时器：每个包的发送时间（用于计算RTO和超时判断）
    
    // ===== RENO 拥塞控制相关字段 =====
    // 描述：实现 TCP RENO 拥塞控制算法的核心参数
    uint32_t cwnd;                              // 拥塞窗口大小（单位：包数），控制发送速率
    uint32_t ssthresh;                          // 慢启动阈值（单位：包数），慢启动和拥塞避免的分界线
    uint32_t dup_ack_count;                     // 重复 ACK 计数器（用于检测丢包和触发快重传）
    uint32_t last_ack;                          // 上一次收到的 ACK 序列号（用于检测重复 ACK）
    RenoPhase reno_phase;                       // 当前 RENO 阶段（慢启动/拥塞避免/快速恢复）
    
    // ===== 统计信息字段 =====
    uint32_t total_packets_sent;                // 发送的总包数（含重传）
    uint32_t total_retransmissions;             // 重传的总包数
    clock_t transmission_start_time;            // 传输开始时间
    int total_bytes_sent;                       // 发送的总字节数（不含协议头）
    
    // 默认构造函数
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
    
    // 重置窗口到初始状态
    void reset(uint32_t initial_seq) {
        base = initial_seq;
        next_seq = initial_seq;
        memset(is_sent, 0, sizeof(is_sent));
        memset(is_ack, 0, sizeof(is_ack));
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(send_time, 0, sizeof(send_time));
        
        // 重置 RENO 拥塞控制参数到初始状态
        // 4、慢启动：初始cwnd设为1（或配置的初始值）
        cwnd = INITIAL_CWND;
        ssthresh = INITIAL_SSTHRESH;
        dup_ack_count = 0;
        last_ack = initial_seq;
        reno_phase = SLOW_START; // 初始进入慢启动阶段
        
        // 重置统计信息
        total_packets_sent = 0;
        total_retransmissions = 0;
        transmission_start_time = clock();
        total_bytes_sent = 0;
    }
    
    // 获取有效发送窗口大小（cwnd与固定窗口的最小值）
    uint32_t getEffectiveWindow() const {
        // 返回拥塞窗口和固定窗口的最小值
        return (cwnd < FIXED_WINDOW_SIZE) ? cwnd : FIXED_WINDOW_SIZE;
    }
    
    // 检查窗口是否可发送新包
    bool canSend() const {
        // 使用有效窗口大小（拥塞窗口和固定窗口的最小值）
        uint32_t effectiveWindow = getEffectiveWindow();
        // 如果下一个序列号在窗口范围内，则允许发送
        return (next_seq < base + effectiveWindow);
    }
    
    // 获取窗口内的索引
    int getIndex(uint32_t seq) const {
        return seq % FIXED_WINDOW_SIZE;
    }
    
    // 滑动窗口：收到连续 ACK 时滑动到新位置
    void slideWindow() {
        // 从base开始，找到连续已确认的包
        while (is_ack[getIndex(base)] && base < next_seq) {
            // 清除当前位置的状态，准备复用
            int idx = getIndex(base);
            is_sent[idx] = 0;
            is_ack[idx] = 0;
            data_len[idx] = 0;
            base++;  // 窗口左边界向前滑动
        }
    }
    
    // RENO 拥塞控制：处理新 ACK，返回 true 表示是新 ACK。核心状态机更新
    bool handleNewACK(uint32_t ack_num) {
        // 检查是否是新 ACK（确认了新的数据）
        if (ack_num > last_ack) {
            // 收到新 ACK，重置重复 ACK 计数器
            dup_ack_count = 0;
            last_ack = ack_num;
            
            // 根据当前阶段更新拥塞窗口
            if (reno_phase == SLOW_START) {
                // ===== 慢启动阶段：每收到 1 个新 ACK，cwnd += 1 ====
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
                reno_phase = CONGESTION_AVOIDANCE;// 进入拥塞避免阶段
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
            }else{
                cwnd++; //接收到三个重复ack之前，cwnd也增加
            }
            
            return false;  // 是重复 ACK
        }
        
        return false;
    }
    
    // RENO 拥塞控制：处理快速重传（3个重复ACK触发）
    void handleFastRetransmit() {
        std::cout << "[RENO] Fast Retransmit triggered (3 duplicate ACKs)" << std::endl;
        
        // 1. 更新慢启动阈值：ssthresh = max(ssthresh/2, 2)
        ssthresh = (ssthresh / 2 > MIN_SSTHRESH) ? (ssthresh / 2) : MIN_SSTHRESH;
        
        // 2. 设置拥塞窗口：cwnd = ssthresh
        cwnd = ssthresh;
        
        // 3. 进入快速恢复阶段
        reno_phase = FAST_RECOVERY;
        
        std::cout << "[RENO] Entering FAST_RECOVERY (cwnd=" << cwnd 
                 << ", ssthresh=" << ssthresh << ")" << std::endl;
        
        // 在主发送循环中检测并重传丢失的包
    }
    
    // RENO 拥塞控制：处理超时，重置拥塞窗口并进入慢启动
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




// 接收端窗口状态结构体：管理滑动窗口，跟踪已接收的数据包
struct RecvWindow {
    //窗口基础状态
    uint32_t base;                              // 窗口左边界（期望接收的下一个序列号）
    char data_buf[FIXED_WINDOW_SIZE][MSS];      // 窗口内已接收的包（按序列号排序存储，用于乱序重组）
    int data_len[FIXED_WINDOW_SIZE];            // 窗口内已接收包的实际数据长度
    uint8_t is_received[FIXED_WINDOW_SIZE];     // 标记窗口内包是否已接收（0=未接收，1=已接收，用于去重）
    
    // 统计信息字段
    uint32_t total_packets_received;            // 接收的总包数（含重复）
    uint32_t total_packets_dropped;             // 模拟丢弃的总包数
    uint32_t total_duplicate_packets;           // 接收到的重复包/旧包数量
    clock_t transmission_start_time;            // 传输开始时间
    int total_bytes_received;                   // 接收的总字节数（不含协议头）
    
    // 默认构造函数
    RecvWindow() : base(0), total_packets_received(0), total_packets_dropped(0), total_duplicate_packets(0),
                   transmission_start_time(0), total_bytes_received(0) {
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(is_received, 0, sizeof(is_received));
    }
    
    // 重置窗口到初始状态
    void reset(uint32_t initial_seq) {
        base = initial_seq;
        memset(data_buf, 0, sizeof(data_buf));
        memset(data_len, 0, sizeof(data_len));
        memset(is_received, 0, sizeof(is_received));
        
        // 重置统计信息
        total_packets_received = 0;
        total_duplicate_packets = 0;
        total_packets_dropped = 0;
        transmission_start_time = clock();
        total_bytes_received = 0;
    }
    
    // 检查序列号是否在窗口范围内
    bool inWindow(uint32_t seq) const {
        return (seq >= base && seq < base + FIXED_WINDOW_SIZE);
    }
    
    // 根据序列号获取窗口内索引
    int getIndex(uint32_t seq) const {
        return seq % FIXED_WINDOW_SIZE;
    }
    
    // 滑动窗口并取出连续数据
    int slideAndGetData(char* out_buf, int max_len) {
        int total_len = 0;
        // 从base开始，取出连续已接收的数据
        while (is_received[getIndex(base)]) {
            int idx = getIndex(base);
            // 检查输出缓冲区是否有足够空间
            if (total_len + data_len[idx] <= max_len) {
                memcpy(out_buf + total_len, data_buf[idx], data_len[idx]);//参数含义：目标地址，源地址，拷贝长度
                total_len += data_len[idx];
            }
            // 清除当前位置的状态，准备复用
            is_received[idx] = 0;
            data_len[idx] = 0;
            base++;  // 窗口左边界向前滑动
        }
        return total_len;
    }
    
    // 生成SACK信息：返回窗口内已接收的序列号列表
    int generateSACK(uint32_t* sack_list, int max_count) const {
        int count = 0;
        // 遍历窗口，收集所有已接收的包序列号（乱序到达的包）
        for (uint32_t seq = base; seq < base + FIXED_WINDOW_SIZE && count < max_count; seq++) {
            if (is_received[getIndex(seq)]) {
                sack_list[count++] = seq;
            }
        }
        return count;
    }
};




// SACK数据结构：在ACK包中携带选择确认信息
struct SACKInfo {
    uint32_t sack_blocks[MAX_SACK_BLOCKS];  // 已接收的序列号列表（乱序到达的包）
    int count;                               // 有效序列号数量
    
    SACKInfo() : count(0) {
        memset(sack_blocks, 0, sizeof(sack_blocks));
    }
    
    // 序列化SACK信息到缓冲区
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
    
    // 从缓冲区反序列化SACK信息
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
    
    // 检查序列号是否在SACK列表中
    bool contains(uint32_t seq) const {
        for (int i = 0; i < count; i++) {
            if (sack_blocks[i] == seq) return true;
        }
        return false;
    }
};




// RFC 1071 校验和算法实现
// 步骤1：计算16位累加和
// 参数：data - 数据指针，len - 数据长度（字节）
// 返回：32位累加和
inline uint32_t checksum_accumulate(const void* data, int len) {
    uint32_t sum = 0;
    const uint8_t* ptr = (const uint8_t*)data;// 定义字节指针，逐字节处理数据
    
    // 每次处理2字节（16位），累加到sum
    while (len > 1) {
        // 大端序组合：ptr[0]为高字节，ptr[1]为低字节
        sum += ((uint16_t)ptr[0] << 8) | ptr[1];
        ptr += 2;
        len -= 2;
    }
    
    // 如果剩余1字节（奇数长度），作为高8位处理，低8位补0
    if (len == 1) {
        sum += (uint16_t)(*ptr) << 8;
    }
    
    return sum;
}

// 步骤2：折叠32位累加和为16位，并取反码
// 参数：sum - 32位累加和
// 返回：16位校验和
inline uint16_t checksum_finalize(uint32_t sum) {
    // 反复将高16位加到低16位，直到高16位为0
    while (sum >> 16) {// sum>>16取高16位不为0时继续循环
        sum = (sum & 0xFFFF) + (sum >> 16);//sun&0xFFFF取低16位，sum>>16取高16位
    }
    // 取反码得到最终校验和
    return (uint16_t)(~sum);
}

// 完整校验和计算（头部 + 负载），调用求和和折叠取反函数
// 参数：header - 头部指针，headerLen - 头部长度
//       payload - 负载指针，payloadLen - 负载长度
// 返回：16位校验和
inline uint16_t checksum_compute_two_parts(
    const void* header, int headerLen,
    const void* payload, int payloadLen) 
{
    // 累加头部
    uint32_t sum = checksum_accumulate(header, headerLen);
    // 累加负载（如果有）
    if (payload != nullptr && payloadLen > 0) {
        sum += checksum_accumulate(payload, payloadLen);
    }
    // 折叠并取反
    return checksum_finalize(sum);
}

// 验证校验和
// 原理：对含校验和的完整数据重新计算，结果应为0
// 返回：true = 数据完整，false = 数据损坏
inline bool checksum_verify_two_parts(
    const void* header, int headerLen,
    const void* payload, int payloadLen)
{
    uint32_t sum = checksum_accumulate(header, headerLen);
    if (payload != nullptr && payloadLen > 0) {
        sum += checksum_accumulate(payload, payloadLen);
    }
    // 如果数据完整，折叠取反后结果为0
    return (checksum_finalize(sum) == 0);
}

// UDP协议头结构体（20字节）
#pragma pack(push, 1)
struct UDPHeader {
    uint32_t seq;         // 序列号：标识数据包的顺序，用于重组和去重
    uint32_t ack;         // 确认号：期望收到的下一个字节的序列号
    uint8_t  flag;        // 标志位：SYN/ACK/FIN/SACK，用于控制连接状态和传输行为
    uint16_t win;         // 窗口大小：接收端通告的窗口大小，用于流量控制
    uint16_t checksum;    // 校验和：用于检测数据在传输过程中是否发生错误
    uint16_t len;         // 数据长度：UDP数据载荷的长度（不含头部）
    uint8_t  reserved[5]; // 保留字段：用于字节对齐或未来扩展，初始化为0
    
    UDPHeader() : seq(0), ack(0), flag(0), win(DEFAULT_WINDOW_SIZE), 
                  checksum(0), len(0) {
        memset(reserved, 0, sizeof(reserved));  // 保留字段清零，确保数据纯净
    }
    
    UDPHeader(uint32_t s, uint32_t a, uint8_t f) 
        : seq(s), ack(a), flag(f), win(DEFAULT_WINDOW_SIZE), 
          checksum(0), len(0) {
        memset(reserved, 0, sizeof(reserved));  // 保留字段清零
    }
    
    // 计算并填充校验和
    // 使用方法：发送前调用，会自动将 checksum 字段置0后计算
    void calculateChecksum(const char* data, int dataLen) {
        checksum = 0;  // 计算前必须将校验和字段置零
        checksum = checksum_compute_two_parts(this, HEADER_SIZE, data, dataLen);
    }
    
    // 验证校验和
    // 使用方法：接收后调用，返回 true 表示数据完整
    bool verifyChecksum(const char* data, int dataLen) const {
        return checksum_verify_two_parts(this, HEADER_SIZE, data, dataLen);
    }
};
#pragma pack(pop)// 恢复默认对齐方式

// 数据包结构体：封装UDP协议头和数据负载
struct Packet {
    UDPHeader header;
    char data[MAX_DATA_SIZE];
    int dataLen;
    
    Packet() : dataLen(0) {
        memset(data, 0, MAX_DATA_SIZE);
    }
    
    // 设置数据负载，自动更新len字段和校验和
    void setData(const char* buf, int len) {
        dataLen = (len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : len;
        memcpy(data, buf, dataLen);
        header.len = (uint16_t)dataLen;
        header.calculateChecksum(data, dataLen);
    }
    
    int getTotalLen() const {
        return HEADER_SIZE + dataLen;
    }
    
    // 序列化为字节流
    void serialize(char* buffer) const {
        memcpy(buffer, &header, HEADER_SIZE);
        memcpy(buffer + HEADER_SIZE, data, dataLen);
    }
    
    // 从字节流反序列化，返回true表示校验通过
    bool deserialize(const char* buffer, int bufLen) {
        if (bufLen < HEADER_SIZE) return false;
        memcpy(&header, buffer, HEADER_SIZE);
        dataLen = header.len;
        if (dataLen > MAX_DATA_SIZE || dataLen > bufLen - HEADER_SIZE) return false;
        if (dataLen > 0) memcpy(data, buffer + HEADER_SIZE, dataLen);
        return header.verifyChecksum(data, dataLen);
    }
};



// 生成初始序列号
inline uint32_t generateInitialSeq() {
    return 0;
}

// 获取当前时间戳（毫秒）
inline long long getCurrentTimeMs() {
    return (long long)time(NULL) * 1000;
}



//==================================================状态/标志位名称获取函数==================================================
// 连接管理：获取状态名称
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

// 握手/挥手：获取标志位名称
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

// 获取 RENO 阶段名称
inline const char* getRenoPhaseName(RenoPhase phase) {
    switch (phase) {
        case SLOW_START:           return "SLOW_START";
        case CONGESTION_AVOIDANCE: return "CONGESTION_AVOIDANCE";
        case FAST_RECOVERY:        return "FAST_RECOVERY";
        default:                   return "UNKNOWN";
    }
}
//==================================================状态/标志位名称获取函数==================================================



// 数据包发送工具函数：封装发送过程
inline int send_packet(int sockfd, const struct sockaddr* dest_addr, int addr_len,
                       const void* data, int data_len, 
                       uint32_t seq, uint32_t ack, uint8_t flag) {
    Packet packet;
    packet.header.seq = seq;
    packet.header.ack = ack;
    packet.header.flag = flag;
    packet.header.win = DEFAULT_WINDOW_SIZE;
    
    if (data != NULL && data_len > 0) {
        packet.dataLen = (data_len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : data_len;
        memcpy(packet.data, data, packet.dataLen);
    } else {
        packet.dataLen = 0;
    }
    
    packet.header.len = (uint16_t)packet.dataLen;
    packet.header.calculateChecksum(packet.data, packet.dataLen);
    
    char sendBuffer[MAX_PACKET_SIZE];
    packet.serialize(sendBuffer);
    return sendto(sockfd, sendBuffer, packet.getTotalLen(), 0, dest_addr, addr_len);
}

// 数据包接收工具函数：接收并验证数据包
// 返回值：成功返回数据长度，校验和失败返回-2，其他错误返回-1
inline int recv_packet(int sockfd, struct sockaddr* src_addr, int* addr_len,
                       void* buf, int buf_len, UDPHeader* header_out) {
    char recvBuffer[MAX_PACKET_SIZE];
    int bytesReceived = recvfrom(sockfd, recvBuffer, MAX_PACKET_SIZE, 0, src_addr, addr_len);
    
    if (bytesReceived <= 0) return -1;
    if (bytesReceived < HEADER_SIZE) return -1;
    
    Packet packet;
    if (!packet.deserialize(recvBuffer, bytesReceived)) return -2;
    
    if (header_out != NULL) {
        memcpy(header_out, &packet.header, sizeof(UDPHeader));
    }
    
    int copyLen = 0;
    if (packet.dataLen > 0 && buf != NULL && buf_len > 0) {
        copyLen = (packet.dataLen > buf_len) ? buf_len : packet.dataLen;
        memcpy(buf, packet.data, copyLen);
    }
    return copyLen;
}

#endif // PROTOCOL_H
