# 传输性能测试指南

## 测试目标

测试不同参数组合对传输性能的影响：
1. **发送窗口大小 (ssthresh)**: 8、16、32
2. **接收窗口大小 (FIXED_WINDOW_SIZE)**: 8、16、32
3. **丢包率**: 0%、5%、10%
4. **测试文件**: 1.jpg

## 文件说明

| 文件名 | 说明 |
|--------|------|
| `test_config_generator.cpp` | 配置生成器源码 |
| `single_test.bat` | 单次测试脚本 |
| `run_tests.bat` | 批量测试脚本 |
| `auto_test.cpp` | 半自动化测试程序 |
| `collect_results.cpp` | 结果汇总程序 |

## 快速开始

### 方法1：使用单次测试脚本 (推荐)

1. **编译配置生成器**（仅需一次）：
   ```powershell
   g++ -o test_config_generator.exe test_config_generator.cpp -std=c++11
   ```

2. **执行单次测试**：
   ```powershell
   # 语法: single_test.bat <ssthresh> <window_size> <loss_rate>
   single_test.bat 16 16 5    # ssthresh=16, 窗口=16, 丢包率=5%
   ```

3. **按提示操作**：
   - 在新终端运行 `server.exe`，输入保存文件名
   - 在另一终端运行 `client.exe`，输入 `1.jpg`
   - 查看 `server.txt` 中的传输时间和吞吐率

### 方法2：使用结果汇总程序

1. **编译汇总程序**：
   ```powershell
   g++ -o collect_results.exe collect_results.cpp -std=c++11
   ```

2. **运行所有测试**，记录结果后运行：
   ```powershell
   collect_results.exe
   ```

3. **输入各测试结果**，程序自动生成表格

## 完整测试流程

### 测试组1：不同窗口大小组合 (丢包率=5%)

| 测试编号 | ssthresh | 窗口大小 | 丢包率 | 命令 |
|---------|----------|---------|--------|------|
| 1 | 8 | 8 | 5% | `single_test.bat 8 8 5` |
| 2 | 8 | 16 | 5% | `single_test.bat 8 16 5` |
| 3 | 8 | 32 | 5% | `single_test.bat 8 32 5` |
| 4 | 16 | 8 | 5% | `single_test.bat 16 8 5` |
| 5 | 16 | 16 | 5% | `single_test.bat 16 16 5` |
| 6 | 16 | 32 | 5% | `single_test.bat 16 32 5` |
| 7 | 32 | 8 | 5% | `single_test.bat 32 8 5` |
| 8 | 32 | 16 | 5% | `single_test.bat 32 16 5` |
| 9 | 32 | 32 | 5% | `single_test.bat 32 32 5` |

### 测试组2：不同丢包率 (ssthresh=16, 窗口=16)

| 测试编号 | ssthresh | 窗口大小 | 丢包率 | 命令 |
|---------|----------|---------|--------|------|
| 10 | 16 | 16 | 0% | `single_test.bat 16 16 0` |
| 11 | 16 | 16 | 5% | `single_test.bat 16 16 5` |
| 12 | 16 | 16 | 10% | `single_test.bat 16 16 10` |

## 结果记录模板

### 表1：不同窗口大小对传输性能的影响 (丢包率=5%)

| ssthresh | 窗口大小 | 传输时间(s) | 平均吞吐率(KB/s) |
|----------|----------|-------------|------------------|
| 8 | 8 | | |
| 8 | 16 | | |
| 8 | 32 | | |
| 16 | 8 | | |
| 16 | 16 | | |
| 16 | 32 | | |
| 32 | 8 | | |
| 32 | 16 | | |
| 32 | 32 | | |

### 表2：不同丢包率对传输性能的影响 (ssthresh=16, 窗口=16)

| 丢包率(%) | 传输时间(s) | 平均吞吐率(KB/s) |
|-----------|-------------|------------------|
| 0 | | |
| 5 | | |
| 10 | | |

## 从日志中读取结果

测试完成后，在 `server.txt` 文件末尾查找以下信息：

```
========== Server Transmission Statistics ==========
Total Packets Received: xxx
Total Packets Dropped (simulated): xxx
Total Bytes Received: xxx bytes
Transmission Time: x.xxx seconds        <-- 传输时间
Average Throughput: xxx.xx KB/s         <-- 平均吞吐率
====================================================
```

## 注意事项

1. 每次测试前确保关闭之前的 server.exe 和 client.exe
2. 服务端必须先于客户端启动
3. 测试文件 `1.jpg` 必须存在于 `testfile` 目录
4. 建议每组参数测试2-3次取平均值以获得更准确结果
5. 测试时保持网络环境稳定

## 参数说明

- **ssthresh (慢启动阈值)**：TCP RENO拥塞控制中，当 cwnd < ssthresh 时进行慢启动（指数增长），否则进行拥塞避免（线性增长）
- **FIXED_WINDOW_SIZE (接收窗口大小)**：限制同时在传输中的未确认数据包数量
- **丢包率**：服务端模拟丢弃数据包的概率，用于测试重传机制的性能
