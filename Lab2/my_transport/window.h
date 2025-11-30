// window.h - 窗口管理模块头文件
// 定义滑动窗口相关的结构和类

#ifndef WINDOW_H
#define WINDOW_H

#include "reliable_transport.h"
#include "packet.h"
#include <cstdint>
#include <vector>
#include <map>
#include <chrono>

// 发送窗口项结构
struct SendWindowItem {
    Packet packet;                // 数据包
    bool sent;                    // 是否已发送
    bool acknowledged;            // 是否已确认
    std::chrono::steady_clock::time_point send_time; // 发送时间
    int retry_count;              // 重传次数
};

// 接收窗口项结构
struct ReceiveWindowItem {
    Packet packet;                // 数据包
    bool received;                // 是否已接收
};

// 滑动窗口类
class SlidingWindow {
private:
    uint32_t window_size;         // 窗口大小
    uint32_t send_base;           // 发送窗口基址（第一个未确认的数据包序列号）
    uint32_t next_seq_num;        // 下一个要发送的数据包序列号
    uint32_t recv_base;           // 接收窗口基址（期望接收的下一个数据包序列号）
    
    std::map<uint32_t, SendWindowItem> send_window;     // 发送窗口
    std::map<uint32_t, ReceiveWindowItem> receive_window; // 接收窗口
    
    // 检查序列号是否在窗口内的辅助函数
    bool is_seq_in_window(uint32_t seq_num, uint32_t window_base, uint32_t window_size) const;
    
public:
    // 构造函数和析构函数
    SlidingWindow(uint32_t window_size = WINDOW_SIZE);
    ~SlidingWindow();
    
    // 发送窗口操作
    bool add_to_send_window(const Packet& packet);
    bool mark_packet_acknowledged(uint32_t seq_num);
    std::vector<uint32_t> get_timed_out_packets(int timeout_ms);
    bool is_send_window_full() const;
    void slide_send_window();
    
    // 接收窗口操作
    bool add_to_receive_window(const Packet& packet);
    std::vector<Packet> get_contiguous_received_packets();
    bool is_packet_expected(uint32_t seq_num) const;
    void slide_receive_window();
    
    // 窗口状态查询
    uint32_t get_window_size() const;
    void set_window_size(uint32_t new_window_size);
    uint32_t get_send_base() const;
    uint32_t get_next_seq_num() const;
    uint32_t get_recv_base() const;
    uint32_t get_available_send_slots() const;
    
    // 序列号管理
    uint32_t increment_sequence(uint32_t seq_num, uint32_t increment = 1) const;
    int compare_sequences(uint32_t seq1, uint32_t seq2) const;
    
    // 重置窗口
    void reset();
};

// 窗口管理工具函数
extern uint32_t calculate_window_end(uint32_t window_base, uint32_t window_size);
extern bool is_sequence_in_window(uint32_t seq_num, uint32_t window_base, uint32_t window_end);
extern uint32_t wrap_sequence(uint32_t seq_num);

#endif // WINDOW_H