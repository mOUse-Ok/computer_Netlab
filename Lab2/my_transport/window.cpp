// window.cpp - 窗口管理模块源文件
// 实现滑动窗口类的方法和窗口管理工具函数

#include "window.h"
#include <iostream>
#include <algorithm>
#include <chrono>

// 构造函数
SlidingWindow::SlidingWindow(uint32_t window_size) : 
    window_size(window_size),
    send_base(0),
    next_seq_num(0),
    recv_base(0) {
    // 初始化代码（如果需要）
}

// 析构函数
SlidingWindow::~SlidingWindow() {
    // 清理代码（如果需要）
    send_window.clear();
    receive_window.clear();
}

// 检查序列号是否在窗口内
bool SlidingWindow::is_seq_in_window(uint32_t seq_num, uint32_t window_base, uint32_t window_size) const {
    return is_sequence_in_window(seq_num, window_base, calculate_window_end(window_base, window_size));
}

// 添加数据包到发送窗口
bool SlidingWindow::add_to_send_window(const Packet& packet) {
    uint32_t seq_num = packet.header.seq_num;
    
    // 检查序列号是否应该在发送窗口中
    if (!is_seq_in_window(seq_num, send_base, window_size)) {
        return false;
    }
    
    // 创建发送窗口项
    SendWindowItem item;
    item.packet = packet;
    item.sent = false;
    item.acknowledged = false;
    item.retry_count = 0;
    
    // 添加到发送窗口
    send_window[seq_num] = item;
    
    // 更新next_seq_num
    if (seq_num >= next_seq_num) {
        next_seq_num = seq_num + 1;
    }
    
    return true;
}

// 标记数据包已确认
bool SlidingWindow::mark_packet_acknowledged(uint32_t seq_num) {
    // 查找数据包
    auto it = send_window.find(seq_num);
    if (it == send_window.end()) {
        return false;
    }
    
    // 标记为已确认
    it->second.acknowledged = true;
    
    // 尝试滑动窗口
    slide_send_window();
    
    return true;
}

// 获取超时的数据包
std::vector<uint32_t> SlidingWindow::get_timed_out_packets(int timeout_ms) {
    std::vector<uint32_t> timed_out_seqs;
    auto now = std::chrono::steady_clock::now();
    
    // 遍历发送窗口中已发送但未确认的数据包
    for (auto& pair : send_window) {
        auto& item = pair.second;
        if (item.sent && !item.acknowledged) {
            // 计算发送时间与当前时间的差值
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - item.send_time);
            
            // 检查是否超时
            if (duration.count() > timeout_ms) {
                timed_out_seqs.push_back(pair.first);
                item.retry_count++;
            }
        }
    }
    
    return timed_out_seqs;
}

// 检查发送窗口是否已满
bool SlidingWindow::is_send_window_full() const {
    return get_available_send_slots() == 0;
}

// 滑动发送窗口
void SlidingWindow::slide_send_window() {
    // 找到第一个未确认的数据包序列号
    uint32_t new_send_base = send_base;
    bool found = false;
    
    while (!found) {
        auto it = send_window.find(new_send_base);
        if (it == send_window.end() || !it->second.acknowledged) {
            found = true;
        } else {
            // 移除已确认且在窗口基址之前的数据包
            send_window.erase(it);
            new_send_base++;
        }
    }
    
    // 更新窗口基址
    send_base = new_send_base;
}

// 添加数据包到接收窗口
bool SlidingWindow::add_to_receive_window(const Packet& packet) {
    uint32_t seq_num = packet.header.seq_num;
    uint32_t window_end = calculate_window_end(recv_base, window_size);
    
    // 检查序列号是否在接收窗口范围内
    if (!is_sequence_in_window(seq_num, recv_base, window_end)) {
        return false;
    }
    
    // 创建或更新接收窗口项
    ReceiveWindowItem item;
    item.packet = packet;
    item.received = true;
    
    receive_window[seq_num] = item;
    
    // 尝试滑动窗口
    slide_receive_window();
    
    return true;
}

