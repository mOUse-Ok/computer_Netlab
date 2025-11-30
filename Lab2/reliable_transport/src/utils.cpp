#include "utils.h"
#include "packet.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdarg>

// Platform-specific headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "wsock32.lib")
    #define SHUT_RDWR 2
#else
    #include <sys/time.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

// ==================== 全局日志文件指针 ====================

static FILE* g_log_file = NULL;

// ==================== 获取时间戳 ====================

uint64_t get_timestamp_ms()
{
#ifdef _WIN32
    // Windows implementation using GetSystemTimeAsFileTime
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    // Convert from 100-nanosecond intervals since 1601 to milliseconds since 1970
    return (uli.QuadPart - 116444736000000000ULL) / 10000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

// ==================== 计算时间差 ====================

uint64_t get_time_diff_ms(uint64_t end_time, uint64_t start_time)
{
    return (end_time >= start_time) ? (end_time - start_time) : 0;
}

// ==================== 十六进制打印 ====================

void print_buffer_hex(const uint8_t* buffer, size_t len, const char* label)
{
    if (buffer == NULL || len == 0) {
        return;
    }

    if (label != NULL) {
        printf("%s:\n", label);
    }

    for (size_t i = 0; i < len; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    if (len % 16 != 0) {
        printf("\n");
    }
}

// ==================== ASCII打印 ====================

void print_buffer_ascii(const uint8_t* buffer, size_t len, const char* label)
{
    if (buffer == NULL || len == 0) {
        return;
    }

    if (label != NULL) {
        printf("%s:\n", label);
    }

    for (size_t i = 0; i < len; i++) {
        if (buffer[i] >= 32 && buffer[i] < 127) {
            printf("%c", buffer[i]);
        } else {
            printf(".");
        }
    }

    printf("\n");
}

// ==================== 日志输出 ====================

void log_message(int level, const char* format, ...)
{
    if (format == NULL) {
        return;
    }

    FILE* output = (g_log_file != NULL) ? g_log_file : stdout;

    const char* level_str = "";
    switch (level) {
        case 0: level_str = "[INFO] "; break;
        case 1: level_str = "[WARNING] "; break;
        case 2: level_str = "[ERROR] "; break;
        default: level_str = "[UNKNOWN] "; break;
    }

    fprintf(output, "%s", level_str);

    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);

    fprintf(output, "\n");
    fflush(output);
}

// ==================== 日志初始化 ====================

int log_init(const char* filename)
{
    if (filename == NULL) {
        g_log_file = NULL;
        return 0;
    }

    g_log_file = fopen(filename, "a");
    return (g_log_file != NULL) ? 0 : -1;
}

// ==================== 日志清理 ====================

void log_cleanup()
{
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

// ==================== 生成随机序列号 ====================

uint32_t generate_random_seq()
{
    static unsigned int seed = 0;
    if (seed == 0) {
        seed = (unsigned int)time(NULL);
        srand(seed);
    }

    return (uint32_t)rand();
}

// ==================== IP地址校验 ====================

int is_valid_ip(const char* ip_str)
{
    if (ip_str == NULL) {
        return 0;
    }

    // 简单的IP校验：检查是否为127.0.0.1或localhost等
    int parts[4];
    if (sscanf(ip_str, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]) != 4) {
        return 0;
    }

    for (int i = 0; i < 4; i++) {
        if (parts[i] < 0 || parts[i] > 255) {
            return 0;
        }
    }

    return 1;
}

// ==================== 端口号校验 ====================

int is_valid_port(uint16_t port)
{
    return (port > 0 && port < 65536) ? 1 : 0;
}

// ==================== 时间相关函数（扩展） ====================

/**
 * 获取当前时间（毫秒）
 * 与get_timestamp_ms()等价，但返回类型为long
 */
long get_current_time_ms()
{
    return (long)get_timestamp_ms();
}

/**
 * 毫秒级延迟函数
 */
void sleep_ms(int milliseconds)
{
    if (milliseconds <= 0) {
        return;
    }

#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

// ==================== 网络相关函数 ====================

/**
 * 创建UDP套接字
 */
int create_udp_socket()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        log_message(2, "Failed to create UDP socket");
        return -1;
    }

    // 设置套接字为可复用
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        log_message(1, "Warning: Failed to set SO_REUSEADDR");
    }

    log_message(0, "UDP socket created: fd=%d", sockfd);
    return sockfd;
}

/**
 * 绑定套接字到指定端口
 */
bool bind_socket(int sockfd, int port)
{
    if (sockfd < 0 || port <= 0 || port > 65535) {
        log_message(2, "Invalid socket or port: sockfd=%d, port=%d", sockfd, port);
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_message(2, "Failed to bind socket to port %d", port);
        return false;
    }

    log_message(0, "Socket bound to port %d", port);
    return true;
}

/**
 * 向指定地址发送数据包
 */
