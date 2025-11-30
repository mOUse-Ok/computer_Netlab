// utils.cpp - 工具函数模块源文件
// 实现各种辅助函数，提供通用功能支持

#include "utils.h"
#include "reliable_transport.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#endif

// 全局统计数据结构
struct Stats {
    std::unordered_map<std::string, uint64_t> counters;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> timers;
};

static Stats stats;
static std::mt19937_64 rng;

// 时间工具函数实现
std::chrono::steady_clock::time_point get_current_time() {
    return std::chrono::steady_clock::now();
}

double calculate_elapsed_time(const std::chrono::steady_clock::time_point& start_time, const std::chrono::steady_clock::time_point& end_time) {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    return duration.count() / 1000000.0; // 转换为秒
}

void sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// 字符串工具函数实现
std::string uint32_to_string(uint32_t value) {
    return std::to_string(value);
}

uint32_t string_to_uint32(const std::string& str) {
    try {
        return std::stoul(str);
    } catch (const std::exception& e) {
        log_error("字符串转换为uint32失败: " + std::string(e.what()));
        return 0;
    }
}

std::string uint16_to_string(uint16_t value) {
    return std::to_string(value);
}

uint16_t string_to_uint16(const std::string& str) {
    try {
        return static_cast<uint16_t>(std::stoul(str));
    } catch (const std::exception& e) {
        log_error("字符串转换为uint16失败: " + std::string(e.what()));
        return 0;
    }
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string trim_string(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    
    return str.substr(start, end - start + 1);
}

// 网络地址转换工具函数实现
std::string ip_to_string(const struct sockaddr_in& addr) {
    char ip_str[INET_ADDRSTRLEN];
#ifdef _WIN32
    inet_ntop(AF_INET, &(addr.sin_addr), ip_str, INET_ADDRSTRLEN);
#else
    inet_ntop(AF_INET, &(addr.sin_addr), ip_str, sizeof(ip_str));
#endif
    return std::string(ip_str) + ":" + std::to_string(ntohs(addr.sin_port));
}

bool string_to_ip(const std::string& ip_str, struct sockaddr_in& addr, uint16_t port) {
    // 检查是否包含端口号
    size_t colon_pos = ip_str.find(':');
    std::string ip_part = ip_str;
    uint16_t port_part = port;
    
    if (colon_pos != std::string::npos) {
        ip_part = ip_str.substr(0, colon_pos);
        try {
            port_part = static_cast<uint16_t>(std::stoul(ip_str.substr(colon_pos + 1)));
        } catch (const std::exception& e) {
            log_error("无效的端口号: " + ip_str.substr(colon_pos + 1));
            return false;
        }
    }
    
    // 初始化地址结构
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_part);
    
    // 转换IP地址
#ifdef _WIN32
    if (inet_pton(AF_INET, ip_part.c_str(), &(addr.sin_addr)) != 1) {
        log_error("无效的IP地址: " + ip_part);
        return false;
    }
#else
    if (inet_pton(AF_INET, ip_part.c_str(), &(addr.sin_addr)) != 1) {
        log_error("无效的IP地址: " + ip_part);
        return false;
    }
#endif
    
    return true;
}

// 调试和日志工具函数实现
void log_debug(const std::string& message) {
    std::cout << "[DEBUG] " << message << std::endl;
}

