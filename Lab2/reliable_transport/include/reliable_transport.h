#ifndef RELIABLE_TRANSPORT_H
#define RELIABLE_TRANSPORT_H

#include <cstdint>
#include <cstddef>

// ==================== 项目配置参数 ====================

// 窗口和数据包配置
#define WINDOW_SIZE 8                  // 最大窗口大小
#define MAX_PACKET_SIZE 1024           // 最大数据包大小（字节）
#define MAX_DATA_LENGTH 1000           // 数据包中数据部分的最大长度（字节）

// 超时和重传配置
#define TIMEOUT_MS 1000                // 超时时间（毫秒）
#define MAX_RETRIES 5                  // 最大重传次数

// 网络配置
#define DEFAULT_PORT 8888              // 默认端口号

// ==================== 全局函数声明 ====================

// 初始化函数
int initialize_reliable_transport();
void cleanup_reliable_transport();

// 客户端和服务器模式选择
void run_client_mode(const char* server_ip, uint16_t port);
void run_server_mode(uint16_t port);

#endif // RELIABLE_TRANSPORT_H
