#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cstdio>

// Platform-specific socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2ipdef.h>
    #include <iphlpapi.h>
    // Windows socket close macro
    #define CLOSE_SOCKET(s) closesocket(s)
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    // Unix socket close macro
    #define CLOSE_SOCKET(s) close(s)
#endif

// Include packet.h for Frame definition (needed by send/receive functions)
#include "packet.h"

/**
 * 获取当前时间戳（毫秒）
 * @return 当前时间戳
 */
uint64_t get_timestamp_ms();

/**
 * 计算两个时间戳的差值（毫秒）
 * @param end_time 结束时间戳
 * @param start_time 开始时间戳
 * @return 时间差（毫秒）
 */
uint64_t get_time_diff_ms(uint64_t end_time, uint64_t start_time);


/**
 * 打印缓冲区内容（十六进制，调试用）
 * @param buffer 缓冲区指针
 * @param len 缓冲区长度
 * @param label 标签（显示信息）
 */
void print_buffer_hex(const uint8_t* buffer, size_t len, const char* label);

/**
 * 打印缓冲区内容（ASCII字符，调试用）
 * @param buffer 缓冲区指针
 * @param len 缓冲区长度
 * @param label 标签（显示信息）
 */
void print_buffer_ascii(const uint8_t* buffer, size_t len, const char* label);

/**
 * 日志输出函数
 * @param level 日志级别（0=INFO, 1=WARNING, 2=ERROR）
 * @param format 格式字符串
 * @param ... 可变参数
 */
void log_message(int level, const char* format, ...);

/**
 * 初始化日志系统
 * @param filename 日志文件名（NULL表示输出到标准输出）
 * @return 成功返回0，失败返回-1
 */
int log_init(const char* filename);

/**
 * 关闭日志系统
 */
void log_cleanup();

/**
 * 生成随机序列号
 * @return 随机的32位序列号
 */
uint32_t generate_random_seq();

/**
 * 校验IP地址格式
 * @param ip_str IP地址字符串
 * @return 格式正确返回1，否则返回0
 */
int is_valid_ip(const char* ip_str);

/**
 * 校验端口号范围
 * @param port 端口号
 * @return 有效返回1，否则返回0
 */
int is_valid_port(uint16_t port);

// ==================== 时间相关函数（扩展） ====================

/**
 * 获取当前时间（毫秒），与get_timestamp_ms()等价
 * @return 当前时间戳（毫秒）
 */
long get_current_time_ms();

/**
 * 毫秒级延迟函数
 * @param milliseconds 延迟时间（毫秒）
 */
void sleep_ms(int milliseconds);

// ==================== 网络相关函数 ====================

/**
 * 创建UDP套接字
 * @return 套接字文件描述符，失败返回-1
 */
int create_udp_socket();

/**
 * 绑定套接字到指定端口
 * @param sockfd 套接字文件描述符
 * @param port 绑定的端口号
 * @return 成功返回true，失败返回false
 */
bool bind_socket(int sockfd, int port);

/**
 * 向指定地址发送数据包
 * @param sockfd 套接字文件描述符
 * @param addr 目标地址
 * @param frame 要发送的Frame指针
 * @return 发送的字节数，失败返回-1
 */
ssize_t send_packet(int sockfd, const struct sockaddr_in* addr, const Frame* frame);

/**
 * 从套接字接收数据包
 * @param sockfd 套接字文件描述符
 * @param addr 发送方地址（输出）
 * @param frame 接收的Frame指针（输出）
 * @return 接收的字节数，失败返回-1
 */
ssize_t receive_packet(int sockfd, struct sockaddr_in* addr, Frame* frame);

/**
 * 设置套接字超时
 * @param sockfd 套接字文件描述符
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功返回true，失败返回false
 */
bool set_socket_timeout(int sockfd, int timeout_ms);

// ==================== 文件操作函数 ====================

/**
 * 打开文件用于读取
 * @param filename 文件名
 * @return 文件指针，失败返回NULL
 */
FILE* open_file_for_read(const char* filename);

/**
 * 打开文件用于写入
 * @param filename 文件名
 * @return 文件指针，失败返回NULL
 */
FILE* open_file_for_write(const char* filename);

/**
 * 从文件读取数据块
 * @param file 文件指针
 * @param buffer 缓冲区
 * @param max_length 最大读取长度
 * @return 实际读取的字节数
 */
size_t read_file_chunk(FILE* file, char* buffer, size_t max_length);

/**
 * 向文件写入数据块
 * @param file 文件指针
 * @param buffer 缓冲区
 * @param length 写入长度
 * @return 实际写入的字节数
 */
size_t write_file_chunk(FILE* file, const char* buffer, size_t length);

// ==================== 统计和日志函数（扩展） ====================

/**
 * 打印传输统计信息
 * @param filename 输出文件名（如果为NULL则输出到stdout）
 * @param total_bytes 传输总字节数
 * @param total_time_ms 传输总耗时（毫秒）
 * @param total_packets 总包数
 * @param retransmitted_packets 重传包数
 */
void print_statistics(const char* filename, size_t total_bytes, 
                      long total_time_ms, int total_packets, int retransmitted_packets);

// ==================== 命令行参数解析 ====================

/**
 * 解析命令行参数
 * @param argc 参数个数
 * @param argv 参数数组
 * @param is_server 是否为服务器模式（输出）
 * @param server_ip 服务器IP地址（输出）
 * @param port 端口号（输出）
 * @param input_file 输入文件名（输出）
 * @param output_file 输出文件名（输出）
 * @param window_size 窗口大小（输出）
 * @return 解析成功返回true，失败返回false
 */
bool parse_command_line(int argc, char* argv[], 
                       bool& is_server, char* server_ip, int& port, 
                       char* input_file, char* output_file, int& window_size);

#endif // UTILS_H
