// utils.h - 工具函数模块头文件
// 定义各种辅助函数，提供通用功能支持

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <chrono>

// 时间工具函数
extern std::chrono::steady_clock::time_point get_current_time();
extern double calculate_elapsed_time(const std::chrono::steady_clock::time_point& start_time, const std::chrono::steady_clock::time_point& end_time);
extern void sleep_ms(int milliseconds);

// 字符串工具函数
extern std::string uint32_to_string(uint32_t value);
extern uint32_t string_to_uint32(const std::string& str);
extern std::string uint16_to_string(uint16_t value);
extern uint16_t string_to_uint16(const std::string& str);
extern std::vector<std::string> split_string(const std::string& str, char delimiter);
extern std::string trim_string(const std::string& str);

// 网络地址转换工具函数
extern std::string ip_to_string(const struct sockaddr_in& addr);
extern bool string_to_ip(const std::string& ip_str, struct sockaddr_in& addr, uint16_t port = DEFAULT_PORT);

// 调试和日志工具函数
extern void log_debug(const std::string& message);
extern void log_info(const std::string& message);
extern void log_error(const std::string& message);
extern void print_hex_dump(const void* data, size_t size);

// 文件操作工具函数
extern bool file_exists(const std::string& file_path);
extern size_t get_file_size(const std::string& file_path);
extern bool read_file(const std::string& file_path, std::vector<uint8_t>& buffer);
extern bool write_file(const std::string& file_path, const std::vector<uint8_t>& buffer);

// 内存操作工具函数
extern void* safe_malloc(size_t size);
extern void* safe_realloc(void* ptr, size_t size);
extern void safe_free(void** ptr);

template <typename T>
extern std::string to_string(const T& value);

// 错误处理工具函数
extern std::string get_last_error_string();
extern void handle_error(const std::string& operation, bool critical = false);

// 统计和性能工具函数
extern void start_timer(const std::string& timer_name);
extern double stop_timer(const std::string& timer_name);
extern void increment_counter(const std::string& counter_name, uint64_t increment = 1);
extern uint64_t get_counter_value(const std::string& counter_name);
extern void print_stats();

// 随机数生成工具函数
extern void initialize_random();
extern uint32_t generate_random_uint32(uint32_t min, uint32_t max);
extern double generate_random_double(double min, double max);
extern bool generate_random_bool(double probability = 0.5);

#endif // UTILS_H