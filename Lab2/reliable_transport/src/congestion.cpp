#include "congestion.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

// ==================== 日志宏定义 ====================

#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)

// ==================== 常量定义 ====================

#define MSS (MAX_DATA_LENGTH)           // 最大报文段长度
#define INITIAL_CWND (1 * MSS)          // 初始拥塞窗口 = 1 MSS
#define INITIAL_SSTHRESH (65536)        // 初始慢启动阈值（很大）
#define DUP_ACK_THRESHOLD 3             // 快速重传的重复ACK阈值

// ==================== 状态字符串转换 ====================

/**
 * 获取拥塞控制状态的字符串表示
 */
const char* congestion_state_to_string(CongestionState state)
{
    switch (state) {
        case SLOW_START:
            return "SLOW_START";
        case CONGESTION_AVOIDANCE:
            return "CONGESTION_AVOIDANCE";
        case FAST_RECOVERY:
            return "FAST_RECOVERY";
        default:
            return "UNKNOWN";
    }
}

// ==================== 拥塞控制对象创建 ====================

/**
 * 创建拥塞控制对象
 * 
 * 初始化参数说明：
 * - cwnd = 1 MSS: 保守的初始值
 * - ssthresh = 65536 bytes: 很大的初始阈值
 * - state = SLOW_START: 从慢启动开始
 * - dup_ack_count = 0: 无重复ACK
 * - rto = TIMEOUT_MS: 初始重传超时
 */
CongestionControl* create_congestion_control()
{
    CongestionControl* cc = (CongestionControl*)malloc(sizeof(CongestionControl));
    if (cc == NULL) {
        LOG_WARN("Failed to allocate CongestionControl");
        return NULL;
    }

    // 初始化拥塞窗口参数
    cc->cwnd = INITIAL_CWND;           // 1 MSS
    cc->ssthresh = INITIAL_SSTHRESH;   // 64 KB
    cc->cwnd_inc = 0;                  // 用于精确计算增量

    // 初始化状态
    cc->state = SLOW_START;            // 从慢启动开始
    cc->dup_ack_count = 0;             // 无重复ACK
    cc->recovery_point = 0;            // 无恢复点

    // 初始化RTT和RTO
    cc->rtt = TIMEOUT_MS;              // 初始RTT = 1000ms
    cc->rttvar = TIMEOUT_MS / 2;       // 初始RTT方差 = 500ms
    cc->rto = TIMEOUT_MS;              // 初始RTO = 1000ms

    // 初始化统计信息
    cc->congestion_events = 0;
    cc->fast_retransmits = 0;

    LOG_INFO("Congestion control created: cwnd=%u, ssthresh=%u, rto=%u",
             cc->cwnd, cc->ssthresh, cc->rto);

    return cc;
}

// ==================== RENO算法：慢启动阶段 ====================

/**
 * 慢启动阶段（Slow Start）
 * 
 * 算法说明：
 * - 初始cwnd = 1 MSS
 * - 每收到一个ACK，cwnd 增加 1 MSS
 * - 效果：cwnd 指数增长 (1, 2, 4, 8, ...)
 * - 当 cwnd >= ssthresh 时，转入拥塞避免
 * 
 * 数学模型：
 * cwnd(n) = cwnd(n-1) + 1 MSS（每个RTT）
 * 增长速度：cwnd 每个RTT翻倍
 * 
 * 目的：
 * 快速探测网络容量，找到合适的发送速率
 * 可以快速响应到新发现的可用带宽
 */
void slow_start(CongestionControl* cc)
{
    if (cc == NULL || cc->state != SLOW_START) {
        return;
    }

    // 每收到一个ACK，增加一个MSS
    cc->cwnd += MSS;

    LOG_DEBUG("Slow Start: cwnd=%u (ssthresh=%u)", cc->cwnd, cc->ssthresh);

    // 检查是否达到慢启动阈值
    if (cc->cwnd >= cc->ssthresh) {
        cc->state = CONGESTION_AVOIDANCE;
        LOG_INFO("Transitioning from SLOW_START to CONGESTION_AVOIDANCE at cwnd=%u", cc->cwnd);
    }
}

// ==================== RENO算法：拥塞避免阶段 ====================