// 获取连续接收的数据包
std::vector<Packet> SlidingWindow::get_contiguous_received_packets() {
    std::vector<Packet> contiguous_packets;
    
    // 从接收窗口基址开始，收集连续接收的数据包
    uint32_t current_seq = recv_base;
    while (true) {
        auto it = receive_window.find(current_seq);
        if (it == receive_window.end() || !it->second.received) {
            break; // 不连续了，退出循环
        }
        
        // 添加到结果列表
        contiguous_packets.push_back(it->second.packet);
        
        // 移除已处理的数据包
        receive_window.erase(it);
        
        // 更新接收窗口基址
        recv_base = current_seq + 1;
        current_seq = recv_base;
    }
    
    return contiguous_packets;
}

// 检查数据包是否是期望接收的
bool SlidingWindow::is_packet_expected(uint32_t seq_num) const {
    uint32_t window_end = calculate_window_end(recv_base, window_size);
    return is_sequence_in_window(seq_num, recv_base, window_end);
}

// 滑动接收窗口
void SlidingWindow::slide_receive_window() {
    // 在get_contiguous_received_packets中已经处理了窗口滑动
    // 这里可以添加额外的滑动逻辑（如果需要）
}

// 获取窗口大小
uint32_t SlidingWindow::get_window_size() const {
    return window_size;
}

// 设置窗口大小
void SlidingWindow::set_window_size(uint32_t new_window_size) {
    window_size = new_window_size;
}

// 获取发送窗口基址
uint32_t SlidingWindow::get_send_base() const {
    return send_base;
}

// 获取下一个发送序列号
uint32_t SlidingWindow::get_next_seq_num() const {
    return next_seq_num;
}

// 获取接收窗口基址
uint32_t SlidingWindow::get_recv_base() const {
    return recv_base;
}

// 获取发送窗口可用槽位数量
uint32_t SlidingWindow::get_available_send_slots() const {
    uint32_t window_end = calculate_window_end(send_base, window_size);
    uint32_t used_slots = 0;
    
    // 计算已使用的槽位
    for (const auto& pair : send_window) {
        if (is_sequence_in_window(pair.first, send_base, window_end) && 
            !pair.second.acknowledged) {
            used_slots++;
        }
    }
    
    return window_size - used_slots;
}

// 增加序列号
uint32_t SlidingWindow::increment_sequence(uint32_t seq_num, uint32_t increment) const {
    return (seq_num + increment) % (1UL << 31); // 使用31位序列号空间，避免溢出
}

// 比较序列号
int SlidingWindow::compare_sequences(uint32_t seq1, uint32_t seq2) const {
    // 处理序列号回绕的情况
    const uint32_t half_range = 1UL << 30;
    
    if (seq1 == seq2) {
        return 0;
    }
    else if ((seq1 < seq2 && seq2 - seq1 < half_range) || 
             (seq1 > seq2 && seq1 - seq2 > half_range)) {
        return -1; // seq1 < seq2
    }
    else {
        return 1; // seq1 > seq2
    }
}

// 重置窗口
void SlidingWindow::reset() {
    send_base = 0;
    next_seq_num = 0;
    recv_base = 0;
    send_window.clear();
    receive_window.clear();
}

// 计算窗口结束位置
uint32_t calculate_window_end(uint32_t window_base, uint32_t window_size) {
    // 计算窗口结束位置，考虑序列号回绕
    return window_base + window_size - 1;
}

// 检查序列号是否在窗口内
bool is_sequence_in_window(uint32_t seq_num, uint32_t window_base, uint32_t window_end) {
    // 处理序列号回绕的情况
    if (window_base <= window_end) {
        // 窗口没有回绕
        return (seq_num >= window_base) && (seq_num <= window_end);
    } else {
        // 窗口已经回绕
        return (seq_num >= window_base) || (seq_num <= window_end);
    }
}

// 包装序列号（处理回绕）
uint32_t wrap_sequence(uint32_t seq_num) {
    // 使用31位序列号空间，避免溢出
    return seq_num % (1UL << 31);
}