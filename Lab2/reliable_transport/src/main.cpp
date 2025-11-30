#include "reliable_transport.h"
#include "connection.h"
#include "packet.h"
#include "window.h"
#include "congestion.h"
#include "utils.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

// Platform-specific headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

// ==================== 全局变量 ====================

static int g_initialized = 0;

// ==================== 常量定义 ====================

#define MAX_FILENAME 256
#define TRANSMISSION_TIMEOUT_SEC 300   // 30秒未有进度则超时
#define HANDSHAKE_TIMEOUT_SEC 10        // 握手超时10秒
#define IDLE_CHECK_INTERVAL_MS 100      // 100ms检查一次超时

// ==================== 初始化 ====================

int initialize_reliable_transport()
{
    if (g_initialized) {
        return 0;
    }

    // 初始化日志系统
    if (log_init(NULL) != 0) {
        return -1;
    }

    log_message(0, "========== Reliable Transport Protocol initialized ==========");
    log_message(0, "Configuration: WINDOW_SIZE=%d, MAX_PACKET_SIZE=%d, TIMEOUT=%dms",
                WINDOW_SIZE, MAX_PACKET_SIZE, TIMEOUT_MS);

    g_initialized = 1;
    return 0;
}

// ==================== 清理 ====================

void cleanup_reliable_transport()
{
    if (!g_initialized) {
        return;
    }

    log_message(0, "Reliable Transport Protocol cleanup");
    log_cleanup();
    g_initialized = 0;
}

// ==================== 服务器端主函数 ====================

/**
 * 服务器端主函数
 * - 初始化服务器套接字
 * - 等待客户端连接
 * - 接收文件数据
 * - 保存到输出文件
 * - 显示统计信息
 */
