#include "window.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <time.h>

// ==================== 日志宏 ====================

#define LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)

// ==================== 发送窗口实现 ====================

/**
 * 创建发送窗口
 * 
 * 初始化发送窗口结构，分配内存存储未确认的数据包
 */
SendWindow* create_send_window(int window_size, int max_packets)
{
    // 参数验证
    if (window_size <= 0 || max_packets <= 0) {
        LOG_WARN("Invalid window parameters: window_size=%d, max_packets=%d", window_size, max_packets);
        return NULL;
    }

    // 分配发送窗口结构
    SendWindow* window = (SendWindow*)malloc(sizeof(SendWindow));
    if (window == NULL) {
        LOG_WARN("Failed to allocate SendWindow structure");
        return NULL;
    }

    // 分配未确认数据包数组
    window->packets = (UnackedPacket*)malloc(sizeof(UnackedPacket) * max_packets);
    if (window->packets == NULL) {
        LOG_WARN("Failed to allocate packets array");
        free(window);
        return NULL;
    }

    // 初始化结构字段
    memset(window->packets, 0, sizeof(UnackedPacket) * max_packets);
    
    window->window_size = window_size;
    window->base = 0;                  // 窗口基序列号
    window->next_seq_num = 0;          // 下一个要发送的序列号
    window->max_packets = max_packets;
    window->packet_count = 0;          // 当前窗口中的包数量

    LOG_INFO("Send window created: size=%d, max_packets=%d", window_size, max_packets);

    return window;
}

/**
 * 添加数据包到发送窗口
 * 
 * 将新的数据包加入窗口，等待发送
 * 计算窗口中的位置并存储数据包信息
 */
bool add_to_send_window(SendWindow* window, const Frame* frame)
{
    if (window == NULL || frame == NULL) {
        LOG_WARN("Invalid parameters: window=%p, frame=%p", window, frame);
        return false;
    }

    // 检查窗口是否已满
    if (is_send_window_full(window)) {
        LOG_WARN("Send window is full: count=%d, size=%d", window->packet_count, window->window_size);
        return false;
    }

    // 计算在数组中的位置
    int index = window->next_seq_num % window->max_packets;

    // 检查是否超出数组边界
    if (index >= window->max_packets) {
        LOG_WARN("Index out of bounds: index=%d, max_packets=%d", index, window->max_packets);
        return false;
    }

    // 在该位置存储数据包
    UnackedPacket* unacked = &window->packets[index];
    unacked->frame = *frame;
    unacked->seq_num = window->next_seq_num;
    unacked->send_time = time(NULL);
    unacked->retry_count = 0;
    unacked->is_retransmitted = false;
    unacked->is_valid = true;

    window->next_seq_num++;
    window->packet_count++;

    LOG_DEBUG("Added packet to send window: seq=%u, count=%d/%d", 
              frame->seq_num, window->packet_count, window->window_size);

    return true;
}

/**
 * 检查发送窗口是否已满
 * 
 * 窗口满的条件：packet_count >= window_size
 */
bool is_send_window_full(SendWindow* window)
{
    if (window == NULL) {
        return true;
    }

    return window->packet_count >= window->window_size;
}

/**
 * 检查是否有未确认的数据包
 */
bool has_unacked_packets(SendWindow* window)
{
    if (window == NULL) {
        return false;
    }

    return window->packet_count > 0;
}

/**
 * 获取指定序列号的未确认数据包
 * 
 * 根据序列号在窗口中查找对应的数据包
 */
UnackedPacket* get_unacked_packet(SendWindow* window, uint32_t seq_num)
{
    if (window == NULL) {
        return NULL;
    }

    // 计算索引
    int index = seq_num % window->max_packets;

    if (index >= window->max_packets) {
        return NULL;
    }

    UnackedPacket* unacked = &window->packets[index];

    // 检查该槽位是否有效且序列号匹配
    if (unacked->is_valid && unacked->seq_num == seq_num) {
        return unacked;
    }

    return NULL;
}

/**
 * 更新发送窗口（处理ACK）
 * 
 * 当接收到ACK时，滑动窗口，释放已确认的数据包
 * 
 * 算法：
 * 1. 检查ACK号是否有效
 * 2. 释放所有序列号 < ACK号的数据包
 * 3. 更新窗口基序列号
 * 4. 滑动窗口
 */
