// connection.h - 连接管理模块头文件
// 定义连接类和连接管理相关函数

#ifndef CONNECTION_H
#define CONNECTION_H

#include "reliable_transport.h"
#include <string>
#include <netinet/in.h>

// 连接状态枚举
enum ConnectionState {
    CLOSED = 0,        // 连接关闭
    LISTEN = 1,        // 监听状态（服务器）
    SYN_SENT = 2,      // 已发送SYN（客户端）
    SYN_RECEIVED = 3,  // 已接收SYN（服务器）
    ESTABLISHED = 4,   // 连接已建立
    FIN_WAIT_1 = 5,    // 已发送FIN（主动关闭）
    FIN_WAIT_2 = 6,    // 已收到ACK，等待FIN（主动关闭）
    TIME_WAIT = 7,     // 等待连接完全关闭
    CLOSE_WAIT = 8,    // 已收到FIN（被动关闭）
    LAST_ACK = 9       // 已发送FIN和ACK（被动关闭）
};

// 连接类
class Connection {
private:
    int socket_fd;               // 套接字文件描述符
    struct sockaddr_in local_addr;  // 本地地址
    struct sockaddr_in remote_addr; // 远程地址
    ConnectionState state;       // 连接状态
    uint32_t seq_num;            // 序列号
    uint32_t ack_num;            // 确认号
    int window_size;             // 窗口大小
    bool is_server;              // 是否为服务器端

public:
    // 构造函数和析构函数
    Connection();
    ~Connection();

    // 连接管理函数
    int init(bool as_server, int port);
    int connect_to(const std::string& server_ip, int port);
    int accept_connection();
    int close_connection();

    // 数据传输函数
    int send_data(const char* data, size_t length);
    int receive_data(char* buffer, size_t max_length);

    // 状态查询函数
    ConnectionState get_state() const;
    bool is_connected() const;

    // 辅助函数
    int get_socket_fd() const;
    struct sockaddr_in get_remote_addr() const;
    void update_state(ConnectionState new_state);
};

// 连接管理工具函数
extern int create_udp_socket();
extern int bind_socket(int socket_fd, int port);
extern int set_non_blocking(int socket_fd, bool non_blocking);

extern int send_packet(int socket_fd, const struct sockaddr_in* addr, 
                       const char* data, size_t length);
extern int receive_packet(int socket_fd, struct sockaddr_in* addr, 
                          char* buffer, size_t max_length);

#endif // CONNECTION_H