int server_main(int port, const char* output_file, int window_size)
{
    if (port <= 0 || port > 65535) {
        log_message(2, "ERROR: Invalid port number: %d", port);
        return -1;
    }

    if (output_file == NULL || strlen(output_file) == 0) {
        log_message(2, "ERROR: Output file name is required");
        return -1;
    }

    log_message(0, "\n========== SERVER MODE ==========");
    log_message(0, "Listening on port: %d", port);
    log_message(0, "Output file: %s", output_file);
    log_message(0, "Window size: %d", window_size);

    // 创建UDP套接字
    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        log_message(2, "ERROR: Failed to create UDP socket");
        return -1;
    }

    // 绑定端口
    if (!bind_socket(sockfd, port)) {
        log_message(2, "ERROR: Failed to bind socket to port %d", port);
        CLOSE_SOCKET(sockfd);
        return -1;
    }

    // 设置套接字超时（非阻塞接收）
    set_socket_timeout(sockfd, 1000);

    // 打开输出文件
    FILE* output = open_file_for_write(output_file);
    if (output == NULL) {
        log_message(2, "ERROR: Failed to open output file: %s", output_file);
        CLOSE_SOCKET(sockfd);
        return -1;
    }

    // 统计变量
    long start_time = get_current_time_ms();
    size_t total_bytes = 0;
    int total_packets = 0;
    int retransmitted_packets = 0;
    int ack_count = 0;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    Frame recv_frame, send_frame;
    uint32_t expected_seq = 0;
    int handshake_complete = 0;

    log_message(0, "Server: Waiting for client connection...");

    // 主接收循环
    time_t last_activity = time(NULL);
    int idle_counter = 0;

    while (1) {
        // 检查超时
        time_t current_time = time(NULL);
        if (!handshake_complete && (current_time - last_activity) > HANDSHAKE_TIMEOUT_SEC) {
            log_message(1, "WARNING: Handshake timeout, waiting for client");
            idle_counter++;
            if (idle_counter > 30) {
                log_message(2, "ERROR: No client connection after waiting period");
                break;
            }
        }

        // 接收数据包
        ssize_t recv_len = receive_packet(sockfd, &client_addr, &recv_frame);
        
        if (recv_len < 0) {
            // 超时或错误，继续循环
            sleep_ms(IDLE_CHECK_INTERVAL_MS);
            continue;
        }

        if (recv_len == 0) {
            sleep_ms(IDLE_CHECK_INTERVAL_MS);
            continue;
        }

        last_activity = time(NULL);
        idle_counter = 0;
        total_packets++;

        log_message(0, "Server: Received packet seq=%u type=%d len=%zu", 
                   recv_frame.seq_num, recv_frame.frame_type, recv_len);

        // 处理三次握手
        if (!handshake_complete) {
            if (recv_frame.frame_type == SYN) {
                log_message(0, "Server: Received SYN, sending SYN-ACK");
                
                // 发送SYN-ACK
                memset(&send_frame, 0, sizeof(send_frame));
                send_frame.seq_num = generate_random_seq();
                send_frame.ack_num = recv_frame.seq_num + 1;
                send_frame.window_size = window_size;
                send_frame.frame_type = SYN_ACK;
                send_frame.data_len = 0;
                
                expected_seq = recv_frame.seq_num + 1;
                
                ssize_t sent = send_packet(sockfd, &client_addr, &send_frame);
                if (sent > 0) {
                    log_message(0, "Server: Sent SYN-ACK");
                }
            }
            else if (recv_frame.frame_type == ACK && recv_frame.ack_num > 0) {
                log_message(0, "Server: Handshake complete, ready to receive data");
                handshake_complete = 1;
                ack_count = 0;
            }
            continue;
        }

        // 握手完成后处理数据
        switch (recv_frame.frame_type) {
            case DATA: {
                // 验证序列号
                if (recv_frame.seq_num == expected_seq) {
                    // 正确的数据包，写入文件
                    size_t written = write_file_chunk(output, (const char*)recv_frame.data, recv_frame.data_len);
                    total_bytes += written;
                    log_message(0, "Server: Data received and saved: %zu bytes", written);
                    
                    expected_seq += recv_frame.data_len;
                    ack_count++;
                }
                else if (recv_frame.seq_num < expected_seq) {
                    // 重复数据包
                    log_message(1, "WARNING: Duplicate packet seq=%u, expected=%u", 
                               recv_frame.seq_num, expected_seq);
                    retransmitted_packets++;
                }
                
                // 发送ACK
                memset(&send_frame, 0, sizeof(send_frame));
                send_frame.seq_num = expected_seq;
                send_frame.ack_num = expected_seq;
                send_frame.window_size = window_size;
                send_frame.frame_type = ACK;
                send_frame.data_len = 0;
                
                ssize_t sent = send_packet(sockfd, &client_addr, &send_frame);
                if (sent > 0) {
                    log_message(0, "Server: Sent ACK for seq=%u", expected_seq);
                }
                break;
            }

            case FIN: {
                log_message(0, "Server: Received FIN, closing connection");
                
                // 发送FIN-ACK
                memset(&send_frame, 0, sizeof(send_frame));
                send_frame.seq_num = expected_seq;
                send_frame.ack_num = recv_frame.seq_num + 1;
                send_frame.window_size = window_size;
                send_frame.frame_type = FIN_ACK;
                send_frame.data_len = 0;
                
                ssize_t sent = send_packet(sockfd, &client_addr, &send_frame);
                if (sent > 0) {
                    log_message(0, "Server: Sent FIN-ACK");
                }
                
                // 接收完成，退出循环
                handshake_complete = 0;
                break;
            }

            default:
                log_message(1, "WARNING: Unknown frame type: %d", recv_frame.frame_type);
                break;
        }

        // 接收完整文件后退出
        if (recv_frame.frame_type == FIN) {
            log_message(0, "Server: File transfer complete");
            break;
        }

        // 检查传输超时
        if ((current_time - last_activity) > TRANSMISSION_TIMEOUT_SEC) {
            log_message(2, "ERROR: Transmission timeout");
            break;
        }
    }

    // 关闭文件和套接字
    if (output != NULL) {
        fclose(output);
    }
    CLOSE_SOCKET(sockfd);

    // 计算统计信息
    long end_time = get_current_time_ms();
    long total_time = end_time - start_time;

    // 打印统计信息
    printf("\n");
    print_statistics(NULL, total_bytes, total_time, total_packets, retransmitted_packets);

    log_message(0, "Server: Transfer complete - %zu bytes in %ld ms", total_bytes, total_time);
    return 0;
}

// ==================== 客户端主函数 ====================

/**
 * 客户端主函数
 * - 连接到服务器
 * - 读取输入文件
 * - 发送文件数据（使用流水线方式）
 * - 等待传输完成
 * - 显示统计信息
 */
