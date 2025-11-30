#ifndef CONGESTION_H
#define CONGESTION_H

#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cstdbool>
#include "reliable_transport.h"
#include "packet.h"

// ==================== 拥塞控制状态枚举 ====================

/**
 * TCP RENO拥塞控制算法的三个阶段
 * 
 * 状态转换图：
 * 
 *   [启动]
 *     ↓
 * [SLOW_START] ─(cwnd >= ssthresh)→ [CONGESTION_AVOIDANCE]
 *     ↓(3个重复ACK或超时)              ↓(3个重复ACK)
 *     └──────────────────→ [FAST_RECOVERY] ─(新ACK)→ [CONGESTION_AVOIDANCE]
 *                              ↓(超时)
 *                         回到 [SLOW_START]
 */
typedef enum {
    SLOW_START = 0,                    // 慢启动：指数增长
    CONGESTION_AVOIDANCE = 1,          // 拥塞避免：线性增长
    FAST_RECOVERY = 2                  // 快速恢复：处理重复ACK
} CongestionState;

// ==================== 拥塞控制结构体 ====================

/**
 * TCP RENO拥塞控制参数
 * 
 * 核心参数说明：
 * - ssthresh: 慢启动阈值，用于区分两个增长阶段
 * - cwnd: 拥塞窗口大小（字节），决定了可以发送的数据量
 * - state: 当前所处的拥塞控制阶段
 * - dup_ack_count: 重复ACK计数（用于检测丢包）
 */
typedef struct {
    uint32_t ssthresh;                 // 慢启动阈值（字节）
    uint32_t cwnd;                     // 拥塞窗口大小（字节）
    uint32_t cwnd_inc;                 // 拥塞窗口增量（用于精确计算）
    CongestionState state;             // 当前拥塞控制状态
    int dup_ack_count;                 // 重复ACK计数器（0-3+）
    uint32_t recovery_point;           // 快速恢复起始点序列号
    
    // RTT相关（用于RTO计算）
    uint32_t rtt;                      // 往返时间估计（毫秒）
    uint32_t rttvar;                   // RTT方差（毫秒）
    uint32_t rto;                      // 重传超时时间（毫秒）
    
    // 统计信息
    uint32_t congestion_events;        // 拥塞事件计数
    uint32_t fast_retransmits;         // 快速重传次数
} CongestionControl;

// ==================== 拥塞控制管理函数 ====================

/**
 * 创建拥塞控制对象
 * 
 * 初始化RENO拥塞控制算法的相关参数
 * 初始状态：SLOW_START，cwnd=1 MSS
 * 
 * @return 返回分配的拥塞控制指针，失败返回NULL
 */
CongestionControl* create_congestion_control();

/**
 * 更新拥塞控制状态
 * 
 * 处理新ACK事件，根据当前状态更新cwnd
 * 
 * @param cc 拥塞控制指针
 * @param ack_num 确认号
 * @param is_duplicate_ack 是否为重复ACK
 * @return 成功返回true，失败返回false
 */
bool update_congestion_control(CongestionControl* cc, uint32_t ack_num, bool is_duplicate_ack);

/**
 * 处理超时事件
 * 
 * 当发生重传超时时调用，回退到慢启动
 * 
 * @param cc 拥塞控制指针
 * @return 成功返回true，失败返回false
 */
bool handle_congestion_timeout(CongestionControl* cc);

/**
 * 获取当前拥塞窗口大小
 * 
 * @param cc 拥塞控制指针
 * @return 拥塞窗口大小（字节）
 */
uint32_t get_congestion_window(CongestionControl* cc);

/**
 * 获取当前可发送字节数
 * 
 * 取决于拥塞窗口和接收方窗口的最小值
 * （这里只考虑拥塞窗口）
 * 
 * @param cc 拥塞控制指针
 * @return 可发送字节数
 */
uint32_t get_send_allowance(CongestionControl* cc);

/**
 * 获取拥塞控制状态字符串
 * 
 * @param state 拥塞控制状态
 * @return 状态字符串
 */
const char* congestion_state_to_string(CongestionState state);

/**
 * 更新RTT估计值
 * 
 * 使用Karn/Partridge算法估计RTT和RTO
 * 
 * @param cc 拥塞控制指针
 * @param sample_rtt 采样的RTT值（毫秒）
 * @return 成功返回true，失败返回false
 */
bool update_rtt(CongestionControl* cc, uint32_t sample_rtt);

/**
 * 获取当前的重传超时时间
 * 
 * @param cc 拥塞控制指针
 * @return RTO值（毫秒）
 */
uint32_t get_rto(CongestionControl* cc);

/**
 * 打印拥塞控制状态（调试用）
 * 
 * @param cc 拥塞控制指针
 */
void print_congestion_control(CongestionControl* cc);

/**
 * 释放拥塞控制资源
 * 
 * @param cc 拥塞控制指针
 */
void free_congestion_control(CongestionControl* cc);

// ==================== 内部RENO算法函数 ====================

/**
 * 慢启动阶段
 * 
 * 拥塞窗口指数增长：cwnd *= 2
 * 每收到一个ACK，cwnd增加一个MSS
 * 
 * @param cc 拥塞控制指针
 */
void slow_start(CongestionControl* cc);

/**
 * 拥塞避免阶段
 * 
 * 拥塞窗口线性增长：cwnd += 1
 * 每RTT时间内增加一个MSS
 * 
 * @param cc 拥塞控制指针
 */
void congestion_avoidance(CongestionControl* cc);

/**
 * 快速重传
 * 
 * 收到3个重复ACK时触发
 * 立即重传丢失的数据包，无需等待超时
 * 
 * @param cc 拥塞控制指针
 */
void fast_retransmit(CongestionControl* cc);

/**
 * 快速恢复
 * 
 * 快速重传后进入快速恢复：
 * - ssthresh = cwnd / 2
 * - cwnd = ssthresh + 3
 * - 继续发送新数据，每个重复ACK增加cwnd
 * - 收到新ACK时退出快速恢复
 * 
 * @param cc 拥塞控制指针
 */
void fast_recovery(CongestionControl* cc);

#endif // CONGESTION_H
