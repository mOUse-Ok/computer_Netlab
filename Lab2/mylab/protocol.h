#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <ctime>
#include <cstring>

// ===== 连接状态枚举 =====
// 描述：用于跟踪连接的当前状态，模拟TCP状态机
enum ConnectionState {
    CLOSED,        // 关闭状态：初始状态，无连接
    SYN_SENT,      // 客户端：已发送SYN，等待SYN+ACK
    SYN_RCVD,      // 服务端：已接收SYN，已发送SYN+ACK，等待ACK
    ESTABLISHED,   // 连接已建立：可以进行数据传输
    FIN_WAIT_1,    // 主动关闭方：已发送FIN，等待ACK
    FIN_WAIT_2,    // 主动关闭方：已收到FIN的ACK，等待对方FIN
    TIME_WAIT,     // 主动关闭方：已发送最后的ACK，等待2MSL
    CLOSE_WAIT,    // 被动关闭方：已收到FIN，已发送ACK，等待本地关闭
    LAST_ACK       // 被动关闭方：已发送FIN，等待最后的ACK
};

// ===== 标志位定义 =====
// 描述：用于标识数据包类型，可以组合使用（如 FLAG_SYN | FLAG_ACK）
#define FLAG_SYN  0x01  // 同步标志（0000 0001）：用于建立连接
#define FLAG_ACK  0x02  // 确认标志（0000 0010）：用于确认收到数据
#define FLAG_FIN  0x04  // 结束标志（0000 0100）：用于关闭连接
#define FLAG_SACK 0x08  // 选择确认标志（0000 1000）：用于后续选择确认功能

// ===== 协议常量 =====
// 协议头大小：seq(4) + ack(4) + flag(1) + win(2) + checksum(2) + len(2) + 保留(5) = 20字节
#define HEADER_SIZE 20            // 协议头大小：20字节（便于对齐）
#define MAX_PACKET_SIZE 1024      // 最大数据包大小（字节）
#define MAX_DATA_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)  // 最大数据负载大小
#define DEFAULT_WINDOW_SIZE 4     // 默认窗口大小（单位：数据包数量，后续可配置）
#define TIMEOUT_MS 3000           // 超时时间：3秒
#define MAX_RETRIES 3             // 最大重传次数
#define TIME_WAIT_MS 4000         // TIME_WAIT状态等待时间（2*MSL，这里设为4秒）
#define HEARTBEAT_INTERVAL_MS 1000  // 心跳间隔：1秒
#define CONNECTION_TIMEOUT_MS 5000  // 连接超时：5秒无响应则认为断开

// ===== RFC 1071 校验和计算函数 =====
// 描述：采用 RFC 1071 标准计算校验和，适用于网络数据校验
// 参数：
//   data - 指向要计算校验和的数据的指针
//   len  - 数据长度（字节数）
// 返回值：16位校验和（1的补码）
// 算法说明：
//   1. 将数据按16位（2字节）分组进行累加
//   2. 如果数据长度为奇数，最后一个字节单独处理
//   3. 累加过程中，如果有进位（超过16位），将进位加回低16位
//   4. 最后对结果取反（1的补码）
inline uint16_t calculate_checksum(const void* data, int len) {
    // 累加和，使用32位以便处理进位
    uint32_t sum = 0;
    // 将数据指针转换为字节指针，逐字节处理以避免对齐问题
    const uint8_t* ptr = (const uint8_t*)data;
    
    // 按16位（2字节）为单位进行累加
    while (len > 1) {
        // 将两个字节组合成一个16位值（网络字节序：高字节在前）
        uint16_t word = ((uint16_t)ptr[0] << 8) | ptr[1];
        sum += word;   // 累加当前16位值
        ptr += 2;      // 移动指针
        len -= 2;      // 剩余长度减2
    }
    
    // 如果长度为奇数，处理最后一个字节
    // 将其视为高8位，低8位补0
    if (len == 1) {
        sum += (uint16_t)(*ptr) << 8;
    }
    
    // 将高16位的进位加到低16位
    // 可能产生新的进位，所以用while循环处理
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // 取反得到1的补码，返回16位校验和
    return (uint16_t)(~sum);
}