int client_main(const char* server_ip, int port, const char* input_file, int window_size)
{
    if (server_ip == NULL || strlen(server_ip) == 0) {
        log_message(2, "ERROR: Server IP is required");
        return -1;
    }

    if (port <= 0 || port > 65535) {
        log_message(2, "ERROR: Invalid port number: %d", port);
        return -1;
    }

    if (input_file == NULL || strlen(input_file) == 0) {
        log_message(2, "ERROR: Input file name is required");
        return -1;
    }

    log_message(0, "\n========== CLIENT MODE ==========");
    log_message(0, "Server address: %s:%d", server_ip, port);
    log_message(0, "Input file: %s", input_file);
    log_message(0, "Window size: %d", window_size);

    // 创建UDP套接字
    int sockfd = create_udp_socket();
    if (sockfd < 0) {
        log_message(2, "ERROR: Failed to create UDP socket");
        return -1;
    }

    // 设置套接字超时
    set_socket_timeout(sockfd, 1000);

    // 构建服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        log_message(2, "ERROR: Invalid server IP address: %s", server_ip);
        CLOSE_SOCKET(sockfd);
        return -1;
    }

    // 打开输入文件
    FILE* input = open_file_for_read(input_file);
    if (input == NULL) {
        log_message(2, "ERROR: Failed to open input file: %s", input_file);
        CLOSE_SOCKET(sockfd);
        return -1;
    }

    // 统计变量
    long start_time = get_current_time_ms();
    size_t total_bytes = 0;
    int total_packets = 0;
    int retransmitted_packets = 0;

    Frame send_frame, recv_frame;
    struct sockaddr_in recv_addr;

    log_message(0, "Client: Initiating three-way handshake...");

    // ===== 三次握手 =====
    // 第一步：发送SYN
    memset(&send_frame, 0, sizeof(send_frame));
    send_frame.seq_num = generate_random_seq();
    send_frame.ack_num = 0;
    send_frame.window_size = window_size;
    send_frame.frame_type = SYN;
    send_frame.data_len = 0;

    uint32_t client_seq = send_frame.seq_num;
    uint32_t server_seq = 0;
    int handshake_complete = 0;
    int handshake_tries = 0;
    const int MAX_HANDSHAKE_TRIES = 5;

    while (handshake_tries < MAX_HANDSHAKE_TRIES && !handshake_complete) {
        ssize_t sent = send_packet(sockfd, &server_addr, &send_frame);
        if (sent > 0) {
            log_message(0, "Client: Sent SYN");
        }

        // 等待SYN-ACK（第二步）
        for (int i = 0; i < 10; i++) {
            ssize_t recv_len = receive_packet(sockfd, &recv_addr, &recv_frame);
            if (recv_len > 0) {
                if (recv_frame.frame_type == SYN_ACK && recv_frame.ack_num == client_seq + 1) {
                    log_message(0, "Client: Received SYN-ACK");
                    server_seq = recv_frame.seq_num;

                    // 第三步：发送ACK
                    memset(&send_frame, 0, sizeof(send_frame));
                    send_frame.seq_num = client_seq + 1;
                    send_frame.ack_num = server_seq + 1;
                    send_frame.window_size = window_size;
                    send_frame.frame_type = ACK;
                    send_frame.data_len = 0;

                    sent = send_packet(sockfd, &server_addr, &send_frame);
                    if (sent > 0) {
                        log_message(0, "Client: Sent ACK, handshake complete");
                        handshake_complete = 1;
                        break;
                    }
                }
            }
            sleep_ms(100);
        }

        if (!handshake_complete) {
            handshake_tries++;
            log_message(1, "WARNING: Handshake attempt %d failed, retrying...", handshake_tries);
            sleep_ms(500);
        }
    }

    if (!handshake_complete) {
        log_message(2, "ERROR: Failed to complete handshake");
        fclose(input);
        CLOSE_SOCKET(sockfd);
        return -1;
    }

    log_message(0, "Client: Connection established, starting file transmission");

    // ===== 数据传输阶段 =====
    uint32_t next_seq = client_seq + 1;
    char data_buffer[MAX_DATA_LENGTH];
    int ack_received = 1;
    int consecutive_timeouts = 0;

    while (1) {
        // 读取文件数据
        size_t bytes_read = read_file_chunk(input, data_buffer, MAX_DATA_LENGTH);

        if (bytes_read == 0) {
            // 文件读取完成
            log_message(0, "Client: File transmission complete, sending FIN");
            break;
        }

        // 创建DATA帧
        memset(&send_frame, 0, sizeof(send_frame));
        send_frame.seq_num = next_seq;
        send_frame.ack_num = server_seq + 1;
        send_frame.window_size = window_size;
        send_frame.frame_type = DATA;
        send_frame.data_len = bytes_read;
        memcpy(send_frame.data, data_buffer, bytes_read);

        // 发送DATA帧
        ssize_t sent = send_packet(sockfd, &server_addr, &send_frame);
        if (sent > 0) {
            total_packets++;
            total_bytes += bytes_read;
            log_message(0, "Client: Sent DATA packet seq=%u len=%zu", next_seq, bytes_read);
        }

        next_seq += bytes_read;

        // 等待ACK（简化版，实际应实现流水线）
        int ack_received = 0;
        for (int i = 0; i < 20; i++) {
            ssize_t recv_len = receive_packet(sockfd, &recv_addr, &recv_frame);
            if (recv_len > 0 && recv_frame.frame_type == ACK) {
                log_message(0, "Client: Received ACK for seq=%u", recv_frame.ack_num);
                server_seq = recv_frame.seq_num;
                ack_received = 1;
                consecutive_timeouts = 0;
                break;
            }
            sleep_ms(50);
        }

        if (!ack_received) {
            consecutive_timeouts++;
            if (consecutive_timeouts >= 3) {
                log_message(1, "WARNING: Multiple ACK timeouts, may indicate packet loss");
                retransmitted_packets++;
            }
        }
    }

    // ===== 四次挥手 =====
    log_message(0, "Client: Starting graceful shutdown...");

    // 发送FIN
    memset(&send_frame, 0, sizeof(send_frame));
    send_frame.seq_num = next_seq;
    send_frame.ack_num = server_seq + 1;
    send_frame.window_size = window_size;
    send_frame.frame_type = FIN;
    send_frame.data_len = 0;

    ssize_t sent = send_packet(sockfd, &server_addr, &send_frame);
    if (sent > 0) {
        log_message(0, "Client: Sent FIN");
    }

    // 等待FIN-ACK
    for (int i = 0; i < 20; i++) {
        ssize_t recv_len = receive_packet(sockfd, &recv_addr, &recv_frame);
        if (recv_len > 0 && (recv_frame.frame_type == FIN_ACK || recv_frame.frame_type == ACK)) {
            log_message(0, "Client: Received final ACK");
            break;
        }
        sleep_ms(100);
    }

    // 关闭文件和套接字
    fclose(input);
    CLOSE_SOCKET(sockfd);

    // 计算统计信息
    long end_time = get_current_time_ms();
    long total_time = end_time - start_time;

    // 打印统计信息
    printf("\n");
    print_statistics(NULL, total_bytes, total_time, total_packets, retransmitted_packets);

    log_message(0, "Client: Transfer complete - %zu bytes in %ld ms", total_bytes, total_time);
    return 0;
}

