// congestion.cpp - 拥塞控制模块源文件
// 实现拥塞控制类的方法和拥塞控制工具函数

#include "congestion.h"
#include <iostream>
#include <cmath>

// 构造函数
CongestionControl::CongestionControl(uint32_t initial_cwnd, uint32_t initial_ssthresh, uint32_t mss) :
    cwnd(initial_cwnd),
    ssthresh(initial_ssthresh),
    rwnd(WINDOW_SIZE), // 默认值，应该由接收方动态更新
    mss(mss),
    state(SLOW_START),
    duplicate_acks(0),
    estimated_rtt(0.0),
    dev_rtt(0.0),
    timeout_interval(TIMEOUT_MS / 1000.0) // 初始超时时间（从毫秒转换为秒）
{
    // 初始化时间戳
    last_ack_time = std::chrono::steady_clock::now();
    last_congestion_time = std::chrono::steady_clock::now();
}

// 析构函数
CongestionControl::~CongestionControl() {
    // 清理代码（如果需要）
}

// 收到ACK时调用
void CongestionControl::on_ack_received() {
    // 更新最后收到ACK的时间
    last_ack_time = std::chrono::steady_clock::now();
    
    // 根据当前拥塞控制状态调整cwnd
    switch (state) {
        case SLOW_START:
            // 慢启动：每个ACK增加1个MSS
            cwnd += 1;
            
            // 检查是否达到慢启动阈值
            if (cwnd >= ssthresh) {
                state = CONGESTION_AVOIDANCE;
                std::cout << "进入拥塞避免阶段，cwnd = " << cwnd << ", ssthresh = " << ssthresh << std::endl;
            }
            break;
            
        case CONGESTION_AVOIDANCE:
            // 拥塞避免：每个RTT增加1个MSS
            // 这里简化处理，实际上应该是每收到一个ACK就增加 1/cwnd
            cwnd += 1 / static_cast<double>(cwnd);
            break;
            
        case FAST_RECOVERY:
            // 快速恢复：每个重复ACK增加1个MSS
            // 当收到新的ACK时，退出快速恢复，进入拥塞避免
            cwnd = ssthresh;
            state = CONGESTION_AVOIDANCE;
            duplicate_acks = 0;
            std::cout << "退出快速恢复阶段，进入拥塞避免阶段，cwnd = " << cwnd << ", ssthresh = " << ssthresh << std::endl;
            break;
    }
    
    // 重置重复ACK计数
    duplicate_acks = 0;
}

// 收到重复ACK时调用
void CongestionControl::on_duplicate_ack_received() {
    duplicate_acks++;
    
    // 收到3个重复ACK，采用快速重传
    if (duplicate_acks == 3) {
        on_congestion_detected();
        
        // 快速重传和快速恢复
        ssthresh = std::max(cwnd / 2, 2U); // 将ssthresh减半，至少为2
        cwnd = ssthresh + 3; // 设置cwnd为ssthresh + 3（3个重复ACK）
        state = FAST_RECOVERY;
        
        std::cout << "检测到拥塞（3个重复ACK），进入快速恢复阶段，cwnd = " << cwnd << ", ssthresh = " << ssthresh << std::endl;
    }
    // 收到更多的重复ACK（在快速恢复阶段）
    else if (duplicate_acks > 3 && state == FAST_RECOVERY) {
        cwnd += 1; // 每个重复ACK增加1个MSS
    }
}

// 发生超时时调用
void CongestionControl::on_timeout() {
    on_congestion_detected();
    
    // 超时处理
    ssthresh = std::max(cwnd / 2, 2U); // 将ssthresh减半，至少为2
    cwnd = 1; // 重置cwnd为1，进入慢启动
    state = SLOW_START;
    duplicate_acks = 0;
    
    // 增加超时时间间隔
    timeout_interval *= 2;
    
    std::cout << "发生超时，进入慢启动阶段，cwnd = " << cwnd << ", ssthresh = " << ssthresh << ", timeout = " << timeout_interval << "s" << std::endl;
}

