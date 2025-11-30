// reliable_transport.h - 可靠传输协议主头文件
// 包含全局定义、配置参数和函数声明

#ifndef RELIABLE_TRANSPORT_H
#define RELIABLE_TRANSPORT_H

#include <string>
#include <cstdint>

// 基本配置参数
#define WINDOW_SIZE 8              // 最大窗口大小
#define MAX_PACKET_SIZE 1024       // 最大数据包大小
#define MAX_DATA_LENGTH 1000       // 数据包中数据部分的最大长度
#define TIMEOUT_MS 1000            // 超时时间（毫秒）
#define MAX_RETRIES 5              // 最大重传次数
#define DEFAULT_PORT 8888          // 默认端口号

// 数据包类型枚举
enum PacketType {
    DATA_PACKET = 0,      // 数据数据包
    ACK_PACKET = 1,       // 确认数据包
    SYN_PACKET = 2,       // 同步数据包（连接建立）
    FIN_PACKET = 3        // 结束数据包（连接关闭）
};

// 错误码枚举
enum ErrorCode {
    SUCCESS = 0,          // 成功
    ERROR_SOCKET = -1,    // 套接字错误
    ERROR_BIND = -2,      // 绑定错误
    ERROR_SEND = -3,      // 发送错误
    ERROR_RECEIVE = -4,   // 接收错误
    ERROR_TIMEOUT = -5,   // 超时错误
    ERROR_CONNECTION = -6 // 连接错误
};

// 主程序函数声明
int start_server(int port);
int start_client(const std::string& server_ip, int port);

// 打印使用帮助信息
void print_usage();

#endif // RELIABLE_TRANSPORT_H