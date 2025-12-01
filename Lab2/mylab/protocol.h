#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <ctime>

// ===== 连接状态枚举 =====
enum ConnectionState {
    CLOSED,        // 关闭状态
    SYN_SENT,      // 客户端：已发送SYN，等待SYN+ACK
    SYN_RCVD,      // 服务端：已接收SYN，已发送SYN+ACK，等待ACK
    ESTABLISHED,   // 连接已建立
    FIN_WAIT_1,    // 主动关闭方：已发送FIN，等待ACK
    FIN_WAIT_2,    // 主动关闭方：已收到FIN的ACK，等待对方FIN
    TIME_WAIT,     // 主动关闭方：已发送最后的ACK，等待2MSL
    CLOSE_WAIT,    // 被动关闭方：已收到FIN，已发送ACK，等待本地关闭
    LAST_ACK       // 被动关闭方：已发送FIN，等待最后的ACK
};

// ===== 标志位定义 =====
#define FLAG_SYN 0x01  // 同步标志，用于建立连接
#define FLAG_ACK 0x02  // 确认标志，用于确认收到数据
#define FLAG_FIN 0x04  // 结束标志，用于关闭连接

// ===== 协议常量 =====
#define HEADER_SIZE 12            // 协议头大小：seq(4) + ack(4) + flag(1) + win(2) + checksum(1) = 12字节
#define MAX_PACKET_SIZE 1024      // 最大数据包大小
#define MAX_DATA_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)  // 最大数据负载大小
#define WINDOW_SIZE 4096          // 窗口大小（用于流量控制）
#define TIMEOUT_MS 3000           // 超时时间：3秒
#define MAX_RETRIES 3             // 最大重传次数
#define TIME_WAIT_MS 4000         // TIME_WAIT状态等待时间（2*MSL，这里设为4秒）
#define HEARTBEAT_INTERVAL_MS 1000  // 心跳间隔：1秒
#define CONNECTION_TIMEOUT_MS 5000  // 连接超时：5秒无响应则认为断开

// ===== UDP协议头结构体 =====
// 注意：使用#pragma pack确保结构体内存对齐，避免填充字节
#pragma pack(push, 1)
struct UDPHeader {
    uint32_t seq;      // 序列号：发送数据的起始序列号
    uint32_t ack;      // 确认号：期望接收的下一个序列号
    uint8_t  flag;     // 标志位：SYN/ACK/FIN
    uint16_t win;      // 窗口大小：用于流量控制
    uint8_t  checksum; // 校验和：简单的8位校验和（可选，初步版本可设为0）
    
    // 构造函数：初始化协议头
    UDPHeader() : seq(0), ack(0), flag(0), win(WINDOW_SIZE), checksum(0) {}
    
    // 带参数的构造函数
    UDPHeader(uint32_t s, uint32_t a, uint8_t f) 
        : seq(s), ack(a), flag(f), win(WINDOW_SIZE), checksum(0) {}
    
    // 计算简单的校验和（所有字节异或）
    void calculateChecksum(const char* data, int dataLen) {
        checksum = 0;
        // 对协议头进行校验
        const uint8_t* ptr = (const uint8_t*)this;
        for (int i = 0; i < HEADER_SIZE - 1; i++) {  // 不包括checksum字段本身
            checksum ^= ptr[i];
        }
        // 对数据部分进行校验
        for (int i = 0; i < dataLen; i++) {
            checksum ^= (uint8_t)data[i];
        }
    }
    
    // 验证校验和
    bool verifyChecksum(const char* data, int dataLen) const {
        uint8_t calc = 0;
        const uint8_t* ptr = (const uint8_t*)this;
        for (int i = 0; i < HEADER_SIZE - 1; i++) {
            calc ^= ptr[i];
        }
        for (int i = 0; i < dataLen; i++) {
            calc ^= (uint8_t)data[i];
        }
        return calc == checksum;
    }
};
#pragma pack(pop)

// ===== 数据包结构体 =====
struct Packet {
    UDPHeader header;                // 协议头
    char data[MAX_DATA_SIZE];        // 数据负载
    int dataLen;                     // 实际数据长度
    
    // 构造函数
    Packet() : dataLen(0) {
        memset(data, 0, MAX_DATA_SIZE);
    }
    
    // 设置数据
    void setData(const char* buf, int len) {
        dataLen = (len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : len;
        memcpy(data, buf, dataLen);
        header.calculateChecksum(data, dataLen);
    }
    
    // 获取总长度
    int getTotalLen() const {
        return HEADER_SIZE + dataLen;
    }
    
    // 序列化为字节流（用于发送）
    void serialize(char* buffer) const {
        memcpy(buffer, &header, HEADER_SIZE);
        memcpy(buffer + HEADER_SIZE, data, dataLen);
    }
    
    // 从字节流反序列化（用于接收）
    bool deserialize(const char* buffer, int bufLen) {
        if (bufLen < HEADER_SIZE) {
            return false;
        }
        memcpy(&header, buffer, HEADER_SIZE);
        dataLen = bufLen - HEADER_SIZE;
        if (dataLen > 0) {
            memcpy(data, buffer + HEADER_SIZE, dataLen);
        }
        // 验证校验和
        return header.verifyChecksum(data, dataLen);
    }
};

// ===== 辅助函数 =====
// 生成随机初始序列号
inline uint32_t generateInitialSeq() {
    srand((unsigned int)time(NULL));
    return (uint32_t)rand();
}

// 获取当前时间戳（毫秒）
inline long long getCurrentTimeMs() {
    return (long long)time(NULL) * 1000;
}

// 获取状态名称（用于调试输出）
inline const char* getStateName(ConnectionState state) {
    switch (state) {
        case CLOSED:       return "CLOSED";
        case SYN_SENT:     return "SYN_SENT";
        case SYN_RCVD:     return "SYN_RCVD";
        case ESTABLISHED:  return "ESTABLISHED";
        case FIN_WAIT_1:   return "FIN_WAIT_1";
        case FIN_WAIT_2:   return "FIN_WAIT_2";
        case TIME_WAIT:    return "TIME_WAIT";
        case CLOSE_WAIT:   return "CLOSE_WAIT";
        case LAST_ACK:     return "LAST_ACK";
        default:           return "UNKNOWN";
    }
}

#endif // PROTOCOL_H
