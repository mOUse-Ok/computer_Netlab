// main.cpp - 可靠传输协议主程序入口
// 实现命令行参数解析和程序启动逻辑

#include "reliable_transport.h"
#include <iostream>
#include <cstdlib>

// 打印使用帮助信息
void print_usage() {
    std::cout << "使用方法：" << std::endl;
    std::cout << "  服务器模式: ./reliable_transport -s [端口号]" << std::endl;
    std::cout << "  客户端模式: ./reliable_transport -c [服务器IP] [端口号]" << std::endl;
    std::cout << "参数说明：" << std::endl;
    std::cout << "  -s: 以服务器模式运行" << std::endl;
    std::cout << "  -c: 以客户端模式运行" << std::endl;
    std::cout << "  端口号: 可选，默认为 " << DEFAULT_PORT << std::endl;
}

// 主函数
int main(int argc, char* argv[]) {
    // 检查命令行参数
    if (argc < 2) {
        print_usage();
        return 1;
    }

    // 解析命令行参数
    std::string mode = argv[1];
    
    // 服务器模式
    if (mode == "-s") {
        int port = DEFAULT_PORT;
        if (argc > 2) {
            port = std::atoi(argv[2]);
        }
        std::cout << "以服务器模式启动，监听端口: " << port << std::endl;
        return start_server(port);
    }
    // 客户端模式
    else if (mode == "-c") {
        if (argc < 3) {
            std::cerr << "错误: 客户端模式需要提供服务器IP地址" << std::endl;
            print_usage();
            return 1;
        }
        std::string server_ip = argv[2];
        int port = DEFAULT_PORT;
        if (argc > 3) {
            port = std::atoi(argv[3]);
        }
        std::cout << "以客户端模式启动，连接到: " << server_ip << ":" << port << std::endl;
        return start_client(server_ip, port);
    }
    // 参数错误
    else {
        std::cerr << "错误: 未知的运行模式" << std::endl;
        print_usage();
        return 1;
    }
}

// 服务器启动函数（待实现）
int start_server(int port) {
    std::cout << "服务器启动函数待实现..." << std::endl;
    return 0;
}

// 客户端启动函数（待实现）
int start_client(const std::string& server_ip, int port) {
    std::cout << "客户端启动函数待实现..." << std::endl;
    return 0;
}