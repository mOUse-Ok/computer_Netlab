#include "packet.h"
#include "checksum.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ==================== 帧创建函数 ====================

/**
 * 创建一个新的帧
 * 初始化帧的所有字段，并计算校验和
 */
Frame create_frame(uint32_t seq_num, uint32_t ack_num, uint16_t window_size,
                   FrameType frame_type, const uint8_t* data, uint16_t data_len)
{
    Frame frame;

    // 初始化帧头部
    frame.seq_num = seq_num;
    frame.ack_num = ack_num;
    frame.window_size = window_size;
    frame.frame_type = (uint8_t)frame_type;
    frame.data_len = data_len;
    frame.checksum = 0;

    // 初始化数据部分
    memset(frame.data, 0, MAX_DATA_LENGTH);

    // 复制数据（如果提供）
    if (data != NULL && data_len > 0) {
        if (data_len > MAX_DATA_LENGTH) {
            data_len = MAX_DATA_LENGTH;
        }
        memcpy(frame.data, data, data_len);
        frame.data_len = data_len;
    }

    // 计算校验和
    frame.checksum = frame_calculate_checksum(&frame);

    return frame;
}

// ==================== 校验和计算 ====================

/**
 * 计算帧的校验和
 * 使用简单的字节和校验（取模256）
 */
uint8_t frame_calculate_checksum(const Frame* frame)
{
    if (frame == NULL) {
        return 0;
    }

    uint32_t sum = 0;

    // 累加所有字节（包括帧头部和数据）
    // 注意：不包括checksum字段本身
    
    // 帧头部（除checksum外）
    uint8_t* ptr = (uint8_t*)frame;
    
    // seq_num (4字节)
    sum += ptr[0] + ptr[1] + ptr[2] + ptr[3];
    
    // ack_num (4字节)
    sum += ptr[4] + ptr[5] + ptr[6] + ptr[7];
    
    // window_size (2字节)
    sum += ptr[8] + ptr[9];
    
    // frame_type (1字节)
    sum += ptr[10];
    
    // data_len (2字节)
    sum += ptr[11] + ptr[12];
    
    // 数据部分
    for (uint16_t i = 0; i < frame->data_len; i++) {
        sum += frame->data[i];
    }

    // 取模256得到校验和
    return (uint8_t)(sum % 256);
}

/**
 * 验证帧的校验和
 */
int frame_verify_checksum(const Frame* frame)
{
    if (frame == NULL) {
        return 0;
    }

    uint8_t calculated = frame_calculate_checksum(frame);
    return (calculated == frame->checksum) ? 1 : 0;
}

// ==================== 字节序转换辅助函数 ====================

/**
 * 主机字节序转网络字节序（32位）
 */