bool update_send_window(SendWindow* window, uint32_t ack_num)
{
    if (window == NULL) {
        LOG_WARN("Window pointer is NULL");
        return false;
    }

    // ACK号应该在窗口范围内
    if (ack_num < window->base || ack_num > window->next_seq_num) {
        LOG_WARN("ACK number out of range: ack=%u, base=%u, next=%u",
                 ack_num, window->base, window->next_seq_num);
        return false;
    }

    // 计算需要释放的数据包数量
    int packets_to_release = ack_num - window->base;

    LOG_DEBUG("Updating send window: ack=%u, releasing %d packets", ack_num, packets_to_release);

    // 释放已确认的数据包
    for (int i = 0; i < packets_to_release && window->packet_count > 0; i++) {
        int index = window->base % window->max_packets;
        
        if (window->packets[index].is_valid) {
            window->packets[index].is_valid = false;
            memset(&window->packets[index], 0, sizeof(UnackedPacket));
            window->packet_count--;
        }

        window->base++;
    }

    return true;
}

/**
 * 获取可用的发送窗口大小（字节）
 */
uint32_t get_send_window_available(SendWindow* window)
{
    if (window == NULL) {
        return 0;
    }

    int available = window->window_size - window->packet_count;
    return (available > 0) ? available * MAX_DATA_LENGTH : 0;
}

/**
 * 释放发送窗口资源
 */
void free_send_window(SendWindow* window)
{
    if (window == NULL) {
        return;
    }

    if (window->packets != NULL) {
        free(window->packets);
        window->packets = NULL;
    }

    free(window);

    LOG_INFO("Send window freed");
}

/**
 * 打印发送窗口状态（调试用）
 */
void print_send_window(SendWindow* window)
{
    if (window == NULL) {
        printf("Error: SendWindow pointer is NULL\n");
        return;
    }

    printf("========== Send Window Status ==========\n");
    printf("Window Size:       %d\n", window->window_size);
    printf("Base Seq:          %u\n", window->base);
    printf("Next Seq:          %u\n", window->next_seq_num);
    printf("Packet Count:      %d/%d\n", window->packet_count, window->window_size);
    printf("\nUnacked Packets:\n");

    for (int i = 0; i < window->max_packets && i < 10; i++) {
        if (window->packets[i].is_valid) {
            printf("  [%d] Seq=%u, Retries=%d, Retransmitted=%s, Time=%ld\n",
                   i,
                   window->packets[i].seq_num,
                   window->packets[i].retry_count,
                   window->packets[i].is_retransmitted ? "Yes" : "No",
                   window->packets[i].send_time);
        }
    }

    printf("========================================\n");
}

// ==================== 接收窗口实现 ====================

/**
 * 创建接收窗口
 * 
 * 初始化接收窗口，为乱序接收的数据包分配缓冲区
 */
ReceiveWindow* create_receive_window(int window_size, int buffer_size)
{
    // 参数验证
    if (window_size <= 0 || buffer_size <= 0) {
        LOG_WARN("Invalid window parameters: window_size=%d, buffer_size=%d", window_size, buffer_size);
        return NULL;
    }

    // 分配接收窗口结构
    ReceiveWindow* window = (ReceiveWindow*)malloc(sizeof(ReceiveWindow));
    if (window == NULL) {
        LOG_WARN("Failed to allocate ReceiveWindow structure");
        return NULL;
    }

    // 分配缓冲区指针数组
    window->buffer = (uint8_t**)malloc(sizeof(uint8_t*) * window_size);
    if (window->buffer == NULL) {
        LOG_WARN("Failed to allocate buffer array");
        free(window);
        return NULL;
    }

    // 分配数据长度数组
    window->data_len = (uint16_t*)malloc(sizeof(uint16_t) * window_size);
    if (window->data_len == NULL) {
        LOG_WARN("Failed to allocate data_len array");
        free(window->buffer);
        free(window);
        return NULL;
    }

    // 分配接收标志数组
    window->received = (bool*)malloc(sizeof(bool) * window_size);
    if (window->received == NULL) {
        LOG_WARN("Failed to allocate received array");
        free(window->buffer);
        free(window->data_len);
        free(window);
        return NULL;
    }

    // 初始化缓冲区数组（每个位置分配一个数据缓冲）
    for (int i = 0; i < window_size; i++) {
        window->buffer[i] = (uint8_t*)malloc(buffer_size);
        if (window->buffer[i] == NULL) {
            LOG_WARN("Failed to allocate buffer[%d]", i);
            // 释放已分配的缓冲
            for (int j = 0; j < i; j++) {
                free(window->buffer[j]);
            }
            free(window->buffer);
            free(window->data_len);
            free(window->received);
            free(window);
            return NULL;
        }
    }

    // 初始化结构字段
    window->window_size = window_size;
    window->base = 0;
    window->expected_seq = 0;
    window->max_buffer_size = buffer_size;
    memset(window->received, 0, sizeof(bool) * window_size);
    memset(window->data_len, 0, sizeof(uint16_t) * window_size);

    LOG_INFO("Receive window created: size=%d, buffer_size=%d", window_size, buffer_size);

    return window;
}

