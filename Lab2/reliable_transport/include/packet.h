#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <cstring>
#include <cstddef>
#include <cstdbool>
#include "reliable_transport.h"

// ==================== 帧类型定义 ====================

/**
 * 帧类型枚举
 * - SYN: 同步帧，用于建立连接
 * - SYN_ACK: 同步确认帧，服务器响应SYN
 * - ACK: 确认帧，确认接收到的数据
 * - FIN: 结束帧，请求关闭连接
 * - FIN_ACK: 结束确认帧，响应FIN请求
 * - DATA: 数据帧，传输实际数据
 */
typedef enum {
    SYN = 0,           // 同步帧
    SYN_ACK = 1,       // 同步-确认帧
    ACK = 2,           // 确认帧
    FIN = 3,           // 结束帧
    FIN_ACK = 4,       // 结束-确认帧
    DATA = 5           // 数据帧
} FrameType;

// ==================== 帧结构定义 ====================

/**
 * 可靠传输协议帧结构
 * 
 * 帧头部（固定14字节）：
 *   序列号(4字节) | 确认号(4字节) | 窗口大小(2字节) | 
 *   帧类型(1字节) | 数据长度(2字节) | 校验和(1字节)
 * 
 * 数据部分（可变长度，最大MAX_DATA_LENGTH字节）
 * 
 * 总大小：帧头部(14字节) + 数据长度，不超过MAX_PACKET_SIZE
 */
typedef struct {
    // === 帧头部（14字节）===
    uint32_t seq_num;                  // [0-3] 序列号：用于标识帧的顺序，32位无符号整数
    uint32_t ack_num;                  // [4-7] 确认号：确认已收到的数据序列号，32位无符号整数
    uint16_t window_size;              // [8-9] 窗口大小：发送方的接收窗口大小，16位无符号整数
    uint8_t frame_type;                // [10]  帧类型：FrameType枚举值，1字节
    uint16_t data_len;                 // [11-12] 数据长度：实际数据的长度，16位无符号整数
    uint8_t checksum;                  // [13]  校验和：对帧头部和数据的校验，1字节
    
    // === 数据部分（可变长度）===
    uint8_t data[MAX_DATA_LENGTH];     // 数据缓冲区：存储实际的传输数据，最大1000字节
} Frame;

// ==================== 帧头部常量 ====================

#define FRAME_HEADER_SIZE 14           // 帧头部大小（字节）
#define FRAME_MAX_SIZE (FRAME_HEADER_SIZE + MAX_DATA_LENGTH)  // 帧的最大大小

// ==================== 帧处理函数 ====================

/**
 * 创建一个新的帧
 * @param seq_num 序列号
 * @param ack_num 确认号
 * @param window_size 窗口大小
 * @param frame_type 帧类型
 * @param data 数据指针（可为NULL）
 * @param data_len 数据长度
 * @return 返回创建的帧结构体，需要配置后使用
 */
Frame create_frame(uint32_t seq_num, uint32_t ack_num, uint16_t window_size,
                   FrameType frame_type, const uint8_t* data, uint16_t data_len);

/**
 * 将帧序列化为网络字节序的字节缓冲区
 * 将Frame结构体转换为可传输的字节流，包含字节序转换
 * @param frame 帧指针
 * @param buffer 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 成功返回序列化的字节数，失败返回-1
 */
int frame_serialize(const Frame* frame, uint8_t* buffer, size_t buf_size);

/**
 * 将字节缓冲区反序列化为帧结构
 * 将网络字节序的字节流转换为Frame结构体，包含字节序转换
 * @param buffer 输入缓冲区
 * @param buf_size 缓冲区大小
 * @param frame 输出帧结构指针
 * @return 成功返回0，失败返回-1
 */
int frame_deserialize(const uint8_t* buffer, size_t buf_size, Frame* frame);

/**
 * 计算帧的校验和
 * 计算包括帧头部和数据部分的校验和
 * @param frame 帧指针
 * @return 计算得到的校验和值
 */
uint8_t frame_calculate_checksum(const Frame* frame);

/**
 * 验证帧的校验和
 * @param frame 帧指针
 * @return 校验通过返回1，失败返回0
 */
int frame_verify_checksum(const Frame* frame);

/**
 * 打印帧的详细信息（调试用）
 * @param frame 帧指针
 */
void frame_print(const Frame* frame);

/**
 * 打印帧的十六进制表示（调试用）
 * @param buffer 帧缓冲区
 * @param len 缓冲区长度
 */
void frame_print_hex(const uint8_t* buffer, size_t len);

/**
 * 获取帧类型的字符串表示
 * @param frame_type 帧类型
 * @return 帧类型的字符串
 */
const char* frame_type_to_string(FrameType frame_type);

#endif // PACKET_H
