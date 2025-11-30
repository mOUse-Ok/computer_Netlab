// packet.cpp - 数据包处理模块源文件
// 实现数据包处理类的方法和数据包处理工具函数

#include "packet.h"
#include "checksum.h" // 后面会创建的校验和模块
#include <iostream>
#include <cstring>

// 构造函数
PacketHandler::PacketHandler() {
    // 初始化代码（如果需要）
}

// 析构函数
PacketHandler::~PacketHandler() {
    // 清理代码（如果需要）
}

// 创建数据数据包
Packet PacketHandler::create_data_packet(uint32_t seq_num, uint32_t ack_num, 
                                       const char* data, size_t data_length) {
    Packet packet;
    
    // 设置头部信息
    packet.header.seq_num = seq_num;
    packet.header.ack_num = ack_num;
    packet.header.type = DATA_PACKET;
    packet.header.flags = 0; // 初始化为0
    packet.header.window_size = WINDOW_SIZE;
    packet.header.data_length = static_cast<uint16_t>(data_length);
    
    // 复制数据（确保不超过最大长度）
    size_t actual_length = (data_length > MAX_DATA_LENGTH) ? MAX_DATA_LENGTH : data_length;
    if (data != nullptr && data_length > 0) {
        memcpy(packet.data, data, actual_length);
        packet.header.data_length = static_cast<uint16_t>(actual_length);
    } else {
        packet.header.data_length = 0;
    }
    
    // 计算校验和
    packet.header.checksum = 0; // 先清零
    packet.header.checksum = calculate_checksum(packet);
    
    return packet;
}

// 创建确认数据包
Packet PacketHandler::create_ack_packet(uint32_t seq_num, uint32_t ack_num) {
    Packet packet;
    
    // 设置头部信息
    packet.header.seq_num = seq_num;
    packet.header.ack_num = ack_num;
    packet.header.type = ACK_PACKET;
    packet.header.flags = 0; // 初始化为0
    packet.header.window_size = WINDOW_SIZE;
    packet.header.data_length = 0; // ACK数据包没有数据部分
    
    // 计算校验和
    packet.header.checksum = 0; // 先清零
    packet.header.checksum = calculate_checksum(packet);
    
    return packet;
}

// 创建同步数据包
Packet PacketHandler::create_syn_packet(uint32_t seq_num) {
    Packet packet;
    
    // 设置头部信息
    packet.header.seq_num = seq_num;
    packet.header.ack_num = 0;
    packet.header.type = SYN_PACKET;
    packet.header.flags = 1; // SYN标志位
    packet.header.window_size = WINDOW_SIZE;
    packet.header.data_length = 0; // SYN数据包没有数据部分
    
    // 计算校验和
    packet.header.checksum = 0; // 先清零
    packet.header.checksum = calculate_checksum(packet);
    
    return packet;
}

// 创建结束数据包
Packet PacketHandler::create_fin_packet(uint32_t seq_num, uint32_t ack_num) {
    Packet packet;
    
    // 设置头部信息
    packet.header.seq_num = seq_num;
    packet.header.ack_num = ack_num;
    packet.header.type = FIN_PACKET;
    packet.header.flags = 2; // FIN标志位
    packet.header.window_size = WINDOW_SIZE;
    packet.header.data_length = 0; // FIN数据包没有数据部分
    
    // 计算校验和
    packet.header.checksum = 0; // 先清零
    packet.header.checksum = calculate_checksum(packet);
    
    return packet;
}

// 序列化数据包
size_t PacketHandler::serialize_packet(const Packet& packet, char* buffer, size_t buffer_size) {
    // 计算数据包总大小
    size_t packet_size = sizeof(PacketHeader) + packet.header.data_length;
    
    // 检查缓冲区是否足够大
    if (buffer_size < packet_size || buffer == nullptr) {
        return 0;
    }
    
    // 复制头部
    memcpy(buffer, &packet.header, sizeof(PacketHeader));
    
    // 复制数据（如果有）
    if (packet.header.data_length > 0) {
        memcpy(buffer + sizeof(PacketHeader), packet.data, packet.header.data_length);
    }
    
    return packet_size;
}

// 反序列化数据包
bool PacketHandler::deserialize_packet(const char* buffer, size_t buffer_size, Packet& packet) {
    // 检查缓冲区大小是否至少包含头部
    if (buffer_size < sizeof(PacketHeader) || buffer == nullptr) {
        return false;
    }
    
    // 复制头部
    memcpy(&packet.header, buffer, sizeof(PacketHeader));
    
    // 检查数据长度是否合法
    if (packet.header.data_length > MAX_DATA_LENGTH) {
        return false;
    }
    
    // 检查缓冲区是否足够大以包含整个数据包
    if (buffer_size < sizeof(PacketHeader) + packet.header.data_length) {
        return false;
    }
    
    // 复制数据（如果有）
    if (packet.header.data_length > 0) {
        memcpy(packet.data, buffer + sizeof(PacketHeader), packet.header.data_length);
    }
    
    return true;
}

