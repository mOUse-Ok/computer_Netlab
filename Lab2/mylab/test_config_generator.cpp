/**
 * ============================================================================
 * 测试配置生成器：test_config_generator.cpp
 * ============================================================================
 * 描述：根据指定参数生成新的 config.h 文件用于测试不同参数组合
 * 用法：test_config_generator.exe <ssthresh> <window_size> <loss_rate>
 *       例如：test_config_generator.exe 8 8 0
 *             生成 ssthresh=8, 接收窗口=8, 丢包率=0% 的配置
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <ssthresh> <window_size> <loss_rate>" << std::endl;
        std::cerr << "  ssthresh: Initial slow-start threshold (8, 16, 32)" << std::endl;
        std::cerr << "  window_size: Fixed window size (8, 16, 32)" << std::endl;
        std::cerr << "  loss_rate: Simulated packet loss rate (0, 5, 10)" << std::endl;
        return 1;
    }
    
    int ssthresh = atoi(argv[1]);
    int windowSize = atoi(argv[2]);
    int lossRate = atoi(argv[3]);
    
    std::cout << "Generating config.h with:" << std::endl;
    std::cout << "  INITIAL_SSTHRESH = " << ssthresh << std::endl;
    std::cout << "  FIXED_WINDOW_SIZE = " << windowSize << std::endl;
    std::cout << "  SIMULATE_LOSS_RATE = " << lossRate << "%" << std::endl;
    
    std::ofstream configFile("config.h");
    if (!configFile.is_open()) {
        std::cerr << "Error: Cannot create config.h" << std::endl;
        return 1;
    }
    
    configFile << R"(/**
 * ============================================================================
 * 配置文件：config.h
 * ============================================================================
 * 描述：集中管理所有与传输效率相关的可配置参数
 * 说明：
 *   - 所有参数都可以根据测试需要进行调整
 *   - 每个参数都包含：含义、修改方法、修改后可能出现的效果
 *   - 修改后需要重新编译 client.cpp 和 server.cpp
 * 
 * 作者：Lab2 Project
 * 日期：2025-12-10
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// 一、网络连接基础参数
// ============================================================================

#define PORT 8888
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"

// ============================================================================
// 二、协议数据包参数
// ============================================================================

#define HEADER_SIZE 20
#define MAX_PACKET_SIZE 8192
#define MAX_DATA_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)

// ============================================================================
// 三、滑动窗口与流量控制参数（重要！影响传输效率）
// ============================================================================

#define DEFAULT_WINDOW_SIZE )" << windowSize << R"(
#define FIXED_WINDOW_SIZE )" << windowSize << R"(
#define MSS MAX_DATA_SIZE

// ============================================================================
// 四、超时与重传参数（重要！影响可靠性和效率）
// ============================================================================

#define TIMEOUT_MS 3000
#define MAX_RETRIES 3
#define TIME_WAIT_MS 4000
#define CONNECTION_TIMEOUT_MS 500
#define SACK_TIMEOUT_MS 500
#define MAX_SACK_BLOCKS 4

// ============================================================================
// 五、TCP RENO 拥塞控制参数（重要！影响拥塞响应）
// ============================================================================

#define INITIAL_CWND 1
#define INITIAL_SSTHRESH )" << ssthresh << R"(
#define MIN_SSTHRESH 2
#define DUP_ACK_THRESHOLD 3

// ============================================================================
// 六、丢包模拟参数（用于测试和调试）
// ============================================================================

#define SIMULATE_LOSS_ENABLED )" << (lossRate > 0 ? "true" : "false") << R"(
#define SIMULATE_LOSS_RATE )" << lossRate << R"(
#define SIMULATE_DELAY_ENABLED true
#define SIMULATE_DELAY_MS 5

#endif // CONFIG_H
)";
    
    configFile.close();
    std::cout << "config.h generated successfully!" << std::endl;
    
    return 0;
}