/**
 * 拥塞避免阶段（Congestion Avoidance）
 * 
 * 算法说明：
 * - 当 cwnd >= ssthresh 时进入此阶段
 * - 每收到一个ACK，cwnd 增加 MSS/cwnd
 * - 效果：cwnd 线性增长
 * - 接近网络饱和点时，增长速度减缓
 * 
 * 精确计算：
 * 累加增量cwnd_inc，当累计值 >= cwnd时，增加1 MSS
 * cwnd_inc += MSS/cwnd
 * if (cwnd_inc >= cwnd) {
 *     cwnd += MSS
 *     cwnd_inc = 0
 * }
 * 
 * 简化计算：
 * 每RTT时间内，cwnd增加1 MSS
 * 由于每RTT接收到的ACK数 ≈ cwnd/MSS，
 * 所以每个ACK增加：MSS/cwnd
 * 
 * 目的：
 * 谨慎地增加发送速率，接近网络容量
 * 准备快速响应任何拥塞信号
 */
void congestion_avoidance(CongestionControl* cc)
{
    if (cc == NULL || cc->state != CONGESTION_AVOIDANCE) {
        return;
    }

    // 精确计算增量
    // 每个ACK增加 1 MSS/cwnd，累计达到cwnd时增加1 MSS
    cc->cwnd_inc += MSS;

    if (cc->cwnd_inc >= cc->cwnd) {
        cc->cwnd += MSS;
        cc->cwnd_inc = 0;
    }

    LOG_DEBUG("Congestion Avoidance: cwnd=%u (inc=%u/%u)", cc->cwnd, cc->cwnd_inc, cc->cwnd);
}

// ==================== RENO算法：快速重传 ====================

/**
 * 快速重传（Fast Retransmit）
 * 
 * 触发条件：
 * - 收到第3个重复ACK（相同的ACK号）
 * - 无需等待RTO超时
 * 
 * 算法说明：
 * 1. 检测到丢包标志：3个重复ACK
 * 2. 记录恢复点：当前的最高序列号
 * 3. 立即重传可能丢失的数据包
 * 4. 进入快速恢复阶段
 * 
 * 优势：
 * - 比RTO超时快得多（通常快1个RTT以上）
 * - 允许继续发送新数据
 * - 比Tahoe算法更高效
 * 
 * 数据包丢失检测：
 * 当接收方不能按顺序确认数据时，会重复发送
 * 上一个确认的序列号，这就是"重复ACK"
 * 3个重复ACK通常表示一个数据包丢失
 */
void fast_retransmit(CongestionControl* cc)
{
    if (cc == NULL) {
        return;
    }

    LOG_INFO("Fast Retransmit triggered: cwnd=%u, ssthresh=%u", cc->cwnd, cc->ssthresh);

    // 进入快速恢复
    fast_recovery(cc);

    cc->fast_retransmits++;
}

// ==================== RENO算法：快速恢复 ====================

/**
 * 快速恢复（Fast Recovery）
 * 
 * 进入条件：
 * - 快速重传后立即进入
 * - 或从拥塞避免/慢启动检测到3个重复ACK
 * 
 * 初始化阶段：
 * 1. ssthresh = max(cwnd/2, 2*MSS)
 * 2. cwnd = ssthresh + 3*MSS（3表示3个重复ACK）
 * 3. 记录恢复点为当前的最高序列号
 * 
 * 运行阶段：
 * - 每收到一个重复ACK，cwnd += 1 MSS（发送新数据）
 * - 收到新ACK时退出，进入拥塞避免
 * - 或发生超时时回到慢启动
 * 
 * 为什么加3*MSS？
 * 3个重复ACK表示网络中有3个数据包已被接收但乱序了
 * 增加3*MSS的cwnd允许继续发送这3个数据包
 * 
 * 目的：
 * - 在检测到丢包后快速恢复
 * - 避免cwnd过度下降
 * - 维持网络利用率
 */
void fast_recovery(CongestionControl* cc)
{
    if (cc == NULL) {
        return;
    }

    // 在进入快速恢复前，设置阈值
    uint32_t new_ssthresh = (cc->cwnd / 2 > 2 * MSS) ? (cc->cwnd / 2) : (2 * MSS);
    cc->ssthresh = new_ssthresh;

    // 设置新的拥塞窗口
    // cwnd = ssthresh + 3*MSS（3个已被接收的数据包）
    cc->cwnd = cc->ssthresh + 3 * MSS;

    // 进入快速恢复状态
    cc->state = FAST_RECOVERY;

    LOG_INFO("Entering Fast Recovery: ssthresh=%u, cwnd=%u", cc->ssthresh, cc->cwnd);
}

// ==================== 更新拥塞控制 ====================

