@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

REM ============================================================
REM 单次测试脚本 - 指定参数进行测试
REM 用法: single_test.bat <ssthresh> <window_size> <loss_rate>
REM 例如: single_test.bat 16 16 5
REM ============================================================

if "%~3"=="" (
    echo 用法: %~nx0 ^<ssthresh^> ^<window_size^> ^<loss_rate^>
    echo 例如: %~nx0 16 16 5
    echo.
    echo 参数说明:
    echo   ssthresh    - 慢启动阈值 (8, 16, 32)
    echo   window_size - 窗口大小 (8, 16, 32)
    echo   loss_rate   - 丢包率百分比 (0, 5, 10)
    exit /b 1
)

set SSTHRESH=%~1
set WINDOW=%~2
set LOSS=%~3

echo ============================================================
echo 单次测试配置
echo ============================================================
echo   ssthresh     = %SSTHRESH%
echo   window_size  = %WINDOW%
echo   loss_rate    = %LOSS%%%
echo ============================================================
echo.

REM 确保配置生成器已编译
if not exist "test_config_generator.exe" (
    echo 编译配置生成器...
    g++ -o test_config_generator.exe test_config_generator.cpp -std=c++11
    if errorlevel 1 (
        echo 编译失败!
        exit /b 1
    )
)

REM 生成配置文件
echo 生成配置文件...
test_config_generator.exe %SSTHRESH% %WINDOW% %LOSS%
echo.

REM 编译服务端和客户端
echo 编译服务端和客户端...
g++ -o server.exe server.cpp -lws2_32 -std=c++11
if errorlevel 1 (
    echo 服务端编译失败!
    exit /b 1
)

g++ -o client.exe client.cpp -lws2_32 -std=c++11
if errorlevel 1 (
    echo 客户端编译失败!
    exit /b 1
)

echo 编译成功!
echo.
echo ============================================================
echo 准备测试!
echo ============================================================
echo.
echo 请按以下步骤操作:
echo.
echo 1. 打开第一个新终端, 运行:
echo    server.exe
echo    然后输入保存文件名 (如: received_1.jpg)
echo.
echo 2. 打开第二个新终端, 运行:
echo    client.exe  
echo    然后输入传输文件名: 1.jpg
echo.
echo 3. 等待传输完成, 在 server.txt 中查看结果:
echo    - Transmission Time: 传输时间
echo    - Average Throughput: 平均吞吐率
echo.
echo ============================================================

endlocal
