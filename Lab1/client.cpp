#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #include <process.h> // _beginthreadex
    #include <windows.h> // SetConsoleOutputCP, SetConsoleCP
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SocketType;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define CLOSE_SOCKET(s) closesocket(s)
    typedef unsigned (__stdcall *ThreadFunction)(void*);
    #define CREATE_THREAD(func, arg) _beginthreadex(NULL, 0, (ThreadFunction)func, arg, 0, NULL)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <pthread.h>
    typedef int SocketType;
    #define INVALID_SOCKET_VALUE -1
    #define CLOSE_SOCKET(s) close(s)
    #define CREATE_THREAD(func, arg) pthread_create(NULL, NULL, func, arg)
#endif

// 聊天协议定义：type|username|message
// type: 0-登录, 1-登出, 2-广播消息, 3-私聊消息, 4-系统消息

// 全局变量
char username[50];
SocketType clientSocket = INVALID_SOCKET_VALUE;
bool clientRunning = true;
#ifdef _WIN32
CRITICAL_SECTION coutMutex;
#else
pthread_mutex_t coutMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// 错误处理函数
void printError(const char* message) {
#ifdef _WIN32
    EnterCriticalSection(&coutMutex);
    fprintf(stderr, "%s: %d\n", message, WSAGetLastError());
    LeaveCriticalSection(&coutMutex);
#else
    pthread_mutex_lock(&coutMutex);
    fprintf(stderr, "%s: %s\n", message, strerror(errno));
    pthread_mutex_unlock(&coutMutex);
#endif
}

// 输出线程安全的消息
void printMessage(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
#ifdef _WIN32
    EnterCriticalSection(&coutMutex);
    vprintf(format, args);
    LeaveCriticalSection(&coutMutex);
#else
    pthread_mutex_lock(&coutMutex);
    vprintf(format, args);
    pthread_mutex_unlock(&coutMutex);
#endif
    
    va_end(args);
}

// 初始化Winsock（Windows平台）
bool initializeWinsock() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printError("WSAStartup failed");
        return false;
    }
    
    // 设置控制台为UTF-8编码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    InitializeCriticalSection(&coutMutex);
#else
    pthread_mutex_init(&coutMutex, NULL);
#endif
    return true;
}

// 清理Winsock（Windows平台）
void cleanupWinsock() {
#ifdef _WIN32
    DeleteCriticalSection(&coutMutex);
    WSACleanup();
#else
    pthread_mutex_destroy(&coutMutex);
#endif
}

// 发送消息给服务器
bool sendToServer(const char* message) {
#ifdef _WIN32
    int result = send(clientSocket, message, strlen(message), 0);
    if (result == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();
        if (errorCode == WSAEWOULDBLOCK) {
            // 非阻塞模式下发送缓冲区已满，尝试重发
            Sleep(5);
            result = send(clientSocket, message, strlen(message), 0);
            if (result == SOCKET_ERROR) {
                printError("Send failed");
                return false;
            }
        } else {
            printError("Send failed");
            return false;
        }
    }
#else
    if (send(clientSocket, message, strlen(message), 0) == -1) {
        printError("Send failed");
        return false;
    }
#endif
    return true;
}

// 接收消息线程函数
#ifdef _WIN32
unsigned __stdcall receiveMessagesThread(void* arg) {
#else
void* receiveMessagesThread(void* arg) {
#endif
    char buffer[1024];
    
    while (clientRunning) {
        // 接收消息
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
#ifdef _WIN32
        if (bytesRead == SOCKET_ERROR) {
            int errorCode = WSAGetLastError();
            if (errorCode == WSAEWOULDBLOCK) {
                // 非阻塞模式下没有数据可读，继续尝试
                Sleep(10);
                continue;
            } else if (errorCode == WSAECONNABORTED || errorCode == WSAECONNRESET) {
                printMessage("连接已断开\n");
                clientRunning = false;
                break;
            } else {
                printError("Recv failed");
                clientRunning = false;
                break;
            }
        } else if (bytesRead == 0) {
            printMessage("服务器已关闭连接\n");
            clientRunning = false;
            break;
        }
#else
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                printMessage("服务器已关闭连接\n");
            } else {
                printError("Recv failed");
            }
            clientRunning = false;
            break;
        }
#endif
        
        // 确保消息以null结尾
        buffer[bytesRead] = '\0';
        
        // 解析协议
        char* firstDelim = strchr(buffer, '|');
        if (firstDelim != NULL) {
            int messageType = atoi(buffer);
            char* secondDelim = strchr(firstDelim + 1, '|');
            
            if (secondDelim != NULL) {
                *firstDelim = '\0';  // 分割类型和发送者名
                char* senderName = firstDelim + 1;
                *secondDelim = '\0';  // 分割发送者名和内容
                char* content = secondDelim + 1;
                
                // 输出消息
                switch (messageType) {
                    case 2: // 广播消息
                        printMessage("%s: %s\n", senderName, content);
                        break;
                    
                    case 3: // 私聊消息
                        printMessage("[私聊] %s: %s\n", senderName, content);
                        break;
                    
                    case 4: // 系统消息
                        printMessage("[系统] %s\n", content);
                        break;
                }
                
                // 恢复分隔符
                *firstDelim = '|';
                *secondDelim = '|';
            }
        }
    }
    
