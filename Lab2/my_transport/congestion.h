// congestion.h - 拥塞控制模块头文件
// 定义拥塞控制算法相关的结构和类

#ifndef CONGESTION_H
#define CONGESTION_H

#include "reliable_transport.h"
#include <cstdint>
#include <chrono>

// 拥塞控制状态枚举
enum CongestionState {
    SLOW_START = 0,         // 慢启动阶段
    CONGESTION_AVOIDANCE = 1, // 拥塞避免阶段
    FAST_RECOVERY = 2       // 快速恢复阶段
};

// 拥塞控制类
class CongestionControl {
private:
    uint32_t cwnd;          // 拥塞窗口大小（以数据包为单位）
    uint32_t ssthresh;      // 慢启动阈值
    uint32_t rwnd;          // 接收窗口大小（从接收方通告）
    uint32_t mss;           // 最大报文段大小（以字节为单位）
    
    CongestionState state;  // 当前拥塞控制状态
    
    // 快速重传相关参数
    int duplicate_acks;     // 收到的重复ACK数量
    
    // RTT估计相关参数
    double estimated_rtt;   // 估计的RTT
    double dev_rtt;         // RTT偏差
    double timeout_interval; // 当前超时时间间隔
    
    // 记录时间戳
    std::chrono::steady_clock::time_point last_ack_time; // 上次收到ACK的时间
    std::chrono::steady_clock::time_point last_congestion_time; // 上次拥塞的时间

public:
    // 构造函数和析构函数
    CongestionControl(uint32_t initial_cwnd = 1, uint32_t initial_ssthresh = 65535, uint32_t mss = 512);
    ~CongestionControl();
    
    // 拥塞控制算法核心函数
    void on_ack_received();          // 收到ACK时调用
    void on_duplicate_ack_received(); // 收到重复ACK时调用
    void on_timeout();               // 发生超时时调用
    void on_congestion_detected();   // 检测到拥塞时调用
    
    // RTT估计函数
    void update_rtt(double sample_rtt); // 更新RTT估计
    double get_timeout_interval() const; // 获取当前超时时间间隔
    
    // 窗口管理函数
    uint32_t get_congestion_window() const; // 获取当前拥塞窗口大小
    void update_receive_window(uint32_t new_rwnd); // 更新接收窗口大小
    uint32_t get_effective_window() const; // 获取有效窗口大小（取cwnd和rwnd的较小值）
    
    // 状态查询函数
    CongestionState get_state() const; // 获取当前拥塞控制状态
    uint32_t get_ssthresh() const;     // 获取当前慢启动阈值
    int get_duplicate_acks() const;    // 获取当前重复ACK计数
    
    // 重置函数
    void reset(); // 重置拥塞控制状态
};

// 拥塞控制工具函数
extern double calculate_backoff_timeout(double base_timeout, int retry_count);
extern uint32_t calculate_initial_ssthresh(uint32_t max_window_size);
extern void print_congestion_state(const CongestionControl& cc);

#endif // CONGESTION_H