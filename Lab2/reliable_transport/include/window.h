#ifndef WINDOW_H
#define WINDOW_H

#include <cstdint>
#include <ctime>
#include <cstdbool>
#include "reliable_transport.h"
#include "packet.h"

// ==================== 未确认数据包结构 ====================

/**
 * 发送窗口中的未确认数据包
 * 
 * 用于跟踪已发送但尚未被确认的数据包
 * 包含重传信息和超时管理
 */
typedef struct {
    Frame frame;                       // 数据包内容
    time_t send_time;                  // 发送时间戳（秒）
    int retry_count;                   // 重传次数（初始为0）
    bool is_retransmitted;             // 是否为重传数据包
    uint32_t seq_num;                  // 序列号（便于查找）
    bool is_valid;                     // 该槽位是否有效
} UnackedPacket;

// ==================== 发送窗口结构 ====================

/**
 * 发送窗口（Go-Back-N或选择性重传）
 * 
 * 用于管理已发送但未确认的数据包
 * 支持流水线传输和超时重传
 */
typedef struct {
    UnackedPacket* packets;            // 未确认数据包数组
    int window_size;                   // 窗口大小（最多同时发送的包数）
    uint32_t base;                     // 窗口基序列号（最早的未确认包）
    uint32_t next_seq_num;             // 下一个要发送的序列号
    int max_packets;                   // 最大未确认数据包数量
    int packet_count;                  // 当前窗口中的包数量
} SendWindow;

// ==================== 接收窗口结构 ====================

/**
 * 接收窗口（缓冲乱序数据包）
 * 
 * 用于接收和重组数据，支持乱序接收
 * 缓冲超出序列的数据包，直到所有数据到达
 */
typedef struct {
    uint8_t** buffer;                  // 接收数据缓冲区（指针数组）
    uint16_t* data_len;                // 每个缓冲区的数据长度
    bool* received;                    // 标记每个位置是否已接收
    int window_size;                   // 窗口大小
    uint32_t base;                     // 窗口基序列号（最早的未交付包）
    uint32_t expected_seq;             // 期望接收的下一个序列号
    int max_buffer_size;               // 单个数据包的最大缓冲区大小
} ReceiveWindow;

// ==================== 发送窗口函数 ====================

/**
 * 创建发送窗口
 * 
 * 初始化发送窗口，分配内存用于缓冲未确认的数据包
 * 
 * @param window_size 窗口大小（最多同时发送的包数）
 * @param max_packets 最大未确认数据包数量
 * @return 返回分配的发送窗口指针，失败返回NULL
 */
SendWindow* create_send_window(int window_size, int max_packets);

/**
 * 添加数据包到发送窗口
 * 
 * 将要发送的数据包加入窗口，准备发送
 * 
 * @param window 发送窗口指针
 * @param frame 要发送的帧
 * @return 成功返回true，失败返回false
 */
bool add_to_send_window(SendWindow* window, const Frame* frame);

/**
 * 检查发送窗口是否已满
 * 
 * 判断是否可以继续添加新的数据包
 * 
 * @param window 发送窗口指针
 * @return 窗口已满返回true，否则返回false
 */
bool is_send_window_full(SendWindow* window);

/**
 * 检查是否有未确认的数据包
 * 
 * @param window 发送窗口指针
 * @return 有未确认包返回true，否则返回false
 */
bool has_unacked_packets(SendWindow* window);

/**
 * 获取指定序列号的未确认数据包
 * 
 * @param window 发送窗口指针
 * @param seq_num 序列号
 * @return 找到返回指针，否则返回NULL
 */
UnackedPacket* get_unacked_packet(SendWindow* window, uint32_t seq_num);

/**
 * 处理收到的ACK，更新发送窗口
 * 
 * 根据ACK号滑动窗口，释放已确认的数据包
 * 
 * @param window 发送窗口指针
 * @param ack_num 收到的ACK号
 * @return 成功返回true，否则返回false
 */
bool update_send_window(SendWindow* window, uint32_t ack_num);

/**
 * 获取可用的发送窗口大小
 * 
 * @param window 发送窗口指针
 * @return 可用窗口大小（字节数）
 */
uint32_t get_send_window_available(SendWindow* window);

/**
 * 释放发送窗口资源
 * 
 * @param window 发送窗口指针
 */
void free_send_window(SendWindow* window);

/**
 * 打印发送窗口状态（调试用）
 * 
 * @param window 发送窗口指针
 */
void print_send_window(SendWindow* window);

// ==================== 接收窗口函数 ====================

/**
 * 创建接收窗口
 * 
 * 初始化接收窗口，分配缓冲区用于接收乱序数据包
 * 
 * @param window_size 窗口大小
 * @param buffer_size 单个数据包的最大缓冲区大小
 * @return 返回分配的接收窗口指针，失败返回NULL
 */
ReceiveWindow* create_receive_window(int window_size, int buffer_size);

/**
 * 接收数据包到接收窗口
 * 
 * 处理接收到的数据包，支持乱序接收
 * 
 * @param window 接收窗口指针
 * @param frame 接收到的帧
 * @return 成功返回true，失败返回false
 */
bool receive_packet(ReceiveWindow* window, const Frame* frame);

/**
 * 获取连续的已接收数据
 * 
 * 从窗口中提取已按顺序接收的数据，交付给应用层
 * 
 * @param window 接收窗口指针
 * @param output 输出缓冲区（必须足够大）
 * @return 返回提取的数据字节数
 */
int get_contiguous_data(ReceiveWindow* window, uint8_t* output);

/**
 * 获取当前可用的接收窗口大小
 * 
 * @param window 接收窗口指针
 * @return 可用窗口大小（字节数）
 */
uint16_t get_receive_window_available(ReceiveWindow* window);

/**
 * 释放接收窗口资源
 * 
 * @param window 接收窗口指针
 */
void free_receive_window(ReceiveWindow* window);

/**
 * 打印接收窗口状态（调试用）
 * 
 * @param window 接收窗口指针
 */
void print_receive_window(ReceiveWindow* window);

// ==================== 超时重传函数 ====================

/**
 * 检查发送窗口中的超时包
 * 
 * 扫描所有未确认的数据包，检查是否超时
 * 如果超时，触发重传
 * 
 * @param window 发送窗口指针
 * @param rto 重传超时时间（秒）
 * @return 返回需要重传的包数量
 */
int check_send_timeouts(SendWindow* window, time_t rto);

/**
 * 重传指定序列号的数据包
 * 
 * @param window 发送窗口指针
 * @param seq_num 要重传的序列号
 * @return 成功返回true，否则返回false
 */
bool retransmit_packet(SendWindow* window, uint32_t seq_num);

/**
 * 检查并应用超时退避（拥塞）
 * 
 * 在检测到超时后调用，用于调整RTO
 * 实现exponential backoff机制
 * 
 * @param window 发送窗口指针
 * @param rto 当前RTO，返回新的RTO
 * @return 返回新的RTO值
 */
time_t apply_timeout_backoff(SendWindow* window, time_t rto);

#endif // WINDOW_H