/**
 * 更新拥塞控制状态（处理ACK）
 * 
 * 主要逻辑：
 * 1. 区分新ACK和重复ACK
 * 2. 新ACK时清除重复ACK计数
 * 3. 重复ACK时增加计数，达到阈值时触发快速重传
 * 4. 根据当前状态更新cwnd
 * 
 * 状态转换：
 * - SLOW_START：调用slow_start()
 * - CONGESTION_AVOIDANCE：调用congestion_avoidance()
 * - FAST_RECOVERY（新ACK）：检查是否应退出
 * - FAST_RECOVERY（重复ACK）：增加cwnd以发送新数据
 * 
 * @param cc 拥塞控制指针
 * @param ack_num 新的ACK号
 * @param is_duplicate_ack 是否为重复ACK
 * @return 成功返回true
 */
bool update_congestion_control(CongestionControl* cc, uint32_t ack_num, bool is_duplicate_ack)
{
    if (cc == NULL) {
        LOG_WARN("CongestionControl pointer is NULL");
        return false;
    }

    if (!is_duplicate_ack) {
        // ===== 新ACK：清除重复ACK计数 =====
        cc->dup_ack_count = 0;

        // 根据当前状态处理新ACK
        switch (cc->state) {
            case SLOW_START:
                // 慢启动阶段
                slow_start(cc);
                break;

            case CONGESTION_AVOIDANCE:
                // 拥塞避免阶段
                congestion_avoidance(cc);
                break;

            case FAST_RECOVERY:
                // 快速恢复中收到新ACK
                // 恢复完成，回到拥塞避免
                cc->state = CONGESTION_AVOIDANCE;
                cc->cwnd_inc = 0;
                LOG_INFO("Exiting Fast Recovery, entering CONGESTION_AVOIDANCE at cwnd=%u", cc->cwnd);
                break;

            default:
                LOG_WARN("Unknown congestion state: %d", cc->state);
                break;
        }

    } else {
        // ===== 重复ACK：增加计数 =====
        cc->dup_ack_count++;

        LOG_DEBUG("Duplicate ACK received: count=%d, state=%s",
                  cc->dup_ack_count, congestion_state_to_string(cc->state));

        // 检查是否触发快速重传
        if (cc->dup_ack_count == DUP_ACK_THRESHOLD) {
            // 第3个重复ACK：触发快速重传
            fast_retransmit(cc);

        } else if (cc->dup_ack_count > DUP_ACK_THRESHOLD && cc->state == FAST_RECOVERY) {
            // 在快速恢复中继续收到重复ACK
            // 增加cwnd以发送新数据
            cc->cwnd += MSS;
            LOG_DEBUG("Fast Recovery: ACK inflation, cwnd=%u", cc->cwnd);
        }
    }

    return true;
}

// ==================== 超时处理 ====================

/**
 * 处理超时事件
 * 
 * 超时事件比快速重传更严重，表示网络状况恶劣
 * 需要大幅降低发送速率并回到慢启动
 * 
 * 算法步骤：
 * 1. 设置ssthresh = cwnd/2
 * 2. 设置cwnd = 1 MSS（保守重启）
 * 3. 回到SLOW_START状态
 * 4. 清除重复ACK计数
 * 5. 增加RTO（指数退避）
 * 
 * Exponential Backoff：
 * 如果继续超时，RTO会不断增加：
 * RTO_new = min(RTO_old * 2, RTO_MAX)
 * 通常RTO_MAX = 60秒
 * 
 * 目的：
 * - 在严重拥塞时大幅降速
 * - 给网络时间恢复
 * - 避免加重网络负担
 */
bool handle_congestion_timeout(CongestionControl* cc)
{
    if (cc == NULL) {
        LOG_WARN("CongestionControl pointer is NULL");
        return false;
    }

    LOG_INFO("Timeout detected! Backing off: cwnd=%u → 1 MSS, ssthresh=%u",
             cc->cwnd, cc->cwnd / 2);

    // 设置新的慢启动阈值
    cc->ssthresh = (cc->cwnd / 2 > 2 * MSS) ? (cc->cwnd / 2) : (2 * MSS);

    // 重置拥塞窗口（保守）
    cc->cwnd = INITIAL_CWND;

    // 回到慢启动
    cc->state = SLOW_START;

    // 清除重复ACK计数
    cc->dup_ack_count = 0;

    // 应用指数退避增加RTO
    // （这里简化处理，实际应在RTT计算中体现）
    if (cc->rto < 32000) {  // 最大RTO = 32秒
        cc->rto *= 2;
    }

    cc->congestion_events++;

    LOG_INFO("Entering SLOW_START: cwnd=%u, ssthresh=%u, rto=%u",
             cc->cwnd, cc->ssthresh, cc->rto);

    return true;
}

