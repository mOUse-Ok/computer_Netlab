// checksum.h - 校验和模块头文件
// 定义校验和计算相关函数

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint>
#include <cstddef>

// 计算16位校验和（类似TCP/IP校验和）
uint16_t calculate_checksum(const void* data, size_t length);

// 计算数据块的校验和
uint16_t calculate_block_checksum(const void* data, size_t length);

// 计算并验证校验和
bool verify_checksum(const void* data, size_t length, uint16_t expected_checksum);

// 计算 Fletcher校验和
uint16_t calculate_fletcher_checksum(const void* data, size_t length);

// 计算 Adler32校验和
uint32_t calculate_adler32_checksum(const void* data, size_t length);

// 计算简单的XOR校验和
uint8_t calculate_xor_checksum(const void* data, size_t length);

// 辅助函数：更新校验和（用于增量计算）
uint16_t update_checksum(uint16_t current_checksum, const void* new_data, size_t new_length);

#endif // CHECKSUM_H