// ===== UDP协议头结构体（完整版，20字节对齐）=====
// 描述：定义完整的协议头格式，用于可靠数据传输
// 字段布局（总共20字节）：
//   - seq（序列号）：     4字节（32位）- 发送数据的起始序列号
//   - ack（确认号）：     4字节（32位）- 期望接收的下一个序列号
//   - flag（标志位）：    1字节（8位） - SYN/ACK/FIN/SACK
//   - win（窗口大小）：   2字节（16位）- 滑动窗口大小
//   - checksum（校验和）：2字节（16位）- 覆盖协议头+数据的校验和
//   - len（数据长度）：   2字节（16位）- 后续数据段的字节数
//   - reserved（保留）：  5字节       - 保留字段，用于将来扩展
// 注意：使用#pragma pack确保结构体内存紧凑对齐，避免编译器填充字节
#pragma pack(push, 1)
struct UDPHeader {
    uint32_t seq;         // 序列号：发送数据的起始序列号（32位）
    uint32_t ack;         // 确认号：期望接收的下一个序列号（32位）
    uint8_t  flag;        // 标志位：SYN(0x01)/ACK(0x02)/FIN(0x04)/SACK(0x08)（8位）
    uint16_t win;         // 窗口大小：用于流量控制（16位）
    uint16_t checksum;    // 校验和：覆盖协议头+数据，采用RFC 1071标准（16位）
    uint16_t len;         // 数据长度：后续数据段的字节数（16位）
    uint8_t  reserved[5]; // 保留字段：用于将来扩展，凑足20字节对齐
    
    // ===== 默认构造函数 =====
    // 描述：初始化协议头所有字段为默认值
    UDPHeader() : seq(0), ack(0), flag(0), win(DEFAULT_WINDOW_SIZE), 
                  checksum(0), len(0) {
        memset(reserved, 0, sizeof(reserved));  // 保留字段清零
    }
    
    // ===== 带参数的构造函数 =====
    // 描述：使用指定的序列号、确认号和标志位初始化协议头
    // 参数：
    //   s - 序列号
    //   a - 确认号
    //   f - 标志位
    UDPHeader(uint32_t s, uint32_t a, uint8_t f) 
        : seq(s), ack(a), flag(f), win(DEFAULT_WINDOW_SIZE), 
          checksum(0), len(0) {
        memset(reserved, 0, sizeof(reserved));  // 保留字段清零
    }
    
    // ===== 计算并填充校验和 =====
    // 描述：计算协议头+数据的校验和，并填入checksum字段
    // 算法：采用RFC 1071标准
    //   1. 先将checksum字段设为0
    //   2. 将协议头和数据复制到临时缓冲区
    //   3. 对整个缓冲区计算校验和
    //   4. 将计算结果填入checksum字段
    // 参数：
    //   data    - 数据部分的指针
    //   dataLen - 数据长度（字节数）
    void calculateChecksum(const char* data, int dataLen) {
        // 步骤1：将checksum字段设为0（校验和计算不包括自身）
        checksum = 0;
        
        // 步骤2：创建临时缓冲区，存放协议头+数据
        int totalLen = HEADER_SIZE + dataLen;
        char* buffer = new char[totalLen];
        
        // 步骤3：将协议头复制到缓冲区前部
        memcpy(buffer, this, HEADER_SIZE);
        
        // 步骤4：将数据复制到缓冲区后部
        if (dataLen > 0 && data != nullptr) {
            memcpy(buffer + HEADER_SIZE, data, dataLen);
        }
        
        // 步骤5：调用RFC 1071算法计算校验和
        checksum = calculate_checksum(buffer, totalLen);
        
        // 步骤6：释放临时缓冲区
        delete[] buffer;
    }
    
    // ===== 验证校验和 =====
    // 描述：验证接收到的数据包是否完整无损
    // 算法：采用RFC 1071标准验证
    //   对整个"协议头+数据"重新计算校验和
    //   如果结果为0，则校验通过；否则数据包损坏
    // 参数：
    //   data    - 数据部分的指针
    //   dataLen - 数据长度（字节数）
    // 返回值：true表示校验通过，false表示数据包损坏
    bool verifyChecksum(const char* data, int dataLen) const {
        // 步骤1：创建临时缓冲区，存放协议头+数据
        int totalLen = HEADER_SIZE + dataLen;
        char* buffer = new char[totalLen];
        
        // 步骤2：将协议头复制到缓冲区（包含原始checksum）
        memcpy(buffer, this, HEADER_SIZE);
        
        // 步骤3：将数据复制到缓冲区
        if (dataLen > 0 && data != nullptr) {
            memcpy(buffer + HEADER_SIZE, data, dataLen);
        }
        
        // 步骤4：对整个缓冲区计算校验和
        // RFC 1071特性：如果数据完整，重新计算的校验和应为0
        uint16_t result = calculate_checksum(buffer, totalLen);
        
        // 步骤5：释放临时缓冲区
        delete[] buffer;
        
        // 步骤6：校验和为0表示数据完整
        return (result == 0);
    }
};
#pragma pack(pop)