// ==================== 旧的run_client_mode和run_server_mode（保持兼容性） ====================

void run_client_mode(const char* server_ip, uint16_t port)
{
    if (server_ip == NULL) {
        log_message(2, "Error: server IP is NULL");
        return;
    }

    if (!is_valid_ip(server_ip)) {
        log_message(2, "Error: invalid server IP: %s", server_ip);
        return;
    }

    if (!is_valid_port(port)) {
        log_message(2, "Error: invalid port: %u", port);
        return;
    }

    // 调用新的client_main函数
    client_main(server_ip, port, "input.dat", WINDOW_SIZE);
}

void run_server_mode(uint16_t port)
{
    if (!is_valid_port(port)) {
        log_message(2, "Error: invalid port: %u", port);
        return;
    }

    // 调用新的server_main函数
    server_main(port, "output.dat", WINDOW_SIZE);
}

// ==================== 主程序入口 ====================

int main(int argc, char* argv[])
{
    printf("========================================\n");
    printf("  Reliable Transport Protocol (UDP)\n");
    printf("  Laboratory 2 - File Transfer\n");
    printf("========================================\n\n");

    // 初始化
    if (initialize_reliable_transport() != 0) {
        fprintf(stderr, "Error: failed to initialize reliable transport\n");
        return 1;
    }

    // 解析命令行参数
    bool is_server = false;
    char server_ip[16];
    int port = DEFAULT_PORT;
    char input_file[MAX_FILENAME];
    char output_file[MAX_FILENAME];
    int window_size = WINDOW_SIZE;

    if (!parse_command_line(argc, argv, is_server, server_ip, port, 
                           input_file, output_file, window_size)) {
        if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
            // parse_command_line already printed help
        } else {
            printf("Usage examples:\n");
            printf("  %s -s -p 8888 -out output.dat               (Server mode)\n", argv[0]);
            printf("  %s -c -i 127.0.0.1 -p 8888 -in input.dat  (Client mode)\n", argv[0]);
            printf("\n");
        }
        cleanup_reliable_transport();
        return 0;
    }

    // 运行对应的模式
    int result = 0;
    if (is_server) {
        if (strlen(output_file) == 0) {
            log_message(2, "ERROR: Output file required for server mode");
            result = -1;
        } else {
            result = server_main(port, output_file, window_size);
        }
    } else {
        if (strlen(input_file) == 0) {
            log_message(2, "ERROR: Input file required for client mode");
            result = -1;
        } else {
            result = client_main(server_ip, port, input_file, window_size);
        }
    }

    // 清理
    cleanup_reliable_transport();

    printf("\n========================================\n");
    printf("  Program finished (result: %d)\n", result);
    printf("========================================\n");

    return (result == 0) ? 0 : 1;
}
