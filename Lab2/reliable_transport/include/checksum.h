#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint>
#include <cstddef>
#include <cstdbool>
#include "packet.h"

// ==================== Internet校验和函数 ====================

/**
 * 计算Internet校验和（RFC 791标准）
 * 
 * 算法描述：
 * 1. 将数据按16位（2字节）分组，按网络字节序组合
 * 2. 对所有16位字进行32位累加
 * 3. 累加溢出时进行循环进位（将高16位加回低16位）
 * 4. 最后对累加和取反（一补数）得到校验和
 * 
 * 验证过程：
 * 发送方：计算校验和，放入报文中发送
 * 接收方：重新计算校验和（包括报文中的校验和字段），
 *        如果结果为0xFFFF则验证通过
 * 
 * @param data 数据指针（指向要计算校验和的数据）
 * @param length 数据长度（字节数）
 * @return 计算得到的16位校验和
 * 
 * 举例：
 * 数据 = [0x45, 0x00, 0x00, 0x3C]
 * 第1个16位字 = 0x4500（网络字节序）
 * 第2个16位字 = 0x003C（网络字节序）
 * 和 = 0x4500 + 0x003C = 0x453C
 * 校验和 = ~0x453C = 0xBAC3
 */
uint16_t calculate_checksum(const void* data, size_t length);

/**
 * 验证数据的校验和
 * 
 * 验证方法：
 * 1. 将接收到的数据（包括其中的校验和字段）进行校验和计算
 * 2. 如果结果为0xFFFF（全1），则验证通过
 * 3. 这是因为：(原校验和) + (数据校验和) = 0xFFFF
 * 
 * @param data 数据指针（包含校验和字段）
 * @param length 数据长度
 * @param checksum 期望的校验和值
 * @return 校验通过返回true，失败返回false
 */
bool verify_checksum(const void* data, size_t length, uint16_t checksum);

// ==================== 针对Frame的校验和函数 ====================

/**
 * 计算帧的校验和（包括头部和数据）
 * 
 * 计算范围：
 * - 帧头部（除checksum字段外的所有字段）
 * - 数据部分
 * 
 * @param frame 帧指针
 * @return 计算得到的16位校验和
 */
uint16_t calculate_frame_checksum(const Frame* frame);

/**
 * 验证帧的校验和
 * 
 * 验证过程：
 * 1. 计算帧中所有字段（包括校验和）的校验和
 * 2. 如果结果为0xFFFF，则验证通过
 * 
 * @param frame 帧指针
 * @return 校验通过返回true，失败返回false
 */
bool verify_frame_checksum(const Frame* frame);

// ==================== 其他工具函数 ====================

/**
 * 计算缓冲区中指定范围的校验和
 * @param buffer 缓冲区指针
 * @param start 起始位置（字节偏移）
 * @param len 数据长度（字节数）
 * @return 计算得到的校验和
 */
uint16_t calculate_checksum_range(const uint8_t* buffer, size_t start, size_t len);

#endif // CHECKSUM_H