ssize_t send_packet(int sockfd, const struct sockaddr_in* addr, const Frame* frame)
{
    if (sockfd < 0 || addr == NULL || frame == NULL) {
        log_message(2, "Invalid parameters for send_packet");
        return -1;
    }

    // 序列化Frame
    uint8_t buffer[MAX_PACKET_SIZE];
    int frame_size = frame_serialize(frame, buffer, MAX_PACKET_SIZE);
    
    if (frame_size <= 0) {
        log_message(2, "Failed to serialize frame");
        return -1;
    }

    // 发送数据包
    ssize_t sent = sendto(sockfd, (const char*)buffer, frame_size, 0,
                         (struct sockaddr*)addr, sizeof(*addr));
    
    if (sent < 0) {
        log_message(2, "Failed to send packet");
        return -1;
    }

    log_message(0, "Packet sent: %zd bytes to %s:%d", sent, 
               inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    
    return sent;
}

/**
 * 从套接字接收数据包
 */
ssize_t receive_packet(int sockfd, struct sockaddr_in* addr, Frame* frame)
{
    if (sockfd < 0 || addr == NULL || frame == NULL) {
        log_message(2, "Invalid parameters for receive_packet");
        return -1;
    }

    uint8_t buffer[MAX_PACKET_SIZE];
    socklen_t addr_len = sizeof(*addr);

    // 接收数据包
    ssize_t received = recvfrom(sockfd, (char*)buffer, MAX_PACKET_SIZE, 0,
                               (struct sockaddr*)addr, &addr_len);
    
    if (received < 0) {
        log_message(2, "Failed to receive packet");
        return -1;
    }

    if (received == 0) {
        log_message(1, "Received empty packet");
        return 0;
    }

    // 反序列化Frame
    int frame_size = frame_deserialize(buffer, received, frame);
    
    if (frame_size <= 0) {
        log_message(2, "Failed to deserialize frame");
        return -1;
    }

    log_message(0, "Packet received: %zd bytes from %s:%d", received,
               inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    
    return received;
}

/**
 * 设置套接字超时
 */
bool set_socket_timeout(int sockfd, int timeout_ms)
{
    if (sockfd < 0 || timeout_ms < 0) {
        log_message(2, "Invalid socket or timeout value");
        return false;
    }

#ifdef _WIN32
    // Windows: 毫秒
    DWORD timeout = timeout_ms;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        log_message(2, "Failed to set receive timeout");
        return false;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        log_message(2, "Failed to set send timeout");
        return false;
    }
#else
    // Linux/Unix: timeval结构
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        log_message(2, "Failed to set receive timeout");
        return false;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        log_message(2, "Failed to set send timeout");
        return false;
    }
#endif

    log_message(0, "Socket timeout set to %d ms", timeout_ms);
    return true;
}

// ==================== 文件操作函数 ====================

/**
 * 打开文件用于读取
 */
FILE* open_file_for_read(const char* filename)
{
    if (filename == NULL) {
        log_message(2, "Filename is NULL");
        return NULL;
    }

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        log_message(2, "Failed to open file for reading: %s", filename);
        return NULL;
    }

    log_message(0, "File opened for reading: %s", filename);
    return file;
}

/**
 * 打开文件用于写入
 */
FILE* open_file_for_write(const char* filename)
{
    if (filename == NULL) {
        log_message(2, "Filename is NULL");
        return NULL;
    }

    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        log_message(2, "Failed to open file for writing: %s", filename);
        return NULL;
    }

    log_message(0, "File opened for writing: %s", filename);
    return file;
}

/**
 * 从文件读取数据块
 */
size_t read_file_chunk(FILE* file, char* buffer, size_t max_length)
{
    if (file == NULL || buffer == NULL || max_length == 0) {
        log_message(2, "Invalid parameters for read_file_chunk");
        return 0;
    }

    size_t bytes_read = fread(buffer, 1, max_length, file);
    
    if (bytes_read == 0 && ferror(file)) {
        log_message(2, "Error reading from file");
        return 0;
    }

    if (bytes_read > 0) {
        log_message(0, "Read %zu bytes from file", bytes_read);
    }

    return bytes_read;
}

/**
 * 向文件写入数据块
 */
size_t write_file_chunk(FILE* file, const char* buffer, size_t length)
{
    if (file == NULL || buffer == NULL || length == 0) {
        log_message(2, "Invalid parameters for write_file_chunk");
        return 0;
    }

    size_t bytes_written = fwrite(buffer, 1, length, file);
    
    if (bytes_written != length) {
        log_message(2, "Failed to write all bytes to file: %zu/%zu", bytes_written, length);
        return bytes_written;
    }

    log_message(0, "Written %zu bytes to file", bytes_written);
    return bytes_written;
}

// ==================== 统计和日志函数（扩展） ====================

/**
 * 打印传输统计信息
 */
