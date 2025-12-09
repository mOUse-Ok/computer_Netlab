# RENO 拥塞控制算法测试指南

## 一、实现概述

本项目成功实现了 TCP RENO 拥塞控制算法，主要修改了以下文件：

### 1. protocol.h 修改内容
- **添加 RENO 阶段枚举**：`enum RenoPhase { SLOW_START, CONGESTION_AVOIDANCE, FAST_RECOVERY }`
- **添加 RENO 常量**：
  - `INITIAL_CWND = 1`：初始拥塞窗口大小
  - `INITIAL_SSTHRESH = 32`：初始慢启动阈值
  - `MIN_SSTHRESH = 2`：最小慢启动阈值
  - `DUP_ACK_THRESHOLD = 3`：触发快速重传的重复 ACK 阈值

- **SendWindow 结构体增强**：
  - `uint32_t cwnd`：拥塞窗口大小（单位：包数）
  - `uint32_t ssthresh`：慢启动阈值（单位：包数）
  - `uint32_t dup_ack_count`：重复 ACK 计数器
  - `uint32_t last_ack`：上一次收到的 ACK 序列号
  - `RenoPhase reno_phase`：当前 RENO 阶段

- **RENO 核心方法**：
  - `getEffectiveWindow()`：获取有效发送窗口大小（min(cwnd, 固定窗口)）
  - `handleNewACK(uint32_t ack_num)`：处理新 ACK，实现 RENO 四阶段逻辑
  - `handleFastRetransmit()`：处理快速重传
  - `handleTimeout()`：处理超时事件

- **辅助函数**：
  - `getRenoPhaseName(RenoPhase phase)`：获取 RENO 阶段名称，用于调试输出

### 2. client.cpp 修改内容
- **整合 RENO 到流水线发送**：
  - 发送窗口受 `cwnd` 限制（实际窗口 = min(cwnd, FIXED_WINDOW_SIZE)）
  - 接收 ACK 时调用 `handleNewACK()` 更新拥塞窗口
  - 检测到 3 个重复 ACK 时触发快速重传
  - 检测到超时时调用 `handleTimeout()` 重置拥塞窗口

- **详细日志输出**：
  - 显示当前 cwnd、ssthresh、phase
  - 显示窗口大小变化
  - 显示阶段转换（SLOW_START -> CONGESTION_AVOIDANCE -> FAST_RECOVERY）

## 二、RENO 四阶段工作原理

### 阶段 1：慢启动（SLOW_START）
**进入条件**：
- 初始状态
- 超时后恢复

**cwnd 调整规则**：
- 每收到 1 个新 ACK，`cwnd += 1`（指数增长：1→2→4→8→16→...）

**退出条件**：
- 当 `cwnd >= ssthresh` 时，进入拥塞避免阶段

**预期观察**：
```
[RENO] Slow Start: cwnd=1, ssthresh=32
[RENO] Slow Start: cwnd=2, ssthresh=32
[RENO] Slow Start: cwnd=4, ssthresh=32
...
[RENO] Phase transition: SLOW_START -> CONGESTION_AVOIDANCE (cwnd=32, ssthresh=32)
```

---

### 阶段 2：拥塞避免（CONGESTION_AVOIDANCE）
**进入条件**：
- 慢启动阶段 `cwnd >= ssthresh`

**cwnd 调整规则**：
- 每收到 1 个新 ACK，`cwnd += 1/cwnd`（线性增长：32→33→34→35→...）
- 实现方式：每收到 `cwnd` 个 ACK，`cwnd` 增加 1

**退出条件**：
- 收到 3 个重复 ACK → 快速重传/快速恢复
- 超时 → 慢启动

**预期观察**：
```
[RENO] Congestion Avoidance: cwnd increased to 33
[RENO] Congestion Avoidance: cwnd increased to 34
```

---

### 阶段 3：快速重传（Fast Retransmit）
**触发条件**：
- 收到 3 个重复 ACK（`dup_ack_count == 3`）

**执行动作**：
1. 重传丢失的包（序列号 = `last_ack`）
2. `ssthresh = max(cwnd/2, 2)`（例如：cwnd=32 → ssthresh=16）
3. `cwnd = ssthresh`（例如：cwnd=16）
4. 进入快速恢复阶段

**预期观察**：
```
[RENO] Duplicate ACK received (count=1, ack=XXX)
[RENO] Duplicate ACK received (count=2, ack=XXX)
[RENO] Duplicate ACK received (count=3, ack=XXX)
[RENO] Fast Retransmit triggered (3 duplicate ACKs)
[RENO] Entering FAST_RECOVERY (cwnd=16, ssthresh=16)
[RENO] Fast Retransmit: retransmitting seq=XXX
```

