#include <iostream>
#include <string>
#include <vector>
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

// 客户端信息结构
typedef struct {
    SocketType socket;
    char username[50];
    struct sockaddr_in address;
} ClientInfo;

// 全局变量
std::vector<ClientInfo*> clients;
#ifdef _WIN32
CRITICAL_SECTION clientsMutex;
#else
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;
#endif
bool serverRunning = true;

// 错误处理函数
void printError(const char* message) {
#ifdef _WIN32
    fprintf(stderr, "%s: %d\n", message, WSAGetLastError());
#else
    fprintf(stderr, "%s: %s\n", message, strerror(errno));
#endif
}

// 互斥锁操作
void lockMutex() {
#ifdef _WIN32
    EnterCriticalSection(&clientsMutex);
#else
    pthread_mutex_lock(&clientsMutex);
#endif
}

void unlockMutex() {
#ifdef _WIN32
    LeaveCriticalSection(&clientsMutex);
#else
    pthread_mutex_unlock(&clientsMutex);
#endif
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
    
    InitializeCriticalSection(&clientsMutex);
#else
    pthread_mutex_init(&clientsMutex, NULL);
#endif
    return true;
}

// 清理Winsock（Windows平台）
void cleanupWinsock() {
#ifdef _WIN32
    DeleteCriticalSection(&clientsMutex);
    WSACleanup();
#else
    pthread_mutex_destroy(&clientsMutex);
#endif
}

// 发送消息给指定客户端
bool sendToClient(const ClientInfo* client, const char* message) {
#ifdef _WIN32
    if (send(client->socket, message, strlen(message), 0) == SOCKET_ERROR) {
        printError("Send failed");
        return false;
    }
#else
    if (send(client->socket, message, strlen(message), 0) == -1) {
        printError("Send failed");
        return false;
    }
#endif
    return true;
}

// 广播消息给所有客户端（除了发送者自己）
void broadcastMessage(const char* message, const char* senderUsername) {
    lockMutex();
    for (size_t i = 0; i < clients.size(); ++i) {
        if (strcmp(clients[i]->username, senderUsername) != 0) {
            sendToClient(clients[i], message);
        }
    }
    unlockMutex();
}

// 处理私聊消息
void handlePrivateMessage(const char* message, const char* senderUsername) {
    // 解析目标用户名，格式为"targetUsername:messageContent"
    const char* colonPos = strchr(message, ':');
    if (colonPos != NULL) {
        char targetUsername[50];
        strncpy(targetUsername, message, colonPos - message);
        targetUsername[colonPos - message] = '\0';
        
        const char* actualMessage = colonPos + 1;
        bool userFound = false;
        
        lockMutex();
        for (size_t i = 0; i < clients.size(); ++i) {
            if (strcmp(clients[i]->username, targetUsername) == 0) {
                char privateMsg[1024];
                sprintf(privateMsg, "3|%s|%s", senderUsername, actualMessage);
                sendToClient(clients[i], privateMsg);
                userFound = true;
                break;
            }
        }
        unlockMutex();
        
        // 给发送者一个确认消息
        lockMutex();
        for (size_t i = 0; i < clients.size(); ++i) {
            if (strcmp(clients[i]->username, senderUsername) == 0) {
                char confirmMsg[1024];
                if (userFound) {
                    sprintf(confirmMsg, "4|System|私聊消息已发送给 %s", targetUsername);
                } else {
                    sprintf(confirmMsg, "4|System|用户 %s 不存在或不在线", targetUsername);
                }
                sendToClient(clients[i], confirmMsg);
                break;
            }
        }
        unlockMutex();
    }
}