#ifdef _WIN32
    return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

// 主函数
int main() {
    struct sockaddr_in serverAddr;
    
    // 初始化
    if (!initializeWinsock()) {
        return 1;
    }
    
    // 创建socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET_VALUE) {
        printError("Socket creation failed");
        cleanupWinsock();
        return 1;
    }
    
    // Windows平台设置套接字为非阻塞模式，以便更好地处理通信
    #ifdef _WIN32
    u_long mode = 1; // 非阻塞模式
    ioctlsocket(clientSocket, FIONBIO, &mode);
    #endif
    
    // 设置服务器地址
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8888);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 本地地址
    
    // 连接服务器
#ifdef _WIN32
    // Windows平台非阻塞连接处理
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int errorCode = WSAGetLastError();
        if (errorCode != WSAEWOULDBLOCK && errorCode != WSAEINPROGRESS) {
            printError("Connect failed");
            CLOSE_SOCKET(clientSocket);
            cleanupWinsock();
            return 1;
        }
        
        // 使用select等待连接完成
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(clientSocket, &writeSet);
        
        // 设置超时（5秒）
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        if (select(0, NULL, &writeSet, NULL, &timeout) <= 0) {
            printError("Connect timeout");
            CLOSE_SOCKET(clientSocket);
            cleanupWinsock();
            return 1;
        }
        
        // 检查连接是否成功
        int optval;
        int optlen = sizeof(optval);
        if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, (char*)&optval, &optlen) < 0 || optval != 0) {
            printError("Connect failed");
            CLOSE_SOCKET(clientSocket);
            cleanupWinsock();
            return 1;
        }
    }
#else
    // 非Windows平台连接处理
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        printError("Connect failed");
        CLOSE_SOCKET(clientSocket);
        cleanupWinsock();
        return 1;
    }
#endif
    
    printMessage("连接服务器成功！\n");
    printMessage("请输入您的用户名: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';  // 移除换行符
    
    // 发送登录消息
    char loginMsg[1024];
    sprintf(loginMsg, "0|%s|", username);
    if (!sendToServer(loginMsg)) {
        CLOSE_SOCKET(clientSocket);
        cleanupWinsock();
        return 1;
    }
    
    // 启动接收消息线程
    CREATE_THREAD(receiveMessagesThread, NULL);
    
    printMessage("欢迎来到聊天室！\n");
    printMessage("输入 /quit 退出聊天室，输入 @用户名:消息 发送私聊\n");
    
    // 主循环：处理用户输入
    while (clientRunning) {
        char message[1024];
        if (fgets(message, sizeof(message), stdin) == NULL) {
            break; // 处理输入错误
        }
        message[strcspn(message, "\n")] = '\0';  // 移除换行符
        
        if (strcmp(message, "/quit") == 0) {
            // 发送登出消息
            char logoutMsg[1024];
            sprintf(logoutMsg, "1|%s|", username);
            sendToServer(logoutMsg);
            
            clientRunning = false;
            break;
        } else if (message[0] == '@') {
            // 处理私聊命令：@用户名:消息
            char* colonPos = strchr(message, ':');
            if (colonPos != NULL && colonPos > message + 1) {
                // 格式: 3|username|targetUsername:message
                char privateMsg[1024];
                sprintf(privateMsg, "3|%s|%s", username, message + 1); // 去掉@符号
                sendToServer(privateMsg);
            } else {
                printMessage("私聊格式: @用户名:消息\n");
            }
        } else {
            // 广播消息
            char broadcastMsg[1024];
            sprintf(broadcastMsg, "2|%s|%s", username, message);
            sendToServer(broadcastMsg);
        }
    }
    
    // 清理资源
    #ifdef _WIN32
    // 恢复为阻塞模式再关闭
    u_long blockMode = 0;
    ioctlsocket(clientSocket, FIONBIO, &blockMode);
    #endif
    CLOSE_SOCKET(clientSocket);
    cleanupWinsock();
    printMessage("已退出聊天室\n");
    
    return 0;
}