// connection.cpp - 连接管理模块源文件
// 实现连接类的方法和连接管理工具函数

#include "connection.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// 构造函数
Connection::Connection() : 
    socket_fd(-1), 
    state(CLOSED), 
    seq_num(0), 
    ack_num(0), 
    window_size(WINDOW_SIZE), 
    is_server(false) {
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));
}

// 析构函数
Connection::~Connection() {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

// 初始化连接
int Connection::init(bool as_server, int port) {
    is_server = as_server;
    
    // 创建UDP套接字
    socket_fd = create_udp_socket();
    if (socket_fd < 0) {
        return ERROR_SOCKET;
    }
    
    // 绑定到本地端口
    if (bind_socket(socket_fd, port) < 0) {
        close(socket_fd);
        socket_fd = -1;
        return ERROR_BIND;
    }
    
    // 设置连接状态
    state = is_server ? LISTEN : CLOSED;
    
    return SUCCESS;
}

// 连接到服务器
int Connection::connect_to(const std::string& server_ip, int port) {
    // 设置远程地址
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &remote_addr.sin_addr) <= 0) {
        return ERROR_CONNECTION;
    }
    
    // 更新状态为SYN_SENT
    state = SYN_SENT;
    
    // TODO: 实现三次握手
    
    return SUCCESS;
}

// 接受连接（服务器端）
int Connection::accept_connection() {
    // TODO: 实现接受连接的逻辑
    
    return SUCCESS;
}

// 关闭连接
int Connection::close_connection() {
    // TODO: 实现四次挥手关闭连接
    
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
    
    state = CLOSED;
    return SUCCESS;
}

// 发送数据
int Connection::send_data(const char* data, size_t length) {
    // TODO: 实现可靠数据传输
    
    return SUCCESS;
}

// 接收数据
int Connection::receive_data(char* buffer, size_t max_length) {
    // TODO: 实现可靠数据接收
    
    return SUCCESS;
}

// 获取连接状态
ConnectionState Connection::get_state() const {
    return state;
}

// 检查连接是否已建立
bool Connection::is_connected() const {
    return state == ESTABLISHED;
}

// 获取套接字文件描述符
int Connection::get_socket_fd() const {
    return socket_fd;
}

// 获取远程地址
struct sockaddr_in Connection::get_remote_addr() const {
    return remote_addr;
}

// 更新连接状态
void Connection::update_state(ConnectionState new_state) {
    state = new_state;
}

// 创建UDP套接字
int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "创建UDP套接字失败" << std::endl;
    }
    return sockfd;
}

// 绑定套接字到端口
int bind_socket(int socket_fd, int port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    
    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "绑定套接字到端口 " << port << " 失败" << std::endl;
        return ERROR_BIND;
    }
    
    return SUCCESS;
}

// 设置套接字为非阻塞模式
int set_non_blocking(int socket_fd, bool non_blocking) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return ERROR_SOCKET;
    }
    
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(socket_fd, F_SETFL, flags) < 0) {
        return ERROR_SOCKET;
    }
    
    return SUCCESS;
}

// 发送数据包
int send_packet(int socket_fd, const struct sockaddr_in* addr, 
               const char* data, size_t length) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int bytes_sent = sendto(socket_fd, data, length, 0, 
                          (struct sockaddr*)addr, addr_len);
    
    if (bytes_sent < 0) {
        std::cerr << "发送数据包失败" << std::endl;
        return ERROR_SEND;
    }
    
    return bytes_sent;
}

// 接收数据包
int receive_packet(int socket_fd, struct sockaddr_in* addr, 
                  char* buffer, size_t max_length) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int bytes_received = recvfrom(socket_fd, buffer, max_length, 0, 
                                (struct sockaddr*)addr, &addr_len);
    
    if (bytes_received < 0) {
        // 非阻塞模式下的EAGAIN是正常的，不是错误
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "接收数据包失败" << std::endl;
            return ERROR_RECEIVE;
        }
        return 0; // 无数据可读
    }
    
    return bytes_received;
}