// ===== 数据包结构体 =====
// 描述：封装完整的数据包，包含协议头和数据负载
struct Packet {
    UDPHeader header;                // 协议头（20字节）
    char data[MAX_DATA_SIZE];        // 数据负载缓冲区
    int dataLen;                     // 实际数据长度（内部使用，不发送）
    
    // ===== 默认构造函数 =====
    // 描述：初始化数据包，清空数据缓冲区
    Packet() : dataLen(0) {
        memset(data, 0, MAX_DATA_SIZE);
    }
    
    // ===== 设置数据负载 =====
    // 描述：设置数据包的数据部分，并自动更新协议头的len字段和校验和
    // 参数：
    //   buf - 数据源指针
    //   len - 数据长度
    void setData(const char* buf, int len) {
        // 确保数据长度不超过最大负载大小
        dataLen = (len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : len;
        // 复制数据到缓冲区
        memcpy(data, buf, dataLen);
        // 更新协议头中的数据长度字段
        header.len = (uint16_t)dataLen;
        // 计算并填充校验和
        header.calculateChecksum(data, dataLen);
    }
    
    // ===== 获取数据包总长度 =====
    // 描述：返回协议头+数据的总长度
    // 返回值：数据包总长度（字节）
    int getTotalLen() const {
        return HEADER_SIZE + dataLen;
    }
    
    // ===== 序列化为字节流 =====
    // 描述：将数据包转换为字节流，用于网络发送
    // 参数：
    //   buffer - 目标缓冲区（需预分配足够空间）
    void serialize(char* buffer) const {
        // 先复制协议头
        memcpy(buffer, &header, HEADER_SIZE);
        // 再复制数据部分
        memcpy(buffer + HEADER_SIZE, data, dataLen);
    }
    
    // ===== 从字节流反序列化 =====
    // 描述：从接收到的字节流中解析出数据包，并验证校验和
    // 参数：
    //   buffer - 接收到的字节流
    //   bufLen - 字节流长度
    // 返回值：true表示解析成功且校验和正确，false表示失败或数据损坏
    bool deserialize(const char* buffer, int bufLen) {
        // 检查缓冲区长度是否足够包含协议头
        if (bufLen < HEADER_SIZE) {
            return false;
        }
        // 解析协议头
        memcpy(&header, buffer, HEADER_SIZE);
        // 计算数据部分长度（可以从header.len或bufLen-HEADER_SIZE获取）
        // 这里使用header.len字段，更可靠
        dataLen = header.len;
        // 安全检查：确保数据长度合理
        if (dataLen > MAX_DATA_SIZE || dataLen > bufLen - HEADER_SIZE) {
            return false;
        }
        // 复制数据部分
        if (dataLen > 0) {
            memcpy(data, buffer + HEADER_SIZE, dataLen);
        }
        // 验证校验和，确保数据完整性
        return header.verifyChecksum(data, dataLen);
    }
};

// ===== 辅助函数 =====

// ===== 生成随机初始序列号 =====
// 描述：生成一个随机的初始序列号，用于连接建立
// 返回值：32位随机序列号
inline uint32_t generateInitialSeq() {
    srand((unsigned int)time(NULL));
    return (uint32_t)rand();
}

// ===== 获取当前时间戳 =====
// 描述：获取当前时间的毫秒表示
// 返回值：当前时间戳（毫秒）
inline long long getCurrentTimeMs() {
    return (long long)time(NULL) * 1000;
}

// ===== 获取状态名称 =====
// 描述：将连接状态枚举值转换为可读的字符串，用于调试输出
// 参数：
//   state - 连接状态枚举值
// 返回值：状态名称字符串
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

// ===== 获取标志位名称 =====
// 描述：将标志位转换为可读的字符串，用于调试输出
// 参数：
//   flag - 标志位值
// 返回值：标志位名称字符串（静态缓冲区）
inline const char* getFlagName(uint8_t flag) {
    static char flagStr[32];
    flagStr[0] = '\0';
    if (flag & FLAG_SYN)  strcat(flagStr, "SYN ");
    if (flag & FLAG_ACK)  strcat(flagStr, "ACK ");
    if (flag & FLAG_FIN)  strcat(flagStr, "FIN ");
    if (flag & FLAG_SACK) strcat(flagStr, "SACK ");
    if (flagStr[0] == '\0') strcpy(flagStr, "NONE");
    return flagStr;
}

// ===== 数据包发送工具函数 =====
// 描述：封装数据包发送过程，自动填充协议头、计算校验和、发送完整数据包
// 参数：
//   sockfd    - 套接字文件描述符
//   dest_addr - 目标地址结构体指针
//   addr_len  - 地址结构体长度
//   data      - 要发送的数据指针（可为NULL表示无数据）
//   data_len  - 数据长度（字节）
//   seq       - 序列号
//   ack       - 确认号
//   flag      - 标志位
// 返回值：发送的字节数，失败返回-1
inline int send_packet(int sockfd, const struct sockaddr* dest_addr, int addr_len,
                       const void* data, int data_len, 
                       uint32_t seq, uint32_t ack, uint8_t flag) {
    // 步骤1：创建数据包对象
    Packet packet;
    
    // 步骤2：填充协议头字段
    packet.header.seq = seq;           // 设置序列号
    packet.header.ack = ack;           // 设置确认号
    packet.header.flag = flag;         // 设置标志位
    packet.header.win = DEFAULT_WINDOW_SIZE;  // 设置窗口大小
    
    // 步骤3：填充数据部分
    if (data != NULL && data_len > 0) {
        // 确保数据长度不超过最大负载大小
        packet.dataLen = (data_len > MAX_DATA_SIZE) ? MAX_DATA_SIZE : data_len;
        memcpy(packet.data, data, packet.dataLen);
    } else {
        packet.dataLen = 0;
    }
    
    // 步骤4：更新协议头中的数据长度字段
    packet.header.len = (uint16_t)packet.dataLen;
    
    // 步骤5：计算校验和（覆盖协议头+数据）
    packet.header.calculateChecksum(packet.data, packet.dataLen);
    
    // 步骤6：序列化数据包到发送缓冲区
    char sendBuffer[MAX_PACKET_SIZE];
    packet.serialize(sendBuffer);
    
    // 步骤7：发送数据包
    int bytesSent = sendto(sockfd, sendBuffer, packet.getTotalLen(), 0, 
                           dest_addr, addr_len);
    
    return bytesSent;
}

// ===== 数据包接收工具函数 =====
// 描述：接收数据包，验证校验和，解析协议头和数据
// 参数：
//   sockfd     - 套接字文件描述符
//   src_addr   - 用于存储发送方地址的结构体指针
//   addr_len   - 地址结构体长度指针（输入/输出参数）
//   buf        - 用于存储接收数据的缓冲区指针
//   buf_len    - 缓冲区大小
//   header_out - 用于输出解析后的协议头
// 返回值：
//   成功时返回数据部分的长度（可为0），
//   校验和失败返回-2，
//   其他错误返回-1
inline int recv_packet(int sockfd, struct sockaddr* src_addr, int* addr_len,
                       void* buf, int buf_len, UDPHeader* header_out) {
    // 步骤1：创建接收缓冲区
    char recvBuffer[MAX_PACKET_SIZE];
    
    // 步骤2：接收数据包
    int bytesReceived = recvfrom(sockfd, recvBuffer, MAX_PACKET_SIZE, 0, 
                                 src_addr, addr_len);
    
    // 步骤3：检查接收是否成功
    if (bytesReceived <= 0) {
        return -1;  // 接收失败
    }
    
    // 步骤4：检查是否至少包含完整协议头
    if (bytesReceived < HEADER_SIZE) {
        return -1;  // 数据包不完整
    }
    
    // 步骤5：创建数据包对象并反序列化
    Packet packet;
    if (!packet.deserialize(recvBuffer, bytesReceived)) {
        // 校验和验证失败，数据包损坏
        return -2;
    }
    
    // 步骤6：输出解析后的协议头
    if (header_out != NULL) {
        memcpy(header_out, &packet.header, sizeof(UDPHeader));
    }
    
    // 步骤7：将数据复制到用户缓冲区
    int copyLen = 0;
    if (packet.dataLen > 0 && buf != NULL && buf_len > 0) {
        copyLen = (packet.dataLen > buf_len) ? buf_len : packet.dataLen;
        memcpy(buf, packet.data, copyLen);
    }
    
    // 步骤8：返回数据部分的长度
    return copyLen;
}

#endif // PROTOCOL_H
