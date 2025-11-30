#include "checksum.h"
#include <cstdio>
#include <cstring>

// ==================== Internet校验和计算（RFC 791标准） ====================

/**
 * 计算Internet标准校验和
 * 
 * 算法步骤：
 * 1. 将数据按16位（2字节）分组
 * 2. 每两个字节组合成一个16位字（高字节在前，低字节在后，网络字节序）
 * 3. 将所有16位字累加成32位和
 * 4. 处理32位和的溢出：将高16位加到低16位
 * 5. 对最终的16位和取反（一补数）得到校验和
 * 
 * 溢出处理说明：
 * - 32位加法可能产生进位（第17位）
 * - 这些进位需要加回到16位结果中
 * - 重复此过程直到没有进位（最多循环一次）
 * 
 * 奇数长度处理：
 * - 如果数据长度为奇数，最后一个字节填充到高8位
 * - 低8位用0填充
 * 
 * @param data 数据指针
 * @param length 数据长度（字节数）
 * @return 16位校验和
 */
uint16_t calculate_checksum(const void* data, size_t length)
{
    // 输入验证
    if (data == NULL || length == 0) {
        return 0xFFFF;  // 空数据返回全1
    }

    const uint8_t* ptr = (const uint8_t*)data;
    uint32_t sum = 0;

    // ===== 步骤1-2：按16位字分组并累加 =====
    // 处理完整的16位字（偶数字节对）
    size_t i = 0;
    for (; i < length - 1; i += 2) {
        // 组合两个字节成16位字（网络字节序：高字节在前）
        uint16_t word = ((uint16_t)ptr[i] << 8) | (uint16_t)ptr[i + 1];
        sum += word;
        
        /*
         * 累加说明：
         * 例如：ptr[i]=0x45, ptr[i+1]=0x00
         * word = (0x45 << 8) | 0x00 = 0x4500
         * sum += 0x4500
         */
    }

    // ===== 步骤1-2：处理奇数长度 =====
    // 如果长度为奇数，处理最后一个字节
    if (length % 2 == 1) {
        // 最后一个字节填充到高8位，低8位补0
        uint16_t last_byte = ((uint16_t)ptr[length - 1] << 8);
        sum += last_byte;
        
        /*
         * 奇数字节处理说明：
         * 例如：ptr[length-1]=0x3C
         * last_byte = 0x3C << 8 = 0x3C00
         * sum += 0x3C00
         */
    }

    // ===== 步骤3-4：循环进位处理溢出 =====
    // 处理32位和中可能出现的进位
    // 原理：32位和 = 高16位 + 低16位，需要继续累加直到没有进位
    /*
     * 溢出处理说明：
     * 假设 sum = 0x00010000 + 0x00005555 = 0x00015555
     * 高16位 = 0x0001，低16位 = 0x5555
     * 继续累加：0x0001 + 0x5555 = 0x5556，没有进位，停止
     * 
     * 另一个例子：
     * sum = 0x0001FFFF + 0x00002222 = 0x00022221
     * 高16位 = 0x0002，低16位 = 0x2221
     * 继续累加：0x0002 + 0x2221 = 0x2223，没有进位，停止
     */
    while (sum >> 16) {
        // 进位处理：将高16位加回低16位
        sum = (sum & 0xFFFF) + (sum >> 16);
        
        /*
         * 说明：sum >> 16 得到高16位
         *      sum & 0xFFFF 得到低16位
         * 例如：sum = 0x10001
         *      高16位 = 0x0001
         *      低16位 = 0x0001
         *      sum = 0x0001 + 0x0001 = 0x0002
         *      继续循环：0x0002 >> 16 = 0，停止
         */
    }

    // ===== 步骤5：对结果取反得到校验和 =====
    // 一补数：~sum（按位取反）
    uint16_t checksum = (uint16_t)(~sum);
    
    /*
     * 取反说明：
     * sum = 0x453C
     * checksum = ~0x453C = 0xBAC3
     * 
     * 验证：0x453C + 0xBAC3 = 0xFFFF
     */

    return checksum;
}

// ==================== 校验和验证 ====================

/**
 * 验证校验和
 * 
 * 验证原理（RFC 791）：
 * 发送方：checksum = ~(sum of all 16-bit words)
 * 接收方：verification = sum of all 16-bit words (including checksum)
 *        if (verification == 0xFFFF) → 验证通过
 * 
 * 数学证明：
 * 发送时：data_sum + checksum = data_sum + ~data_sum = 0xFFFF
 * 接收时：计算 (data_sum + checksum)
 *        = (data_sum + ~data_sum) = 0xFFFF ✓
 * 
 * @param data 包含校验和的完整数据
 * @param length 数据总长度
 * @param checksum 期望的校验和值
 * @return 验证通过返回true
 */