// ==================== 窗口和超时查询 ====================

/**
 * 获取当前拥塞窗口大小
 */
uint32_t get_congestion_window(CongestionControl* cc)
{
    if (cc == NULL) {
        return INITIAL_CWND;
    }

    return cc->cwnd;
}

/**
 * 获取当前可发送字节数
 */
uint32_t get_send_allowance(CongestionControl* cc)
{
    if (cc == NULL) {
        return 0;
    }

    return cc->cwnd;
}

/**
 * 更新RTT估计值
 * 
 * 使用Karn/Partridge算法：
 * 1. 测量往返时间（sample_rtt）
 * 2. 更新RTT估计：RTT = 7/8*RTT + 1/8*sample_rtt
 * 3. 更新方差：RTTVAR = 3/4*RTTVAR + 1/4*|sample_rtt - RTT|
 * 4. 计算RTO：RTO = RTT + 4*RTTVAR
 * 5. RTO最小值：通常是200ms或1秒
 * 
 * 优点：
 * - 对单个样本的变化不敏感
 * - 自适应网络变化
 * - 对抖动有良好的容忍度
 */
bool update_rtt(CongestionControl* cc, uint32_t sample_rtt)
{
    if (cc == NULL || sample_rtt == 0) {
        LOG_WARN("Invalid RTT sample: %u", sample_rtt);
        return false;
    }

    // 计算RTT差值
    int32_t delta = sample_rtt - cc->rtt;
    if (delta < 0) {
        delta = -delta;
    }

    // 更新RTT估计
    // RTT_new = 7/8 * RTT_old + 1/8 * sample_rtt
    cc->rtt = (cc->rtt * 7 + sample_rtt) / 8;

    // 更新RTT方差
    // RTTVAR_new = 3/4 * RTTVAR_old + 1/4 * |delta|
    cc->rttvar = (cc->rttvar * 3 + delta) / 4;

    // 计算新的RTO
    // RTO = RTT + 4 * RTTVAR
    uint32_t new_rto = cc->rtt + 4 * cc->rttvar;

    // RTO最小值和最大值
    if (new_rto < 1000) {      // 最小1秒
        new_rto = 1000;
    }
    if (new_rto > 60000) {     // 最大60秒
        new_rto = 60000;
    }

    cc->rto = new_rto;

    LOG_DEBUG("RTT Updated: sample=%u, rtt=%u, rttvar=%u, rto=%u",
              sample_rtt, cc->rtt, cc->rttvar, cc->rto);

    return true;
}

/**
 * 获取重传超时时间
 */
uint32_t get_rto(CongestionControl* cc)
{
    if (cc == NULL) {
        return TIMEOUT_MS;
    }

    return cc->rto;
}

// ==================== 调试和管理 ====================

/**
 * 打印拥塞控制状态（调试用）
 */
void print_congestion_control(CongestionControl* cc)
{
    if (cc == NULL) {
        printf("Error: CongestionControl pointer is NULL\n");
        return;
    }

    printf("========== Congestion Control Status ==========\n");
    printf("State:             %s\n", congestion_state_to_string(cc->state));
    printf("CWND:              %u bytes (%.1f MSS)\n", cc->cwnd, (float)cc->cwnd / MSS);
    printf("SSTHRESH:          %u bytes (%.1f MSS)\n", cc->ssthresh, (float)cc->ssthresh / MSS);
    printf("CWND Increment:    %u\n", cc->cwnd_inc);
    printf("Duplicate ACKs:    %d/%d\n", cc->dup_ack_count, DUP_ACK_THRESHOLD);
    printf("Recovery Point:    %u\n", cc->recovery_point);
    printf("\nTiming Information:\n");
    printf("RTT Estimate:      %u ms\n", cc->rtt);
    printf("RTT Variance:      %u ms\n", cc->rttvar);
    printf("RTO:               %u ms\n", cc->rto);
    printf("\nStatistics:\n");
    printf("Congestion Events: %u\n", cc->congestion_events);
    printf("Fast Retransmits:  %u\n", cc->fast_retransmits);
    printf("================================================\n");
}

/**
 * 释放拥塞控制资源
 */
void free_congestion_control(CongestionControl* cc)
{
    if (cc == NULL) {
        return;
    }

    free(cc);
    LOG_INFO("Congestion control freed");
}