// 检测到拥塞时调用
void CongestionControl::on_congestion_detected() {
    // 更新最后拥塞时间
    last_congestion_time = std::chrono::steady_clock::now();
    
    // 这里可以添加拥塞统计信息或其他拥塞处理逻辑
}

// 更新RTT估计
void CongestionControl::update_rtt(double sample_rtt) {
    const double alpha = 0.125; // RTT平滑因子
    const double beta = 0.25;   // RTT偏差平滑因子
    
    if (estimated_rtt == 0.0) {
        // 首次测量
        estimated_rtt = sample_rtt;
        dev_rtt = sample_rtt / 2;
    } else {
        // 更新估计RTT和RTT偏差
        dev_rtt = (1 - beta) * dev_rtt + beta * std::abs(sample_rtt - estimated_rtt);
        estimated_rtt = (1 - alpha) * estimated_rtt + alpha * sample_rtt;
    }
    
    // 计算超时时间间隔：estimated_rtt + 4 * dev_rtt
    timeout_interval = estimated_rtt + 4 * dev_rtt;
    
    // 确保超时时间间隔在合理范围内
    const double min_timeout = 100.0 / 1000.0; // 最小超时时间0.1秒
    const double max_timeout = 60.0;           // 最大超时时间60秒
    timeout_interval = std::max(min_timeout, std::min(timeout_interval, max_timeout));
}

// 获取当前超时时间间隔
double CongestionControl::get_timeout_interval() const {
    return timeout_interval;
}

// 获取当前拥塞窗口大小
uint32_t CongestionControl::get_congestion_window() const {
    return static_cast<uint32_t>(cwnd);
}

// 更新接收窗口大小
void CongestionControl::update_receive_window(uint32_t new_rwnd) {
    rwnd = new_rwnd;
}

// 获取有效窗口大小
uint32_t CongestionControl::get_effective_window() const {
    return std::min(static_cast<uint32_t>(cwnd), rwnd);
}

// 获取当前拥塞控制状态
CongestionState CongestionControl::get_state() const {
    return state;
}

// 获取当前慢启动阈值
uint32_t CongestionControl::get_ssthresh() const {
    return ssthresh;
}

// 获取当前重复ACK计数
int CongestionControl::get_duplicate_acks() const {
    return duplicate_acks;
}

// 重置拥塞控制状态
void CongestionControl::reset() {
    cwnd = 1;
    ssthresh = 65535;
    rwnd = WINDOW_SIZE;
    state = SLOW_START;
    duplicate_acks = 0;
    estimated_rtt = 0.0;
    dev_rtt = 0.0;
    timeout_interval = TIMEOUT_MS / 1000.0;
    
    last_ack_time = std::chrono::steady_clock::now();
    last_congestion_time = std::chrono::steady_clock::now();
}

// 计算退避超时时间
double calculate_backoff_timeout(double base_timeout, int retry_count) {
    // 指数退避：base_timeout * (2^retry_count)
    return base_timeout * std::pow(2.0, retry_count);
}

// 计算初始慢启动阈值
uint32_t calculate_initial_ssthresh(uint32_t max_window_size) {
    // 初始慢启动阈值通常设为较大的值，这里设为最大窗口大小的2倍
    return max_window_size * 2;
}

// 打印拥塞控制状态
void print_congestion_state(const CongestionControl& cc) {
    std::cout << "拥塞控制状态:" << std::endl;
    std::cout << "  状态: ";
    switch (cc.get_state()) {
        case SLOW_START:
            std::cout << "慢启动";
            break;
        case CONGESTION_AVOIDANCE:
            std::cout << "拥塞避免";
            break;
        case FAST_RECOVERY:
            std::cout << "快速恢复";
            break;
    }
    std::cout << std::endl;
    std::cout << "  拥塞窗口(cwnd): " << cc.get_congestion_window() << std::endl;
    std::cout << "  慢启动阈值(ssthresh): " << cc.get_ssthresh() << std::endl;
    std::cout << "  重复ACK数: " << cc.get_duplicate_acks() << std::endl;
    std::cout << "  超时时间: " << cc.get_timeout_interval() * 1000 << " ms" << std::endl;
}