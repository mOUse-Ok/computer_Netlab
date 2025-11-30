#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>

// 协议常量定义
const int PACKET_SIZE = 1024;           // 总包大小
// header layout: seq(2) + ack(2) + flags(1) + reserved(1) + checksum(2) + wnd_size(2) = 10 bytes
const int HEADER_SIZE = 10;             // 协议头大小 (字节)
const int PAYLOAD_SIZE = PACKET_SIZE - HEADER_SIZE;  // 负载大小
const int WINDOW_SIZE = 4;              // 发送/接收窗口大小
const int TIMEOUT_MS = 1000;            // 超时时间(毫秒)

// 控制标志位
enum PacketFlag {
    FLAG_SYN = 0x01,     // 同步(建立连接)
    FLAG_ACK = 0x02,     // 确认
    FLAG_FIN = 0x04,     // 终止连接
    FLAG_DAT = 0x08      // 数据包
};

// 数据包结构
#pragma pack(1)
struct Packet {
    uint16_t seq;         // 序列号
    uint16_t ack;         // 确认号
    uint8_t flags;        // 控制标志
    uint8_t reserved;     // 保留字段
    uint16_t checksum;    // 校验和
    uint16_t wnd_size;    // 窗口大小
    char payload[PAYLOAD_SIZE];  // 负载数据
};
#pragma pack()

// 连接状态
enum ConnectionState {
    CLOSED = 0,
    SYN_SENT = 1,
    SYN_RECV = 2,
    ESTABLISHED = 3,
    FIN_SENT = 4,
    FIN_RECV = 5
};

// 拥塞控制状态
enum CongestionState {
    SLOW_START = 0,
    CONGESTION_AVOIDANCE = 1,
    FAST_RECOVERY = 2
};

// 计算校验和
inline uint16_t calculateChecksum(const Packet& pkt) {
    uint16_t checksum = 0;
    const uint8_t* data = (const uint8_t*)&pkt;
    
    // 校验和不包括 checksum 字段本身 (offset 6-7)
    for (int i = 0; i < HEADER_SIZE; i += 2) {
        if (i == 6) continue;  // 跳过 checksum 字段 (字节6-7)
        uint16_t word = ((uint16_t)data[i] << 8) | (uint16_t)data[i + 1];
        checksum += word;
        if (checksum > 0xFFFF) {
            checksum = (checksum & 0xFFFF) + (checksum >> 16);
        }
    }
    
    // 加上payload（按无符号字节）
    for (int i = 0; i < PAYLOAD_SIZE; i += 2) {
        if (i + 1 < PAYLOAD_SIZE) {
            uint16_t high = (uint8_t)pkt.payload[i];
            uint16_t low = (uint8_t)pkt.payload[i + 1];
            uint16_t word = (high << 8) | low;
            checksum += word;
            if (checksum > 0xFFFF) {
                checksum = (checksum & 0xFFFF) + (checksum >> 16);
            }
        }
    }
    
    return ~checksum;
}

// 验证校验和
inline bool verifyChecksum(const Packet& pkt) {
    uint16_t orig_checksum = pkt.checksum;
    Packet temp = pkt;
    temp.checksum = 0;
    return calculateChecksum(temp) == orig_checksum;
}

#endif // PROTOCOL_H