void log_info(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void log_error(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void print_hex_dump(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    char ascii[17];
    size_t i, j;
    
    ascii[16] = '\0';
    
    for (i = 0; i < size; ++i) {
        if (i % 16 == 0) {
            if (i != 0) {
                printf("  %s\n", ascii);
            }
            printf("%04zX: ", i);
        }
        
        printf("%02X ", bytes[i]);
        
        if (bytes[i] >= 32 && bytes[i] <= 126) {
            ascii[i % 16] = bytes[i];
        } else {
            ascii[i % 16] = '.';
        }
    }
    
    while (i % 16 != 0) {
        printf("   ");
        ascii[i % 16] = ' ';
        ++i;
    }
    
    printf("  %s\n", ascii);
}

// 文件操作工具函数实现
bool file_exists(const std::string& file_path) {
    std::ifstream file(file_path);
    return file.good();
}

size_t get_file_size(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        return 0;
    }
    return file.tellg();
}

bool read_file(const std::string& file_path, std::vector<uint8_t>& buffer) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        log_error("无法打开文件: " + file_path);
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    buffer.resize(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    
    if (!file) {
        log_error("读取文件失败: " + file_path);
        return false;
    }
    
    return true;
}

bool write_file(const std::string& file_path, const std::vector<uint8_t>& buffer) {
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        log_error("无法创建文件: " + file_path);
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    
    if (!file) {
        log_error("写入文件失败: " + file_path);
        return false;
    }
    
    return true;
}

// 内存操作工具函数实现
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        log_error("内存分配失败");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void* safe_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        log_error("内存重新分配失败");
        free(ptr);
        exit(EXIT_FAILURE);
    }
    return new_ptr;
}

void safe_free(void** ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = nullptr;
    }
}

template <typename T>
std::string to_string(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// 错误处理工具函数实现
std::string get_last_error_string() {
#ifdef _WIN32
    DWORD error_code = GetLastError();
    char* error_message = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&error_message),
        0,
        nullptr
    );
    std::string message(error_message ? error_message : "未知错误");
    LocalFree(error_message);
    return message;
#else
    return std::string(strerror(errno));
#endif
}

void handle_error(const std::string& operation, bool critical) {
    std::string error_msg = operation + " 失败: " + get_last_error_string();
    log_error(error_msg);
    
    if (critical) {
        exit(EXIT_FAILURE);
    }
}

// 统计和性能工具函数实现
void start_timer(const std::string& timer_name) {
    stats.timers[timer_name] = get_current_time();
}

double stop_timer(const std::string& timer_name) {
    auto it = stats.timers.find(timer_name);
    if (it == stats.timers.end()) {
        log_error("计时器不存在: " + timer_name);
        return 0.0;
    }
    
    double elapsed = calculate_elapsed_time(it->second, get_current_time());
    stats.timers.erase(it);
    return elapsed;
}

void increment_counter(const std::string& counter_name, uint64_t increment) {
    stats.counters[counter_name] += increment;
}

uint64_t get_counter_value(const std::string& counter_name) {
    auto it = stats.counters.find(counter_name);
    if (it == stats.counters.end()) {
        return 0;
    }
    return it->second;
}

void print_stats() {
    log_info("====== 统计信息 ======");
    
    for (const auto& pair : stats.counters) {
        log_info(pair.first + ": " + std::to_string(pair.second));
    }
    
    if (!stats.timers.empty()) {
        log_info("未停止的计时器:");
        for (const auto& pair : stats.timers) {
            log_info("  " + pair.first);
        }
    }
    
    log_info("======================");
}

// 随机数生成工具函数实现
void initialize_random() {
    std::random_device rd;
    rng.seed(rd());
}

uint32_t generate_random_uint32(uint32_t min, uint32_t max) {
    std::uniform_int_distribution<uint32_t> dist(min, max);
    return dist(rng);
}

double generate_random_double(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng);
}

bool generate_random_bool(double probability) {
    std::bernoulli_distribution dist(probability);
    return dist(rng);
}

// 显式实例化常用的to_string模板
template std::string to_string<int>(const int& value);
template std::string to_string<long>(const long& value);
template std::string to_string<long long>(const long long& value);
template std::string to_string<unsigned int>(const unsigned int& value);
template std::string to_string<unsigned long>(const unsigned long& value);
template std::string to_string<unsigned long long>(const unsigned long long& value);
template std::string to_string<float>(const float& value);
template std::string to_string<double>(const double& value);
template std::string to_string<bool>(const bool& value);