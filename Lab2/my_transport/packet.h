// packet.h - 数据包处理模块头文件
// 定义数据包结构和数据包处理相关函数

#ifndef PACKET_H
#define PACKET_H

#include "reliable_transport.h"
#include <cstdint>
#include <cstring>

// 数据包头部结构
struct PacketHeader {
    uint32_t seq_num;      // 序列号
    uint32_t ack_num;      // 确认号
    uint16_t checksum;     // 校验和
    uint8_t type;          // 数据包类型（见PacketType枚举）
    uint8_t flags;         // 标志位
    uint16_t window_size;  // 窗口大小
    uint16_t data_length;  // 数据长度
};

// 完整数据包结构
struct Packet {
    PacketHeader header;   // 数据包头部
    char data[MAX_DATA_LENGTH]; // 数据部分
};

// 数据包处理类
class PacketHandler {
private:
    // 计算校验和的辅助函数
    uint16_t calculate_checksum(const Packet& packet);

public:
    // 构造函数和析构函数
    PacketHandler();
    ~PacketHandler();

    // 数据包创建函数
    Packet create_data_packet(uint32_t seq_num, uint32_t ack_num, 
                           const char* data, size_t data_length);
    Packet create_ack_packet(uint32_t seq_num, uint32_t ack_num);
    Packet create_syn_packet(uint32_t seq_num);
    Packet create_fin_packet(uint32_t seq_num, uint32_t ack_num);

    // 数据包序列化和反序列化
    size_t serialize_packet(const Packet& packet, char* buffer, size_t buffer_size);
    bool deserialize_packet(const char* buffer, size_t buffer_size, Packet& packet);

    // 数据包验证函数
    bool validate_packet(const Packet& packet);
    bool verify_checksum(const Packet& packet);

    // 数据包信息获取函数
    uint32_t get_seq_num(const Packet& packet) const;
    uint32_t get_ack_num(const Packet& packet) const;
    PacketType get_packet_type(const Packet& packet) const;
    size_t get_data_length(const Packet& packet) const;
    const char* get_data(const Packet& packet) const;

    // 数据包窗口大小函数
    uint16_t get_window_size(const Packet& packet) const;
    void set_window_size(Packet& packet, uint16_t window_size);
};

// 工具函数
extern void print_packet_info(const Packet& packet);
extern bool is_packet_in_window(uint32_t seq_num, uint32_t window_start, uint32_t window_end, uint16_t window_size);

#endif // PACKET_H