bool verify_checksum(const void* data, size_t length, uint16_t checksum)
{
    // 输入验证
    if (data == NULL || length == 0) {
        return false;
    }

    // 计算整个数据的校验和（包括checksum字段）
    uint16_t calculated = calculate_checksum(data, length);

    // 验证逻辑
    /*
     * 两种验证方式：
     * 方式1：calculated == checksum（直接比较）
     * 方式2：calculated == 0xFFFF（包含checksum的整体验证）
     * 
     * 这里使用方式1，因为checksum已经单独提供
     */
    return (calculated == checksum);
}

// ==================== 帧校验和计算 ====================

/**
 * 计算帧的校验和
 * 
 * 计算对象：
 * - seq_num (4字节) + ack_num (4字节) + window_size (2字节) + 
 *   frame_type (1字节) + data_len (2字节) + data (可变长度)
 * - 不包括checksum字段本身
 * 
 * @param frame 帧指针
 * @return 16位校验和
 */
uint16_t calculate_frame_checksum(const Frame* frame)
{
    if (frame == NULL) {
        return 0xFFFF;
    }

    // 创建临时缓冲区，用于计算校验和
    // 计算需要的大小：帧头部（14字节）+ 数据长度
    size_t frame_size = FRAME_HEADER_SIZE + frame->data_len;

    // 分配临时缓冲区
    uint8_t* temp_buffer = new uint8_t[frame_size];
    if (temp_buffer == NULL) {
        return 0xFFFF;
    }

    // ===== 序列化帧头部到临时缓冲区 =====
    // 注意：这里使用网络字节序（大端）
    
    // seq_num (4字节，网络字节序)
    temp_buffer[0] = (frame->seq_num >> 24) & 0xFF;
    temp_buffer[1] = (frame->seq_num >> 16) & 0xFF;
    temp_buffer[2] = (frame->seq_num >> 8) & 0xFF;
    temp_buffer[3] = frame->seq_num & 0xFF;

    // ack_num (4字节，网络字节序)
    temp_buffer[4] = (frame->ack_num >> 24) & 0xFF;
    temp_buffer[5] = (frame->ack_num >> 16) & 0xFF;
    temp_buffer[6] = (frame->ack_num >> 8) & 0xFF;
    temp_buffer[7] = frame->ack_num & 0xFF;

    // window_size (2字节，网络字节序)
    temp_buffer[8] = (frame->window_size >> 8) & 0xFF;
    temp_buffer[9] = frame->window_size & 0xFF;

    // frame_type (1字节)
    temp_buffer[10] = frame->frame_type;

    // data_len (2字节，网络字节序)
    temp_buffer[11] = (frame->data_len >> 8) & 0xFF;
    temp_buffer[12] = frame->data_len & 0xFF;

    // 校验和字段设置为0（计算时不包括该字段）
    temp_buffer[13] = 0;

    // ===== 复制数据部分 =====
    if (frame->data_len > 0) {
        memcpy(temp_buffer + FRAME_HEADER_SIZE, frame->data, frame->data_len);
    }

    // ===== 计算校验和 =====
    uint16_t checksum = calculate_checksum(temp_buffer, frame_size);

    // ===== 释放临时缓冲区 =====
    delete[] temp_buffer;

    return checksum;
}

// ==================== 帧校验和验证 ====================

/**
 * 验证帧的校验和
 * 
 * 验证过程：
 * 1. 重新计算帧的校验和（使用frame->checksum字段中的值）
 * 2. 比较计算结果与frame->checksum
 * 3. 如果相同则验证通过
 * 
 * 或者使用RFC 791方式：
 * 1. 将帧序列化为字节流（包括checksum字段）
 * 2. 计算整个字节流的校验和
 * 3. 如果结果为0xFFFF则验证通过
 * 
 * @param frame 帧指针
 * @return 验证通过返回true
 */
bool verify_frame_checksum(const Frame* frame)
{
    if (frame == NULL) {
        return false;
    }

    // 计算期望的校验和
    uint16_t calculated = calculate_frame_checksum(frame);

    // 比较计算结果与帧中存储的校验和
    bool result = (calculated == frame->checksum);

    /*
     * 可选的验证方法（RFC 791标准方式）：
     * 
     * 创建临时缓冲区包含完整的帧（包括校验和）
     * uint16_t verification = calculate_checksum(整个帧数据);
     * return (verification == 0xFFFF);
     * 
     * 两种方法在数学上是等价的：
     * 方法1：calculated == frame->checksum
     * 方法2：(calculated + frame->checksum) & 0xFFFF == 0xFFFF
     */

    return result;
}

// ==================== 范围校验和计算 ====================

/**
 * 计算缓冲区中指定范围的校验和
 * 
 * @param buffer 缓冲区指针
 * @param start 起始位置（字节偏移）
 * @param len 数据长度（字节数）
 * @return 计算得到的校验和
 */
uint16_t calculate_checksum_range(const uint8_t* buffer, size_t start, size_t len)
{
    // 输入验证
    if (buffer == NULL || len == 0) {
        return 0xFFFF;
    }

    // 调用主计算函数
    return calculate_checksum(buffer + start, len);
}