// 验证数据包
bool PacketHandler::validate_packet(const Packet& packet) {
    // 检查数据包类型是否合法
    if (packet.header.type < DATA_PACKET || packet.header.type > FIN_PACKET) {
        return false;
    }
    
    // 检查数据长度是否合法
    if (packet.header.data_length > MAX_DATA_LENGTH) {
        return false;
    }
    
    // 验证校验和
    return verify_checksum(packet);
}

// 验证校验和
bool PacketHandler::verify_checksum(const Packet& packet) {
    uint16_t original_checksum = packet.header.checksum;
    
    // 保存当前校验和并清零
    packet.header.checksum = 0;
    
    // 计算新的校验和
    uint16_t calculated_checksum = calculate_checksum(packet);
    
    // 恢复原始校验和
    packet.header.checksum = original_checksum;
    
    // 比较校验和
    return (original_checksum == calculated_checksum);
}

// 计算校验和（简化版，后续会在checksum模块中实现更完整的版本）
uint16_t PacketHandler::calculate_checksum(const Packet& packet) {
    // 这里只是一个占位符实现
    // 后续会使用checksum模块中的函数来计算校验和
    uint32_t sum = 0;
    
    // 计算头部校验和
    const uint16_t* header_ptr = reinterpret_cast<const uint16_t*>(&packet.header);
    for (size_t i = 0; i < sizeof(PacketHeader) / 2; ++i) {
        sum += *header_ptr++;
    }
    
    // 计算数据部分校验和（如果有）
    if (packet.header.data_length > 0) {
        const uint16_t* data_ptr = reinterpret_cast<const uint16_t*>(packet.data);
        size_t data_words = packet.header.data_length / 2;
        
        for (size_t i = 0; i < data_words; ++i) {
            sum += *data_ptr++;
        }
        
        // 处理奇数字节
        if (packet.header.data_length % 2 != 0) {
            uint16_t last_byte = static_cast<uint16_t>(packet.data[packet.header.data_length - 1]) << 8;
            sum += last_byte;
        }
    }
    
    // 折叠校验和
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<uint16_t>(~sum);
}

// 获取序列号
uint32_t PacketHandler::get_seq_num(const Packet& packet) const {
    return packet.header.seq_num;
}

// 获取确认号
uint32_t PacketHandler::get_ack_num(const Packet& packet) const {
    return packet.header.ack_num;
}

// 获取数据包类型
PacketType PacketHandler::get_packet_type(const Packet& packet) const {
    return static_cast<PacketType>(packet.header.type);
}

// 获取数据长度
size_t PacketHandler::get_data_length(const Packet& packet) const {
    return packet.header.data_length;
}

// 获取数据
const char* PacketHandler::get_data(const Packet& packet) const {
    return packet.data;
}

// 获取窗口大小
uint16_t PacketHandler::get_window_size(const Packet& packet) const {
    return packet.header.window_size;
}

// 设置窗口大小
void PacketHandler::set_window_size(Packet& packet, uint16_t window_size) {
    packet.header.window_size = window_size;
    
    // 更新校验和
    packet.header.checksum = 0; // 先清零
    packet.header.checksum = calculate_checksum(packet);
}

// 打印数据包信息
void print_packet_info(const Packet& packet) {
    std::cout << "数据包信息:" << std::endl;
    std::cout << "  类型: ";
    switch (packet.header.type) {
        case DATA_PACKET: std::cout << "DATA";
            break;
        case ACK_PACKET: std::cout << "ACK";
            break;
        case SYN_PACKET: std::cout << "SYN";
            break;
        case FIN_PACKET: std::cout << "FIN";
            break;
        default: std::cout << "未知";
            break;
    }
    std::cout << std::endl;
    std::cout << "  序列号: " << packet.header.seq_num << std::endl;
    std::cout << "  确认号: " << packet.header.ack_num << std::endl;
    std::cout << "  窗口大小: " << packet.header.window_size << std::endl;
    std::cout << "  数据长度: " << packet.header.data_length << std::endl;
    std::cout << "  校验和: 0x" << std::hex << packet.header.checksum << std::dec << std::endl;
}

// 检查数据包是否在窗口内
bool is_packet_in_window(uint32_t seq_num, uint32_t window_start, uint32_t window_end, uint16_t window_size) {
    // 考虑回绕的情况
    if (window_start <= window_end) {
        return (seq_num >= window_start) && (seq_num <= window_end);
    } else {
        // 窗口已经回绕
        return (seq_num >= window_start) || (seq_num <= window_end);
    }
}