// 处理客户端消息的线程函数
#ifdef _WIN32
unsigned __stdcall handleClientThread(void* arg) {
#else
void* handleClientThread(void* arg) {
#endif
    ClientInfo* client = (ClientInfo*)arg;
    char buffer[1024];
    bool connected = true;
    
    while (connected && serverRunning) {
        // 接收消息
        int bytesRead = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        
#ifdef _WIN32
        if (bytesRead == SOCKET_ERROR) {
            int errorCode = WSAGetLastError();
            if (errorCode == WSAEWOULDBLOCK) {
                // 非阻塞模式下没有数据可读，继续尝试
                Sleep(10);
                continue;
            } else {
                printError("Recv failed");
                connected = false;
                break;
            }
        } else if (bytesRead == 0) {
            printf("%s 断开连接\n", client->username);
            connected = false;
            break;
        }
        
        // 确保消息以null结尾
        buffer[bytesRead] = '\0';
#else
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                printf("%s 断开连接\n", client->username);
            } else {
                printError("Recv failed");
            }
            connected = false;
            break;
        }
        
        // 确保消息以null结尾
        buffer[bytesRead] = '\0';
#endif
        
        // 解析协议
        char* firstDelim = strchr(buffer, '|');
        if (firstDelim != NULL) {
            int messageType = atoi(buffer);
            char* secondDelim = strchr(firstDelim + 1, '|');
            
            if (secondDelim != NULL) {
                *firstDelim = '\0';  // 分割类型和用户名
                char* username = firstDelim + 1;
                *secondDelim = '\0';  // 分割用户名和内容
                char* content = secondDelim + 1;
                
                switch (messageType) {
                    case 0: // 登录
                        strncpy(client->username, username, sizeof(client->username) - 1);
                        printf("%s 加入聊天室\n", username);
                        
                        char joinMsg[1024];
                        sprintf(joinMsg, "4|System|%s 加入了聊天室", username);
                        broadcastMessage(joinMsg, username);
                        break;
                    
                    case 2: // 广播消息
                        printf("%s: %s\n", username, content);
                        
                        // 重新组装消息
                        char broadcastMsg[1024];
                        sprintf(broadcastMsg, "2|%s|%s", username, content);
                        broadcastMessage(broadcastMsg, username);
                        break;
                    
                    case 3: // 私聊消息
                        handlePrivateMessage(content, username);
                        break;
                    
                    case 1: // 登出
                        connected = false;
                        printf("%s 退出聊天室\n", username);
                        
                        char leaveMsg[1024];
                        sprintf(leaveMsg, "4|System|%s 离开了聊天室", username);
                        broadcastMessage(leaveMsg, username);
                        break;
                }
                
                // 恢复分隔符
                *firstDelim = '|';
                *secondDelim = '|';
            }
        }
    }
    
    // 移除客户端并关闭socket
    lockMutex();
    for (size_t i = 0; i < clients.size(); ++i) {
        if (strcmp(clients[i]->username, client->username) == 0) {
            clients.erase(clients.begin() + i);
            break;
        }
    }
    unlockMutex();
    
    CLOSE_SOCKET(client->socket);
    delete client;
    
#ifdef _WIN32
    return 0;
#else
    pthread_exit(NULL);
    return NULL;
#endif
}

// 处理服务器输入的线程函数
#ifdef _WIN32
unsigned __stdcall inputThreadFunction(void* arg) {
#else
void* inputThreadFunction(void* arg) {
#endif
    char command[100];
    while (true) {
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0';  // 移除换行符
        
        if (strcmp(command, "/quit") == 0) {
            serverRunning = false;
            break;
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
    SocketType serverSocket = INVALID_SOCKET_VALUE;
    struct sockaddr_in serverAddr;
    
    // 初始化
    if (!initializeWinsock()) {
        return 1;
    }
    
    // 创建socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET_VALUE) {
        printError("Socket creation failed");
        cleanupWinsock();
        return 1;
    }
    
    // 设置套接字地址
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8888);
    
    // 绑定socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        printError("Bind failed");
        CLOSE_SOCKET(serverSocket);
        cleanupWinsock();
        return 1;
    }
    
    // 监听连接
    if (listen(serverSocket, SOMAXCONN) == -1) {
        printError("Listen failed");
        CLOSE_SOCKET(serverSocket);
        cleanupWinsock();
        return 1;
    }
    
    printf("服务器启动成功，监听端口 8888...\n");
    printf("输入 /quit 停止服务器\n");
    
    // 启动输入线程处理服务器退出
    CREATE_THREAD(inputThreadFunction, NULL);
    
    // 接受客户端连接
    while (serverRunning) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        // 使用非阻塞方式接受连接，以便能够检测服务器是否需要停止
        #ifdef _WIN32
        u_long mode = 1; // 非阻塞模式
        ioctlsocket(serverSocket, FIONBIO, &mode);
        #else
        int flags = fcntl(serverSocket, F_GETFL, 0);
        fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
        #endif
        
        SocketType clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        
        // 恢复阻塞模式
        #ifdef _WIN32
        mode = 0; // 阻塞模式
        ioctlsocket(serverSocket, FIONBIO, &mode);
        #else
        fcntl(serverSocket, F_SETFL, flags);
        #endif
        
        if (clientSocket != INVALID_SOCKET_VALUE) {
            ClientInfo* client = new ClientInfo;
            client->socket = clientSocket;
            client->address = clientAddr;
            client->username[0] = '\0';
            
            lockMutex();
            clients.push_back(client);
            unlockMutex();
            
            // 创建线程处理客户端
            CREATE_THREAD(handleClientThread, client);
        }
        
        // 短暂休眠避免CPU占用过高
        #ifdef _WIN32
        Sleep(10);
        #else
        usleep(10000);
        #endif
    }
    
    // 关闭服务器
    printf("正在关闭服务器...\n");
    
    // 关闭所有客户端连接
    lockMutex();
    for (size_t i = 0; i < clients.size(); ++i) {
        CLOSE_SOCKET(clients[i]->socket);
        delete clients[i];
    }
    clients.clear();
    unlockMutex();
    
    CLOSE_SOCKET(serverSocket);
    cleanupWinsock();
    
    printf("服务器已关闭\n");
    
    return 0;
}