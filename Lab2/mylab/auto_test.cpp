/**
 * ============================================================================
 * 自动化测试程序：auto_test.cpp
 * ============================================================================
 * 描述：自动测试不同参数组合对传输性能的影响
 * 功能：
 *   1. 修改配置文件
 *   2. 重新编译服务端和客户端
 *   3. 自动启动服务端和客户端
 *   4. 收集测试结果
 *   5. 生成测试报告
 * 
 * 编译：g++ -o auto_test.exe auto_test.cpp -std=c++11
 * 运行：auto_test.exe
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <windows.h>
#include <direct.h>

// 测试结果结构
struct TestResult {
    int ssthresh;
    int windowSize;
    int lossRate;
    double transmissionTime;
    double throughput;
    int totalPackets;
    int retransmissions;
    bool success;
};

// 全局测试结果存储
std::vector<TestResult> g_results;

// 生成配置文件
bool generateConfig(int ssthresh, int windowSize, int lossRate) {
    std::ofstream configFile("config.h");
    if (!configFile.is_open()) {
        std::cerr << "Error: Cannot create config.h" << std::endl;
        return false;
    }
    
    configFile << R"(/**
 * ============================================================================
 * 配置文件：config.h (自动测试生成)
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// 网络连接基础参数
#define PORT 8888
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"

// 协议数据包参数
#define HEADER_SIZE 20
#define MAX_PACKET_SIZE 8192
#define MAX_DATA_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)

// 滑动窗口与流量控制参数
#define DEFAULT_WINDOW_SIZE )" << windowSize << R"(
#define FIXED_WINDOW_SIZE )" << windowSize << R"(
#define MSS MAX_DATA_SIZE

// 超时与重传参数
#define TIMEOUT_MS 3000
#define MAX_RETRIES 3
#define TIME_WAIT_MS 4000
#define CONNECTION_TIMEOUT_MS 500
#define SACK_TIMEOUT_MS 500
#define MAX_SACK_BLOCKS 4

// TCP RENO 拥塞控制参数
#define INITIAL_CWND 1
#define INITIAL_SSTHRESH )" << ssthresh << R"(
#define MIN_SSTHRESH 2
#define DUP_ACK_THRESHOLD 3

// 丢包模拟参数
#define SIMULATE_LOSS_ENABLED )" << (lossRate > 0 ? "true" : "false") << R"(
#define SIMULATE_LOSS_RATE )" << lossRate << R"(
#define SIMULATE_DELAY_ENABLED true
#define SIMULATE_DELAY_MS 5

#endif // CONFIG_H
)";
    
    configFile.close();
    return true;
}

// 编译程序
bool compilePrograms() {
    std::cout << "  Compiling server.cpp..." << std::endl;
    int ret1 = system("g++ -o server.exe server.cpp -lws2_32 -std=c++11 2>nul");
    
    std::cout << "  Compiling client.cpp..." << std::endl;
    int ret2 = system("g++ -o client.exe client.cpp -lws2_32 -std=c++11 2>nul");
    
    return (ret1 == 0 && ret2 == 0);
}

// 解析服务端日志获取结果
TestResult parseServerLog(int ssthresh, int windowSize, int lossRate) {
    TestResult result;
    result.ssthresh = ssthresh;
    result.windowSize = windowSize;
    result.lossRate = lossRate;
    result.transmissionTime = 0;
    result.throughput = 0;
    result.totalPackets = 0;
    result.retransmissions = 0;
    result.success = false;
    
    std::ifstream logFile("server.txt");
    if (!logFile.is_open()) {
        return result;
    }
    
    std::string line;
    while (std::getline(logFile, line)) {
        // 查找 "Transmission Time:"
        size_t pos = line.find("Transmission Time:");
        if (pos != std::string::npos) {
            std::string timeStr = line.substr(pos + 18);
            sscanf(timeStr.c_str(), "%lf", &result.transmissionTime);
        }
        
        // 查找 "Average Throughput:"
        pos = line.find("Average Throughput:");
        if (pos != std::string::npos) {
            std::string tpStr = line.substr(pos + 19);
            sscanf(tpStr.c_str(), "%lf", &result.throughput);
            result.success = true;
        }
        
        // 查找 "Total Packets Received:"
        pos = line.find("Total Packets Received:");
        if (pos != std::string::npos) {
            std::string pktStr = line.substr(pos + 23);
            sscanf(pktStr.c_str(), "%d", &result.totalPackets);
        }
    }
    
    logFile.close();
    return result;
}

// 输出测试结果表格
void printResultsTable() {
    std::cout << "\n" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << "                     测试结果汇总表" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << std::endl;
    
    // 表格1：不同窗口大小组合测试结果
    std::cout << "【表1】不同发送窗口和接收窗口大小对传输性能的影响 (丢包率=5%)" << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;
    std::cout << "| ssthresh | 窗口大小 | 传输时间(s) | 平均吞吐率(KB/s) |" << std::endl;
    std::cout << "|----------|----------|-------------|------------------|" << std::endl;
    
    for (const auto& r : g_results) {
        if (r.lossRate == 5 && r.success) {
            printf("|    %2d    |    %2d    |   %7.3f   |     %8.2f     |\n",
                   r.ssthresh, r.windowSize, r.transmissionTime, r.throughput);
        }
    }
    std::cout << "-------------------------------------------------------------" << std::endl;
    std::cout << std::endl;
    
    // 表格2：不同丢包率测试结果
    std::cout << "【表2】不同丢包率对传输性能的影响 (ssthresh=16, 窗口大小=16)" << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;
    std::cout << "| 丢包率(%) | 传输时间(s) | 平均吞吐率(KB/s) |" << std::endl;
    std::cout << "|-----------|-------------|------------------|" << std::endl;
    
    for (const auto& r : g_results) {
        if (r.ssthresh == 16 && r.windowSize == 16 && r.success) {
            printf("|     %2d    |   %7.3f   |     %8.2f     |\n",
                   r.lossRate, r.transmissionTime, r.throughput);
        }
    }
    std::cout << "-------------------------------------------------------------" << std::endl;
}

// 保存结果到文件
void saveResultsToFile() {
    std::ofstream outFile("test_results\\performance_report.txt");
    if (!outFile.is_open()) {
        std::cerr << "Error: Cannot create report file" << std::endl;
        return;
    }
    
    outFile << "=============================================================" << std::endl;
    outFile << "              传输性能测试报告" << std::endl;
    outFile << "=============================================================" << std::endl;
    outFile << "测试文件: 1.jpg" << std::endl;
    outFile << std::endl;
    
    // 表格1
    outFile << "【表1】不同发送窗口和接收窗口大小对传输性能的影响 (丢包率=5%)" << std::endl;
    outFile << "-------------------------------------------------------------" << std::endl;
    outFile << "| ssthresh | 窗口大小 | 传输时间(s) | 平均吞吐率(KB/s) |" << std::endl;
    outFile << "|----------|----------|-------------|------------------|" << std::endl;
    
    for (const auto& r : g_results) {
        if (r.lossRate == 5 && r.success) {
            char buf[256];
            sprintf(buf, "|    %2d    |    %2d    |   %7.3f   |     %8.2f     |",
                   r.ssthresh, r.windowSize, r.transmissionTime, r.throughput);
            outFile << buf << std::endl;
        }
    }
    outFile << "-------------------------------------------------------------" << std::endl;
    outFile << std::endl;
    
    // 表格2
    outFile << "【表2】不同丢包率对传输性能的影响 (ssthresh=16, 窗口大小=16)" << std::endl;
    outFile << "-------------------------------------------------------------" << std::endl;
    outFile << "| 丢包率(%) | 传输时间(s) | 平均吞吐率(KB/s) |" << std::endl;
    outFile << "|-----------|-------------|------------------|" << std::endl;
    
    for (const auto& r : g_results) {
        if (r.ssthresh == 16 && r.windowSize == 16 && r.success) {
            char buf[256];
            sprintf(buf, "|     %2d    |   %7.3f   |     %8.2f     |",
                   r.lossRate, r.transmissionTime, r.throughput);
            outFile << buf << std::endl;
        }
    }
    outFile << "-------------------------------------------------------------" << std::endl;
    
    // CSV格式
    outFile << std::endl;
    outFile << "=============================================================" << std::endl;
    outFile << "                  原始数据 (CSV格式)" << std::endl;
    outFile << "=============================================================" << std::endl;
    outFile << "ssthresh,window_size,loss_rate,transmission_time,throughput" << std::endl;
    
    for (const auto& r : g_results) {
        if (r.success) {
            outFile << r.ssthresh << "," << r.windowSize << "," << r.lossRate << ","
                   << r.transmissionTime << "," << r.throughput << std::endl;
        }
    }
    
    outFile.close();
    std::cout << "Report saved to: test_results\\performance_report.txt" << std::endl;
}

int main() {
    std::cout << "=============================================================" << std::endl;
    std::cout << "            传输性能自动化测试程序" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << std::endl;
    std::cout << "测试参数组合:" << std::endl;
    std::cout << "  - 发送窗口 (ssthresh): 8, 16, 32" << std::endl;
    std::cout << "  - 接收窗口 (FIXED_WINDOW_SIZE): 8, 16, 32" << std::endl;
    std::cout << "  - 丢包率: 0%, 5%, 10%" << std::endl;
    std::cout << "  - 测试文件: 1.jpg" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << std::endl;
    
    // 创建结果目录
    _mkdir("test_results");
    
    // 定义测试参数
    int ssthreshValues[] = {8, 16, 32};
    int windowValues[] = {8, 16, 32};
    int lossRates[] = {0, 5, 10};
    
    int testNum = 0;
    
    std::cout << "=============================================================" << std::endl;
    std::cout << " 测试组1: 不同发送窗口和接收窗口大小 (丢包率=5%)" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    // 测试不同窗口大小组合 (丢包率固定为5%)
    for (int ss : ssthreshValues) {
        for (int win : windowValues) {
            testNum++;
            std::cout << std::endl;
            std::cout << "[Test " << testNum << "] ssthresh=" << ss 
                     << ", window=" << win << ", loss=5%" << std::endl;
            std::cout << "-------------------------------------------------------------" << std::endl;
            
            // 生成配置
            if (!generateConfig(ss, win, 5)) {
                std::cerr << "Failed to generate config!" << std::endl;
                continue;
            }
            
            // 编译
            if (!compilePrograms()) {
                std::cerr << "Compilation failed!" << std::endl;
                continue;
            }
            
            std::cout << std::endl;
            std::cout << "*** 请在新终端中执行以下步骤 ***" << std::endl;
            std::cout << "  1. 运行 server.exe, 输入文件名: test" << testNum << ".jpg" << std::endl;
            std::cout << "  2. 运行 client.exe, 输入文件名: 1.jpg" << std::endl;
            std::cout << "  3. 等待传输完成" << std::endl;
            std::cout << std::endl;
            std::cout << "完成后按 Enter 继续下一个测试..." << std::endl;
            std::cin.get();
            
            // 解析结果
            TestResult result = parseServerLog(ss, win, 5);
            if (result.success) {
                g_results.push_back(result);
                std::cout << "Result: Time=" << result.transmissionTime 
                         << "s, Throughput=" << result.throughput << "KB/s" << std::endl;
            } else {
                std::cout << "Warning: Could not parse results from server.txt" << std::endl;
                std::cout << "Please enter the results manually:" << std::endl;
                std::cout << "  Transmission Time (seconds): ";
                std::cin >> result.transmissionTime;
                std::cout << "  Average Throughput (KB/s): ";
                std::cin >> result.throughput;
                std::cin.ignore();
                result.success = true;
                g_results.push_back(result);
            }
        }
    }
    
    std::cout << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << " 测试组2: 不同丢包率 (ssthresh=16, window=16)" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    // 测试不同丢包率 (窗口大小固定为16)
    for (int loss : lossRates) {
        // 跳过丢包率5%的测试（已在上面测试过）
        if (loss == 5) {
            // 检查是否已有结果
            bool found = false;
            for (const auto& r : g_results) {
                if (r.ssthresh == 16 && r.windowSize == 16 && r.lossRate == 5) {
                    found = true;
                    break;
                }
            }
            if (found) continue;
        }
        
        testNum++;
        std::cout << std::endl;
        std::cout << "[Test " << testNum << "] ssthresh=16, window=16, loss=" << loss << "%" << std::endl;
        std::cout << "-------------------------------------------------------------" << std::endl;
        
        // 生成配置
        if (!generateConfig(16, 16, loss)) {
            std::cerr << "Failed to generate config!" << std::endl;
            continue;
        }
        
        // 编译
        if (!compilePrograms()) {
            std::cerr << "Compilation failed!" << std::endl;
            continue;
        }
        
        std::cout << std::endl;
        std::cout << "*** 请在新终端中执行以下步骤 ***" << std::endl;
        std::cout << "  1. 运行 server.exe, 输入文件名: test" << testNum << ".jpg" << std::endl;
        std::cout << "  2. 运行 client.exe, 输入文件名: 1.jpg" << std::endl;
        std::cout << "  3. 等待传输完成" << std::endl;
        std::cout << std::endl;
        std::cout << "完成后按 Enter 继续下一个测试..." << std::endl;
        std::cin.get();
        
        // 解析结果
        TestResult result = parseServerLog(16, 16, loss);
        if (result.success) {
            g_results.push_back(result);
            std::cout << "Result: Time=" << result.transmissionTime 
                     << "s, Throughput=" << result.throughput << "KB/s" << std::endl;
        } else {
            std::cout << "Warning: Could not parse results from server.txt" << std::endl;
            std::cout << "Please enter the results manually:" << std::endl;
            std::cout << "  Transmission Time (seconds): ";
            std::cin >> result.transmissionTime;
            std::cout << "  Average Throughput (KB/s): ";
            std::cin >> result.throughput;
            std::cin.ignore();
            result.success = true;
            g_results.push_back(result);
        }
    }
    
    // 输出结果
    printResultsTable();
    saveResultsToFile();
    
    std::cout << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << "                    测试完成!" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    return 0;
}
