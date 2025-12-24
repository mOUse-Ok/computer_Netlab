@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo ============================================================
echo            传输性能自动化测试脚本
echo ============================================================
echo.
echo 测试参数组合:
echo   - 发送窗口 (ssthresh): 8, 16, 32
echo   - 接收窗口 (FIXED_WINDOW_SIZE): 8, 16, 32  
echo   - 丢包率: 0%%, 5%%, 10%%
echo   - 测试文件: 1.jpg
echo ============================================================
echo.

REM 创建测试结果目录
if not exist "test_results" mkdir test_results

REM 编译配置生成器
echo [Step 1] 编译配置生成器...
g++ -o test_config_generator.exe test_config_generator.cpp -std=c++11
if errorlevel 1 (
    echo 编译配置生成器失败!
    exit /b 1
)
echo 配置生成器编译成功!
echo.

REM 初始化结果文件
echo 测试时间: %date% %time% > test_results\test_summary.txt
echo. >> test_results\test_summary.txt
echo ============================================================ >> test_results\test_summary.txt
echo                    测试结果汇总表                           >> test_results\test_summary.txt
echo ============================================================ >> test_results\test_summary.txt
echo. >> test_results\test_summary.txt
echo 序号  ssthresh  窗口大小  丢包率  传输时间(s)  吞吐率(KB/s) >> test_results\test_summary.txt
echo ---- -------- -------- ------ ----------- ------------ >> test_results\test_summary.txt

set testNum=0

REM 测试不同发送窗口和接收窗口大小组合 (丢包率固定为5%)
echo ============================================================
echo [测试组1] 不同发送窗口和接收窗口大小 (丢包率=5%%)
echo ============================================================

for %%s in (8 16 32) do (
    for %%w in (8 16 32) do (
        set /a testNum+=1
        echo.
        echo ----------------------------------------------------------
        echo [Test !testNum!] ssthresh=%%s, window=%%w, loss=5%%
        echo ----------------------------------------------------------
        
        REM 生成配置文件
        test_config_generator.exe %%s %%w 5
        
        REM 重新编译
        echo 重新编译 server 和 client...
        g++ -o server.exe server.cpp -lws2_32 -std=c++11 2>nul
        g++ -o client.exe client.cpp -lws2_32 -std=c++11 2>nul
        
        if exist server.exe if exist client.exe (
            echo 编译成功, 准备测试...
            echo ssthresh=%%s, window=%%w, loss=5%% > test_results\test_!testNum!_config.txt
            
            echo.
            echo *** 请手动执行以下步骤进行测试 ***
            echo 1. 打开新终端, 运行: server.exe
            echo 2. 服务端输入保存文件名: test_!testNum!_result.jpg
            echo 3. 打开另一终端, 运行: client.exe
            echo 4. 客户端输入传输文件名: 1.jpg
            echo 5. 等待传输完成, 记录 server.txt 中的传输时间和吞吐率
            echo.
            
            REM 保存当前测试参数
            echo Test !testNum!: ssthresh=%%s, window=%%w, loss=5%% >> test_results\all_tests.txt
        ) else (
            echo 编译失败, 跳过测试 !testNum!
        )
    )
)

REM 测试不同丢包率 (固定窗口大小为16)
echo.
echo ============================================================
echo [测试组2] 不同丢包率 (ssthresh=16, window=16)
echo ============================================================

for %%l in (0 5 10) do (
    set /a testNum+=1
    echo.
    echo ----------------------------------------------------------
    echo [Test !testNum!] ssthresh=16, window=16, loss=%%l%%
    echo ----------------------------------------------------------
    
    REM 生成配置文件
    test_config_generator.exe 16 16 %%l
    
    REM 重新编译
    echo 重新编译 server 和 client...
    g++ -o server.exe server.cpp -lws2_32 -std=c++11 2>nul
    g++ -o client.exe client.cpp -lws2_32 -std=c++11 2>nul
    
    if exist server.exe if exist client.exe (
        echo 编译成功, 准备测试...
        echo ssthresh=16, window=16, loss=%%l%% > test_results\test_!testNum!_config.txt
        
        echo.
        echo *** 请手动执行以下步骤进行测试 ***
        echo 1. 打开新终端, 运行: server.exe
        echo 2. 服务端输入保存文件名: test_!testNum!_result.jpg
        echo 3. 打开另一终端, 运行: client.exe
        echo 4. 客户端输入传输文件名: 1.jpg
        echo 5. 等待传输完成, 记录 server.txt 中的传输时间和吞吐率
        echo.
        
        REM 保存当前测试参数
        echo Test !testNum!: ssthresh=16, window=16, loss=%%l%% >> test_results\all_tests.txt
    ) else (
        echo 编译失败, 跳过测试 !testNum!
    )
)

echo.
echo ============================================================
echo 测试参数已生成完毕!
echo 请根据上述提示手动运行各测试用例并记录结果
echo 结果将保存在 test_results 目录中
echo ============================================================

endlocal