/**
 * 接收数据包到接收窗口
 * 
 * 处理接收到的数据包，支持乱序接收
 * 
 * 算法：
 * 1. 检查序列号是否在窗口范围内
 * 2. 计算在窗口中的位置
 * 3. 如果未收到过，将数据存储到缓冲区
 * 4. 标记该位置已接收
 */
bool receive_packet(ReceiveWindow* window, const Frame* frame)
{
    if (window == NULL || frame == NULL) {
        LOG_WARN("Invalid parameters: window=%p, frame=%p", window, frame);
        return false;
    }

    uint32_t seq_num = frame->seq_num;

    // 检查序列号是否在接收窗口范围内
    int32_t diff = seq_num - window->expected_seq;
    if (diff < 0 || diff >= window->window_size) {
        LOG_WARN("Packet out of window: seq=%u, expected=%u, window_size=%d",
                 seq_num, window->expected_seq, window->window_size);
        return false;
    }

    // 计算在窗口中的位置
    int index = diff;

    // 如果已经接收过该数据包，忽略
    if (window->received[index]) {
        LOG_DEBUG("Duplicate packet received: seq=%u", seq_num);
        return true;  // 返回true表示处理成功
    }

    // 复制数据到缓冲区
    if (frame->data_len > window->max_buffer_size) {
        LOG_WARN("Data length exceeds buffer size: data_len=%u, buffer_size=%d",
                 frame->data_len, window->max_buffer_size);
        return false;
    }

    memcpy(window->buffer[index], frame->data, frame->data_len);
    window->data_len[index] = frame->data_len;
    window->received[index] = true;

    LOG_DEBUG("Packet received: seq=%u, data_len=%u, position=%d", 
              seq_num, frame->data_len, index);

    return true;
}

/**
 * 获取连续的已接收数据
 * 
 * 从窗口基开始，提取所有连续已接收的数据
 * 并滑动窗口
 * 
 * @return 返回提取的数据字节数
 */
int get_contiguous_data(ReceiveWindow* window, uint8_t* output)
{
    if (window == NULL || output == NULL) {
        LOG_WARN("Invalid parameters: window=%p, output=%p", window, output);
        return 0;
    }

    int total_bytes = 0;
    int i = 0;

    // 从窗口基开始，提取连续的已接收数据
    while (i < window->window_size && window->received[i]) {
        int copy_len = window->data_len[i];
        
        // 检查缓冲区大小是否足够
        // （在实际应用中应该有更好的边界检查）

        memcpy(output + total_bytes, window->buffer[i], copy_len);
        total_bytes += copy_len;

        i++;
    }

    // 如果有连续数据被提取，滑动窗口
    if (i > 0) {
        // 滑动窗口
        for (int j = i; j < window->window_size; j++) {
            window->received[j - i] = window->received[j];
            window->data_len[j - i] = window->data_len[j];
            
            if (window->received[j - i]) {
                memcpy(window->buffer[j - i], window->buffer[j], window->data_len[j]);
            }
        }

        // 清除滑过的位置
        for (int j = window->window_size - i; j < window->window_size; j++) {
            window->received[j] = false;
            window->data_len[j] = 0;
            memset(window->buffer[j], 0, window->max_buffer_size);
        }

        // 更新期望序列号
        window->expected_seq += i;
        window->base += i;

        LOG_DEBUG("Sliding receive window: moved %d positions, total_bytes=%d", i, total_bytes);
    }

    return total_bytes;
}

/**
 * 获取接收窗口的可用空间（字节）
 */