---

### 阶段 4：快速恢复（FAST_RECOVERY）
**进入条件**：
- 快速重传后

**cwnd 调整规则**：
- 每收到 1 个重复 ACK，`cwnd += 1`（窗口膨胀：16→17→18→...）
- 收到新 ACK，`cwnd = ssthresh`，进入拥塞避免

**退出条件**：
- 收到新 ACK → 拥塞避免阶段

**预期观察**：
```
[RENO] Fast Recovery: cwnd inflated to 17
[RENO] Fast Recovery: cwnd inflated to 18
[RENO] Fast Recovery completed, transition to CONGESTION_AVOIDANCE (cwnd=16, ssthresh=16)
```

---

### 超时处理（任何阶段）
**触发条件**：
- 数据包发送后超过 `SACK_TIMEOUT_MS`（500ms）未收到 ACK

**执行动作**：
1. `ssthresh = max(cwnd/2, 2)`（例如：cwnd=32 → ssthresh=16）
2. `cwnd = 1`（重置为初始值）
3. `dup_ack_count = 0`（重置重复 ACK 计数）
4. 进入慢启动阶段

**预期观察**：
```
[RENO] Timeout detected
[RENO] Timeout recovery: entering SLOW_START (cwnd=1, ssthresh=16)
[Timeout Retransmit] seq=XXX, elapsed 500ms
```

---

## 三、测试验证步骤

### 1. 编译项目
```powershell
cd d:\computer_NetLab\Lab2\mylab
g++ -o server.exe server.cpp -lws2_32
g++ -o client.exe client.cpp -lws2_32
```

### 2. 启动服务端
```powershell
.\server.exe
```

预期输出：
```
UDP Server is running on port 8888
Waiting for client messages...
```

### 3. 启动客户端
```powershell
.\client.exe
```

### 4. 观察日志验证 RENO 行为

#### 验证点 1：慢启动阶段
在客户端日志中查找：
```
[RENO] Initial state: cwnd=1, ssthresh=32, phase=SLOW_START
[Send] Data packet seq=XXX, ..., cwnd=1
[Receive] ACK packet ack=XXX
[RENO] Slow Start: cwnd=2, ssthresh=32
[Send] Data packet seq=XXX, ..., cwnd=2
```

**验证要点**：
- 初始 `cwnd=1`，每收到 ACK 后 cwnd 翻倍（1→2→4→8...）
- 当 `cwnd >= ssthresh=32` 时，阶段转换

#### 验证点 2：拥塞避免阶段
```
[RENO] Phase transition: SLOW_START -> CONGESTION_AVOIDANCE (cwnd=32, ssthresh=32)
[RENO] Congestion Avoidance: cwnd increased to 33
[RENO] Congestion Avoidance: cwnd increased to 34
```

**验证要点**：
- cwnd 从 32 开始线性增长（32→33→34...）
- 增长速度远慢于慢启动

#### 验证点 3：快速重传/快速恢复（模拟丢包）
服务端默认会丢弃第 2 个包（`seq=clientSeq+2`），触发重复 ACK：

```
[RENO] Duplicate ACK received (count=1, ack=XXX)
[RENO] Duplicate ACK received (count=2, ack=XXX)
[RENO] Duplicate ACK received (count=3, ack=XXX)
[RENO] Fast Retransmit triggered (3 duplicate ACKs)
[RENO] Entering FAST_RECOVERY (cwnd=16, ssthresh=16)  # 假设触发时 cwnd=32
[RENO] Fast Retransmit: retransmitting seq=XXX
[RENO] Fast Recovery: cwnd inflated to 17
[RENO] Fast Recovery completed, transition to CONGESTION_AVOIDANCE (cwnd=16, ssthresh=16)
```

**验证要点**：
- 收到 3 个重复 ACK 后，`ssthresh = cwnd/2`，`cwnd = ssthresh`
- 快速恢复期间，每收到重复 ACK，cwnd += 1
- 收到新 ACK 后，cwnd 恢复为 ssthresh，进入拥塞避免

#### 验证点 4：超时处理（可修改超时时间测试）
如需测试超时，可临时修改 `SACK_TIMEOUT_MS` 为更小值（如 200ms）：

```
[RENO] Timeout detected
[RENO] Timeout recovery: entering SLOW_START (cwnd=1, ssthresh=16)
[Timeout Retransmit] seq=XXX, elapsed 200ms
```

**验证要点**：
- 超时后，`ssthresh = cwnd/2`，`cwnd = 1`
- 重新进入慢启动阶段

---

## 四、关键设计特点