static inline uint32_t htonl_custom(uint32_t value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

/**
 * 网络字节序转主机字节序（32位）
 */
static inline uint32_t ntohl_custom(uint32_t value)
{
    return htonl_custom(value);  // 转换是对称的
}

/**
 * 主机字节序转网络字节序（16位）
 */
static inline uint16_t htons_custom(uint16_t value)
{
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

/**
 * 网络字节序转主机字节序（16位）
 */
static inline uint16_t ntohs_custom(uint16_t value)
{
    return htons_custom(value);  // 转换是对称的
}

// ==================== 帧序列化 ====================

/**
 * 将帧序列化为字节流
 * 
 * 序列化格式（网络字节序，大端）：
 * [0-3]   : seq_num (32位)
 * [4-7]   : ack_num (32位)
 * [8-9]   : window_size (16位)
 * [10]    : frame_type (8位)
 * [11-12] : data_len (16位)
 * [13]    : checksum (8位)
 * [14+]   : data (可变长度)
 */
int frame_serialize(const Frame* frame, uint8_t* buffer, size_t buf_size)
{
    if (frame == NULL || buffer == NULL) {
        return -1;
    }

    // 计算需要的缓冲区大小
    size_t needed = FRAME_HEADER_SIZE + frame->data_len;

    if (buf_size < needed) {
        return -1;
    }

    // 序列化seq_num (网络字节序)
    uint32_t seq_net = htonl_custom(frame->seq_num);
    buffer[0] = (seq_net >> 24) & 0xFF;
    buffer[1] = (seq_net >> 16) & 0xFF;
    buffer[2] = (seq_net >> 8) & 0xFF;
    buffer[3] = seq_net & 0xFF;

    // 序列化ack_num (网络字节序)
    uint32_t ack_net = htonl_custom(frame->ack_num);
    buffer[4] = (ack_net >> 24) & 0xFF;
    buffer[5] = (ack_net >> 16) & 0xFF;
    buffer[6] = (ack_net >> 8) & 0xFF;
    buffer[7] = ack_net & 0xFF;

    // 序列化window_size (网络字节序)
    uint16_t window_net = htons_custom(frame->window_size);
    buffer[8] = (window_net >> 8) & 0xFF;
    buffer[9] = window_net & 0xFF;

    // 序列化frame_type
    buffer[10] = frame->frame_type;

    // 序列化data_len (网络字节序)
    uint16_t data_len_net = htons_custom(frame->data_len);
    buffer[11] = (data_len_net >> 8) & 0xFF;
    buffer[12] = data_len_net & 0xFF;

    // 序列化checksum
    buffer[13] = frame->checksum;

    // 复制数据部分
    if (frame->data_len > 0) {
        memcpy(buffer + FRAME_HEADER_SIZE, frame->data, frame->data_len);
    }

    return (int)needed;
}

// ==================== 帧反序列化 ====================

/**
 * 将字节流反序列化为帧
 */
int frame_deserialize(const uint8_t* buffer, size_t buf_size, Frame* frame)
{
    if (buffer == NULL || frame == NULL) {
        return -1;
    }

    // 检查最小帧大小
    if (buf_size < FRAME_HEADER_SIZE) {
        return -1;
    }

    // 反序列化seq_num (从网络字节序转换)
    uint32_t seq_net = ((uint32_t)buffer[0] << 24) |
                       ((uint32_t)buffer[1] << 16) |
                       ((uint32_t)buffer[2] << 8) |
                       (uint32_t)buffer[3];
    frame->seq_num = ntohl_custom(seq_net);

    // 反序列化ack_num (从网络字节序转换)
    uint32_t ack_net = ((uint32_t)buffer[4] << 24) |
                       ((uint32_t)buffer[5] << 16) |
                       ((uint32_t)buffer[6] << 8) |
                       (uint32_t)buffer[7];
    frame->ack_num = ntohl_custom(ack_net);

    // 反序列化window_size (从网络字节序转换)
    uint16_t window_net = ((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9];
    frame->window_size = ntohs_custom(window_net);

    // 反序列化frame_type
    frame->frame_type = buffer[10];

    // 反序列化data_len (从网络字节序转换)
    uint16_t data_len_net = ((uint16_t)buffer[11] << 8) | (uint16_t)buffer[12];
    frame->data_len = ntohs_custom(data_len_net);

    // 反序列化checksum
    frame->checksum = buffer[13];

    // 校验数据长度
    if (frame->data_len > MAX_DATA_LENGTH) {
        return -1;
    }

    // 检查缓冲区大小是否足够
    if (buf_size < (size_t)(FRAME_HEADER_SIZE + frame->data_len)) {
        return -1;
    }

    // 初始化数据缓冲区
    memset(frame->data, 0, MAX_DATA_LENGTH);

    // 复制数据部分
    if (frame->data_len > 0) {
        memcpy(frame->data, buffer + FRAME_HEADER_SIZE, frame->data_len);
    }

    return 0;
}

// ==================== 帧类型转字符串 ====================

/**
 * 获取帧类型的字符串表示
 */
const char* frame_type_to_string(FrameType frame_type)
{
    switch (frame_type) {
        case SYN:
            return "SYN";
        case SYN_ACK:
            return "SYN_ACK";
        case ACK:
            return "ACK";
        case FIN:
            return "FIN";
        case FIN_ACK:
            return "FIN_ACK";
        case DATA:
            return "DATA";
        default:
            return "UNKNOWN";
    }
}

// ==================== 调试输出函数 ====================

/**
 * 打印帧的详细信息（调试用）
 */
void frame_print(const Frame* frame)
{
    if (frame == NULL) {
        printf("Error: frame pointer is NULL\n");
        return;
    }

    printf("========== Frame Information ==========\n");
    printf("Frame Type:      %s (0x%02X)\n", frame_type_to_string((FrameType)frame->frame_type), frame->frame_type);
    printf("Sequence Number: %u (0x%08X)\n", frame->seq_num, frame->seq_num);
    printf("Ack Number:      %u (0x%08X)\n", frame->ack_num, frame->ack_num);
    printf("Window Size:     %u\n", frame->window_size);
    printf("Data Length:     %u bytes\n", frame->data_len);
    printf("Checksum:        0x%02X\n", frame->checksum);
    printf("Frame Size:      %u bytes (header: %d, data: %u)\n", 
           FRAME_HEADER_SIZE + frame->data_len, FRAME_HEADER_SIZE, frame->data_len);
    
    if (frame->data_len > 0 && frame->data_len <= 64) {
        printf("Data (hex):      ");
        for (uint16_t i = 0; i < frame->data_len; i++) {
            printf("%02X ", frame->data[i]);
        }
        printf("\n");
    }

    printf("========================================\n");
}

/**
 * 打印帧的十六进制表示（调试用）
 */
void frame_print_hex(const uint8_t* buffer, size_t len)
{
    if (buffer == NULL || len == 0) {
        printf("Error: buffer is NULL or empty\n");
        return;
    }

    printf("========== Frame Hex Dump ==========\n");
    printf("Offset   : Hex Data\n");
    printf("-" "---" "--" "---" "--" "---" "--" "---" "-\n");

    for (size_t i = 0; i < len; i += 16) {
        printf("%08zX : ", i);

        // 打印十六进制
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            printf("%02X ", buffer[i + j]);
        }

        printf("| ");

        // 打印ASCII
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            char c = (buffer[i + j] >= 32 && buffer[i + j] < 127) ? buffer[i + j] : '.';
            printf("%c", c);
        }

        printf("\n");
    }

    printf("====================================\n");
}