uint16_t get_receive_window_available(ReceiveWindow* window)
{
    if (window == NULL) {
        return 0;
    }

    // 计算还有多少个未接收的位置
    int unrecv_count = 0;
    for (int i = 0; i < window->window_size; i++) {
        if (!window->received[i]) {
            unrecv_count++;
        }
    }

    return unrecv_count * MAX_DATA_LENGTH;
}

/**
 * 释放接收窗口资源
 */
void free_receive_window(ReceiveWindow* window)
{
    if (window == NULL) {
        return;
    }

    if (window->buffer != NULL) {
        for (int i = 0; i < window->window_size; i++) {
            if (window->buffer[i] != NULL) {
                free(window->buffer[i]);
            }
        }
        free(window->buffer);
    }

    if (window->data_len != NULL) {
        free(window->data_len);
    }

    if (window->received != NULL) {
        free(window->received);
    }

    free(window);

    LOG_INFO("Receive window freed");
}

/**
 * 打印接收窗口状态（调试用）
 */
void print_receive_window(ReceiveWindow* window)
{
    if (window == NULL) {
        printf("Error: ReceiveWindow pointer is NULL\n");
        return;
    }

    printf("========== Receive Window Status ==========\n");
    printf("Window Size:       %d\n", window->window_size);
    printf("Base Seq:          %u\n", window->base);
    printf("Expected Seq:      %u\n", window->expected_seq);
    printf("\nReceived Status:\n");

    for (int i = 0; i < window->window_size; i++) {
        printf("  [%d] Expected=%u, Received=%s, DataLen=%u\n",
               i,
               window->expected_seq + i,
               window->received[i] ? "Yes" : "No",
               window->data_len[i]);
    }

    printf("==========================================\n");
}

// ==================== 超时重传实现 ====================

/**
 * 检查发送窗口中的超时包
 * 
 * 扫描所有未确认的数据包，检查是否超时
 * 如果超时（当前时间 - 发送时间 > rto），标记为需要重传
 * 
 * @param window 发送窗口指针
 * @param rto 重传超时时间（秒）
 * @return 返回需要重传的包数量
 */
int check_send_timeouts(SendWindow* window, time_t rto)
{
    if (window == NULL) {
        return 0;
    }

    time_t current_time = time(NULL);
    int timeout_count = 0;

    // 扫描所有未确认的数据包
    for (int i = 0; i < window->max_packets; i++) {
        UnackedPacket* unacked = &window->packets[i];

        if (!unacked->is_valid) {
            continue;  // 跳过无效槽位
        }

        time_t elapsed = current_time - unacked->send_time;

        // 检查是否超时
        if (elapsed > rto) {
            LOG_WARN("Packet timeout detected: seq=%u, elapsed=%ld, rto=%ld",
                     unacked->seq_num, elapsed, rto);

            timeout_count++;

            // 标记为需要重传
            unacked->retry_count++;
            unacked->is_retransmitted = true;
        }
    }

    return timeout_count;
}

/**
 * 重传指定序列号的数据包
 * 
 * 更新发送时间和重传标志
 */
bool retransmit_packet(SendWindow* window, uint32_t seq_num)
{
    if (window == NULL) {
        return false;
    }

    UnackedPacket* unacked = get_unacked_packet(window, seq_num);
    if (unacked == NULL) {
        LOG_WARN("Packet not found for retransmission: seq=%u", seq_num);
        return false;
    }

    // 更新重传信息
    unacked->send_time = time(NULL);
    unacked->retry_count++;
    unacked->is_retransmitted = true;

    LOG_INFO("Retransmitting packet: seq=%u, retry=%d", seq_num, unacked->retry_count);

    // 这里应该实际发送数据包（实现在UDP层）

    return true;
}

/**
 * 检查并应用超时退避
 * 
 * 实现exponential backoff机制
 * 每次超时时，将RTO加倍，直到达到最大值
 * 
 * @return 返回新的RTO值
 */
time_t apply_timeout_backoff(SendWindow* window, time_t rto)
{
    if (window == NULL || rto <= 0) {
        return TIMEOUT_MS / 1000;  // 返回默认值
    }

    // Exponential backoff: RTO *= 2，最大不超过 60秒
    time_t new_rto = rto * 2;
    if (new_rto > 60) {
        new_rto = 60;
    }

    LOG_DEBUG("Applying timeout backoff: old_rto=%ld, new_rto=%ld", rto, new_rto);

    return new_rto;
}
