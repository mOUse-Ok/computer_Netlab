@echo off
REM 编译服务器
echo 编译服务器...
g++ -o server.exe server.cpp -lws2_32 -std=c++11
if errorlevel 1 (
    echo 服务器编译失败
    exit /b 1
)

REM 编译客户端
echo 编译客户端...
g++ -o client.exe client.cpp -lws2_32 -std=c++11
if errorlevel 1 (
    echo 客户端编译失败
    exit /b 1
)

echo 编译完成！
echo 使用方式：
echo 1. 先运行: server.exe
echo 2. 再运行: client.exe