void print_statistics(const char* filename, size_t total_bytes, 
                      long total_time_ms, int total_packets, int retransmitted_packets)
{
    FILE* output = stdout;
    
    // 如果指定了文件名，尝试打开文件
    if (filename != NULL && strlen(filename) > 0) {
        output = fopen(filename, "a");
        if (output == NULL) {
            log_message(2, "Failed to open statistics file: %s", filename);
            output = stdout;
        }
    }

    fprintf(output, "\n");
    fprintf(output, "========== 传输统计信息 ==========\n");
    fprintf(output, "总传输字节数:     %zu bytes\n", total_bytes);
    fprintf(output, "传输总耗时:       %ld ms (%.2f s)\n", total_time_ms, total_time_ms / 1000.0);
    fprintf(output, "总包数:          %d packets\n", total_packets);
    fprintf(output, "重传包数:        %d packets\n", retransmitted_packets);
    
    // 计算传输速率
    if (total_time_ms > 0) {
        double throughput = (double)total_bytes * 8 / (total_time_ms / 1000.0) / 1000000; // Mbps
        fprintf(output, "平均传输速率:     %.2f Mbps\n", throughput);
    }
    
    // 计算包丢失率
    if (total_packets > 0) {
        double loss_rate = (double)retransmitted_packets / total_packets * 100;
        fprintf(output, "包丢失率:        %.2f%% (%d/%d)\n", loss_rate, retransmitted_packets, total_packets);
    }
    
    // 计算平均包大小
    if (total_packets > 0) {
        double avg_size = (double)total_bytes / total_packets;
        fprintf(output, "平均包大小:      %.0f bytes\n", avg_size);
    }
    
    fprintf(output, "===================================\n");
    fflush(output);
    
    if (output != stdout) {
        fclose(output);
    }
}

// ==================== 命令行参数解析 ====================

/**
 * 打印使用说明
 */
static void print_usage(const char* program_name)
{
    printf("使用方法: %s [选项]\n", program_name);
    printf("\n选项:\n");
    printf("  -s, --server              以服务器模式运行\n");
    printf("  -c, --client              以客户端模式运行（默认）\n");
    printf("  -i, --server-ip <IP>      服务器IP地址（仅客户端模式，默认127.0.0.1）\n");
    printf("  -p, --port <PORT>         端口号（默认%d）\n", DEFAULT_PORT);
    printf("  -in, --input <FILE>       输入文件名（客户端模式）\n");
    printf("  -out, --output <FILE>     输出文件名（服务器模式）\n");
    printf("  -w, --window <SIZE>       窗口大小（默认%d）\n", WINDOW_SIZE);
    printf("  -h, --help                显示此帮助信息\n");
}

/**
 * 解析命令行参数
 */
bool parse_command_line(int argc, char* argv[], 
                       bool& is_server, char* server_ip, int& port, 
                       char* input_file, char* output_file, int& window_size)
{
    // 设置默认值
    is_server = false;
    strncpy(server_ip, "127.0.0.1", 15);
    server_ip[15] = '\0';
    port = DEFAULT_PORT;
    strcpy(input_file, "");
    strcpy(output_file, "");
    window_size = WINDOW_SIZE;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return false;
        }
        else if (strcmp(arg, "-s") == 0 || strcmp(arg, "--server") == 0) {
            is_server = true;
            log_message(0, "Server mode enabled");
        }
        else if (strcmp(arg, "-c") == 0 || strcmp(arg, "--client") == 0) {
            is_server = false;
            log_message(0, "Client mode enabled");
        }
        else if (strcmp(arg, "-i") == 0 || strcmp(arg, "--server-ip") == 0) {
            if (i + 1 >= argc) {
                log_message(2, "Missing value for %s", arg);
                return false;
            }
            const char* ip = argv[++i];
            if (!is_valid_ip(ip)) {
                log_message(2, "Invalid IP address: %s", ip);
                return false;
            }
            strncpy(server_ip, ip, 15);
            server_ip[15] = '\0';
            log_message(0, "Server IP set to: %s", server_ip);
        }
        else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--port") == 0) {
            if (i + 1 >= argc) {
                log_message(2, "Missing value for %s", arg);
                return false;
            }
            port = atoi(argv[++i]);
            if (!is_valid_port(port)) {
                log_message(2, "Invalid port number: %d", port);
                return false;
            }
            log_message(0, "Port set to: %d", port);
        }
        else if (strcmp(arg, "-in") == 0 || strcmp(arg, "--input") == 0) {
            if (i + 1 >= argc) {
                log_message(2, "Missing value for %s", arg);
                return false;
            }
            strncpy(input_file, argv[++i], 255);
            input_file[255] = '\0';
            log_message(0, "Input file: %s", input_file);
        }
        else if (strcmp(arg, "-out") == 0 || strcmp(arg, "--output") == 0) {
            if (i + 1 >= argc) {
                log_message(2, "Missing value for %s", arg);
                return false;
            }
            strncpy(output_file, argv[++i], 255);
            output_file[255] = '\0';
            log_message(0, "Output file: %s", output_file);
        }
        else if (strcmp(arg, "-w") == 0 || strcmp(arg, "--window") == 0) {
            if (i + 1 >= argc) {
                log_message(2, "Missing value for %s", arg);
                return false;
            }
            window_size = atoi(argv[++i]);
            if (window_size <= 0 || window_size > 1024) {
                log_message(2, "Invalid window size: %d (must be 1-1024)", window_size);
                return false;
            }
            log_message(0, "Window size set to: %d", window_size);
        }
        else {
            log_message(2, "Unknown option: %s", arg);
            print_usage(argv[0]);
            return false;
        }
    }

    return true;
}
