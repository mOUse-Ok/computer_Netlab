/**
 * ============================================================================
 * 结果汇总程序：collect_results.cpp
 * ============================================================================
 * 描述：手动输入测试结果，自动生成汇总表格
 * 编译：g++ -o collect_results.exe collect_results.cpp -std=c++11
 * 运行：collect_results.exe
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

struct TestResult {
    int ssthresh;
    int windowSize;
    int lossRate;
    double transmissionTime;
    double throughput;
};

int main() {
    std::cout << "=============================================================" << std::endl;
    std::cout << "            传输性能测试结果汇总程序" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << std::endl;
    
    std::vector<TestResult> results;
    
    // ========== 测试组1: 不同窗口大小 (丢包率5%) ==========
    std::cout << "【测试组1】不同发送窗口和接收窗口大小 (丢包率=5%)" << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;
    
    int ssthreshValues[] = {8, 16, 32};
    int windowValues[] = {8, 16, 32};
    
    for (int ss : ssthreshValues) {
        for (int win : windowValues) {
            TestResult r;
            r.ssthresh = ss;
            r.windowSize = win;
            r.lossRate = 5;
            
            std::cout << std::endl;
            std::cout << "配置: ssthresh=" << ss << ", window=" << win << ", loss=5%" << std::endl;
            std::cout << "请输入传输时间 (秒): ";
            std::cin >> r.transmissionTime;
            std::cout << "请输入平均吞吐率 (KB/s): ";
            std::cin >> r.throughput;
            
            results.push_back(r);
        }
    }
    
    // ========== 测试组2: 不同丢包率 (窗口16) ==========
    std::cout << std::endl;
    std::cout << "【测试组2】不同丢包率 (ssthresh=16, 窗口大小=16)" << std::endl;
    std::cout << "-------------------------------------------------------------" << std::endl;
    
    int lossRates[] = {0, 10};  // 5%的结果已在上面录入
    
    for (int loss : lossRates) {
        TestResult r;
        r.ssthresh = 16;
        r.windowSize = 16;
        r.lossRate = loss;
        
        std::cout << std::endl;
        std::cout << "配置: ssthresh=16, window=16, loss=" << loss << "%" << std::endl;
        std::cout << "请输入传输时间 (秒): ";
        std::cin >> r.transmissionTime;
        std::cout << "请输入平均吞吐率 (KB/s): ";
        std::cin >> r.throughput;
        
        results.push_back(r);
    }
    
    // ========== 输出结果表格 ==========
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << "                     测试结果汇总表" << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << std::endl;
    
    // 表格1: 不同窗口大小
    std::cout << "【表1】不同发送窗口(ssthresh)和接收窗口大小对传输性能的影响" << std::endl;
    std::cout << "       (丢包率=5%, 测试文件: 1.jpg)" << std::endl;
    std::cout << "┌──────────┬──────────┬─────────────┬──────────────────┐" << std::endl;
    std::cout << "│ ssthresh │ 窗口大小 │ 传输时间(s) │ 平均吞吐率(KB/s) │" << std::endl;
    std::cout << "├──────────┼──────────┼─────────────┼──────────────────┤" << std::endl;
    
    for (const auto& r : results) {
        if (r.lossRate == 5) {
            std::cout << "│    " << std::setw(2) << r.ssthresh 
                     << "    │    " << std::setw(2) << r.windowSize 
                     << "    │   " << std::fixed << std::setprecision(3) << std::setw(7) << r.transmissionTime 
                     << "   │     " << std::setw(8) << std::setprecision(2) << r.throughput << "     │" << std::endl;
        }
    }
    std::cout << "└──────────┴──────────┴─────────────┴──────────────────┘" << std::endl;
    std::cout << std::endl;
    
    // 表格2: 不同丢包率
    std::cout << "【表2】不同丢包率对传输性能的影响" << std::endl;
    std::cout << "       (ssthresh=16, 窗口大小=16, 测试文件: 1.jpg)" << std::endl;
    std::cout << "┌───────────┬─────────────┬──────────────────┐" << std::endl;
    std::cout << "│ 丢包率(%) │ 传输时间(s) │ 平均吞吐率(KB/s) │" << std::endl;
    std::cout << "├───────────┼─────────────┼──────────────────┤" << std::endl;
    
    // 先输出丢包率为0的
    for (const auto& r : results) {
        if (r.ssthresh == 16 && r.windowSize == 16 && r.lossRate == 0) {
            std::cout << "│     " << std::setw(2) << r.lossRate 
                     << "    │   " << std::fixed << std::setprecision(3) << std::setw(7) << r.transmissionTime 
                     << "   │     " << std::setw(8) << std::setprecision(2) << r.throughput << "     │" << std::endl;
        }
    }
    // 再输出丢包率为5的
    for (const auto& r : results) {
        if (r.ssthresh == 16 && r.windowSize == 16 && r.lossRate == 5) {
            std::cout << "│     " << std::setw(2) << r.lossRate 
                     << "    │   " << std::fixed << std::setprecision(3) << std::setw(7) << r.transmissionTime 
                     << "   │     " << std::setw(8) << std::setprecision(2) << r.throughput << "     │" << std::endl;
        }
    }
    // 最后输出丢包率为10的
    for (const auto& r : results) {
        if (r.ssthresh == 16 && r.windowSize == 16 && r.lossRate == 10) {
            std::cout << "│     " << std::setw(2) << r.lossRate 
                     << "    │   " << std::fixed << std::setprecision(3) << std::setw(7) << r.transmissionTime 
                     << "   │     " << std::setw(8) << std::setprecision(2) << r.throughput << "     │" << std::endl;
        }
    }
    std::cout << "└───────────┴─────────────┴──────────────────┘" << std::endl;
    
    // ========== 保存到文件 ==========
    std::ofstream outFile("performance_report.txt");
    if (outFile.is_open()) {
        outFile << "=============================================================" << std::endl;
        outFile << "              传输性能测试报告" << std::endl;
        outFile << "=============================================================" << std::endl;
        outFile << "测试文件: 1.jpg" << std::endl;
        outFile << std::endl;
        
        outFile << "【表1】不同发送窗口(ssthresh)和接收窗口大小对传输性能的影响" << std::endl;
        outFile << "       (丢包率=5%)" << std::endl;
        outFile << "-------------------------------------------------------------" << std::endl;
        outFile << "| ssthresh | 窗口大小 | 传输时间(s) | 平均吞吐率(KB/s) |" << std::endl;
        outFile << "|----------|----------|-------------|------------------|" << std::endl;
        
        for (const auto& r : results) {
            if (r.lossRate == 5) {
                outFile << "|    " << std::setw(2) << r.ssthresh 
                       << "    |    " << std::setw(2) << r.windowSize 
                       << "    |   " << std::fixed << std::setprecision(3) << std::setw(7) << r.transmissionTime 
                       << "   |     " << std::setw(8) << std::setprecision(2) << r.throughput << "     |" << std::endl;
            }
        }
        outFile << "-------------------------------------------------------------" << std::endl;
        outFile << std::endl;
        
        outFile << "【表2】不同丢包率对传输性能的影响" << std::endl;
        outFile << "       (ssthresh=16, 窗口大小=16)" << std::endl;
        outFile << "-------------------------------------------------------------" << std::endl;
        outFile << "| 丢包率(%) | 传输时间(s) | 平均吞吐率(KB/s) |" << std::endl;
        outFile << "|-----------|-------------|------------------|" << std::endl;
        
        // 按丢包率排序输出
        for (int loss : {0, 5, 10}) {
            for (const auto& r : results) {
                if (r.ssthresh == 16 && r.windowSize == 16 && r.lossRate == loss) {
                    outFile << "|     " << std::setw(2) << r.lossRate 
                           << "    |   " << std::fixed << std::setprecision(3) << std::setw(7) << r.transmissionTime 
                           << "   |     " << std::setw(8) << std::setprecision(2) << r.throughput << "     |" << std::endl;
                }
            }
        }
        outFile << "-------------------------------------------------------------" << std::endl;
        outFile << std::endl;
        
        // CSV格式
        outFile << "=============================================================" << std::endl;
        outFile << "              原始数据 (CSV格式)" << std::endl;
        outFile << "=============================================================" << std::endl;
        outFile << "ssthresh,window_size,loss_rate,transmission_time,throughput" << std::endl;
        for (const auto& r : results) {
            outFile << r.ssthresh << "," << r.windowSize << "," << r.lossRate << ","
                   << r.transmissionTime << "," << r.throughput << std::endl;
        }
        
        outFile.close();
        std::cout << std::endl;
        std::cout << "结果已保存到: performance_report.txt" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << "                    完成!" << std::endl;
    std::cout << "=============================================================" << std::endl;
    
    return 0;
}