### 1. 拥塞窗口优先于流量控制
实际发送窗口大小 = `min(cwnd, FIXED_WINDOW_SIZE)`
```cpp
uint32_t getEffectiveWindow() const {
    return (cwnd < FIXED_WINDOW_SIZE) ? cwnd : FIXED_WINDOW_SIZE;
}
```

### 2. 重复 ACK 检测机制
通过比较 `ack_num` 和 `last_ack` 判断是否为重复 ACK：
```cpp
if (ack_num == last_ack) {
    dup_ack_count++;  // 累积重复 ACK 计数
}
```

### 3. 快速重传与选择确认（SACK）结合
- SACK 用于标记非连续已接收的包
- 快速重传用于快速恢复丢失的包
- 两者协同工作，提高传输效率

### 4. 阶段转换清晰可见
每次阶段转换都有明确的日志输出：
```cpp
std::cout << "[RENO] Phase transition: SLOW_START -> CONGESTION_AVOIDANCE" << std::endl;
```

---

## 五、RENO 参数配置

可根据网络环境调整以下参数（在 `protocol.h` 中定义）：

```cpp
#define INITIAL_CWND 1           // 初始拥塞窗口（建议 1-4）
#define INITIAL_SSTHRESH 32      // 初始慢启动阈值（建议 16-64）
#define MIN_SSTHRESH 2           // 最小慢启动阈值（建议 2-4）
#define DUP_ACK_THRESHOLD 3      // 快速重传阈值（标准值 3）
#define SACK_TIMEOUT_MS 500      // 超时重传时间（建议 200-1000ms）
```

---

## 六、预期测试结果总结

### 正常传输流程（无丢包）
1. **慢启动**：cwnd 从 1 指数增长到 32（ssthresh）
2. **拥塞避免**：cwnd 从 32 线性增长到窗口上限（FIXED_WINDOW_SIZE=4）
3. 数据成功传输完成

### 丢包场景（服务端模拟丢包）
1. **慢启动**：cwnd 从 1 指数增长
2. **触发快速重传**：收到 3 个重复 ACK
3. **快速恢复**：ssthresh 和 cwnd 减半，cwnd 逐步膨胀
4. **拥塞避免**：cwnd 从 ssthresh 线性增长
5. 数据成功传输完成

### 超时场景（网络延迟大）
1. **慢启动**：cwnd 从 1 指数增长
2. **触发超时**：某个包超时未收到 ACK
3. **超时恢复**：ssthresh 减半，cwnd 重置为 1，重新慢启动
4. 数据成功传输完成

---

## 七、故障排查

### 问题 1：看不到 RENO 日志
**原因**：可能编译时使用了旧的代码
**解决**：
```powershell
# 删除旧的可执行文件
rm client.exe
# 重新编译
g++ -o client.exe client.cpp -lws2_32
```

### 问题 2：cwnd 没有增长
**原因**：可能没有收到 ACK，或 ACK 处理逻辑有误
**解决**：
- 检查服务端是否正常运行
- 检查客户端日志中是否有 `[Receive] ACK packet` 输出
- 检查 `handleNewACK()` 是否被正确调用

### 问题 3：快速重传未触发
**原因**：服务端未模拟丢包，或重复 ACK 检测逻辑有误
**解决**：
- 确认 `server.cpp` 中 `g_simulateLoss = true` 且 `g_lossSeq` 设置正确
- 检查客户端是否收到重复 ACK（`dup_ack_count` 是否增加）

### 问题 4：超时未触发
**原因**：`SACK_TIMEOUT_MS` 设置过大，测试时未达到超时
**解决**：
- 临时将 `SACK_TIMEOUT_MS` 改为 200ms 测试
- 或在服务端增加延迟模拟网络慢速

---

## 八、总结

本实现完整覆盖了 TCP RENO 拥塞控制算法的四个核心阶段：
1. ✅ 慢启动（Slow Start）
2. ✅ 拥塞避免（Congestion Avoidance）
3. ✅ 快速重传（Fast Retransmit）
4. ✅ 快速恢复（Fast Recovery）
5. ✅ 超时处理（Timeout Recovery）

**代码特点**：
- 遵循"最小化改动"原则，只在必要处修改
- 详细的中文注释，便于理解和维护
- 清晰的日志输出，便于调试和验证
- 符合 TCP RENO 标准规范

**实际应用价值**：
- 动态调整发送速率，适应网络拥塞
- 快速检测和恢复丢包，提高传输效率
- 平衡吞吐量和稳定性

---

**测试完成后，请查看客户端和服务端的完整日志，验证以上各个阶段是否按预期工作！**
