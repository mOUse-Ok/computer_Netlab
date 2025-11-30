#ifndef RELIABLE_TRANSPORT_H
#define RELIABLE_TRANSPORT_H

#include "protocol.h"
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <queue>
#include <ctime>
#include <cstdio>

#pragma comment(lib, "ws2_32.lib")

// 发送窗口中的数据包记录
struct SentPacket {
    Packet pkt;
    uint64_t send_time;     // 发送时间(ms)
    int retrans_count;      // 重传次数
};

// 接收窗口中的数据包记录
struct RecvPacket {
    Packet pkt;
    bool received;
};

class ReliableTransport {
protected:
    SOCKET sock;
    sockaddr_in peer_addr;
    int peer_addr_len;
    
    ConnectionState conn_state;
    CongestionState cong_state;
    
    // 发送侧参数
    uint16_t send_seq;          // 发送序列号
    uint16_t send_base;         // 窗口基序号
    uint16_t send_next;         // 下一个要发送的序列号
    SentPacket send_window[WINDOW_SIZE];
    int cwnd;                   // 拥塞窗口大小(段数)
    int ssthresh;               // 慢启动阈值
    int dup_ack_count;          // 重复ACK计数
    
    // 接收侧参数
    uint16_t recv_seq;          // 接收序列号
    uint16_t recv_base;         // 窗口基序号
    RecvPacket recv_window[WINDOW_SIZE];
    
    uint64_t getTimestampMs() {
        return (uint64_t)clock() * 1000 / CLOCKS_PER_SEC;
    }
    
    bool isInWindow(uint16_t seq, uint16_t base) {
        return (seq - base) < WINDOW_SIZE;
    }
    
public:
    ReliableTransport() : sock(INVALID_SOCKET), conn_state(CLOSED),
        cong_state(SLOW_START), send_seq(0), send_base(0), send_next(0),
        recv_seq(0), recv_base(0), cwnd(1), ssthresh(16), dup_ack_count(0) {
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr_len = sizeof(peer_addr);
        memset(send_window, 0, sizeof(send_window));
        memset(recv_window, 0, sizeof(recv_window));
        for (int i = 0; i < WINDOW_SIZE; i++) {
            recv_window[i].received = false;
        }
    }
    
    ~ReliableTransport() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
    }
    
    // 创建UDP套接字
    bool createSocket() {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            wprintf(L"[ERROR][错误] 创建套接字失败: %d\n", WSAGetLastError());
            return false;
        }
        
        // 设置非阻塞模式
        unsigned long mode = 1;
        if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
            wprintf(L"[ERROR][错误] 设置非阻塞模式失败: %d\n", WSAGetLastError());
            closesocket(sock);
            return false;
        }
        
        return true;
    }
    
    // 绑定本地地址
    bool bind(const char* host, int port) {
        sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(port);
        
        // 如果为空字符串或特殊主机，使用INADDR_ANY
        if (host == nullptr || host[0] == '\0') {
            local_addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            local_addr.sin_addr.s_addr = inet_addr(host);
        }
        
        if (::bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
            wprintf(L"[ERROR][错误] 绑定失败: %d\n", WSAGetLastError());
            return false;
        }
        
        return true;
    }
    
    // 设置对端地址
    void setPeerAddr(const char* host, int port) {
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(port);
        peer_addr.sin_addr.s_addr = inet_addr(host);
    }
    
    // 发送原始数据包
    bool sendPacket(const Packet& pkt) {
        int ret = sendto(sock, (const char*)&pkt, PACKET_SIZE, 0,
                        (sockaddr*)&peer_addr, peer_addr_len);
        if (ret == SOCKET_ERROR) {
            wprintf(L"[ERROR][错误] 发送失败: %d\n", WSAGetLastError());
            return false;
        }

        // 调试: 报告目标地址和发送字节数
        const char* addrStr = inet_ntoa(peer_addr.sin_addr);
        wprintf(L"[SEND][发送] 到 %hs:%u (%d 字节)\n", addrStr, ntohs(peer_addr.sin_port), ret);
        return true;
    }
    
    // 接收原始数据包
    bool recvPacket(Packet& pkt, sockaddr_in* from_addr = nullptr) {
        // 将数据包缓冲区清零以避免残留数据
        memset(&pkt, 0, sizeof(pkt));

        sockaddr_in* addr_ptr = from_addr ? from_addr : &peer_addr;
        int addr_len = sizeof(sockaddr_in);
        int ret = recvfrom(sock, (char*)&pkt, PACKET_SIZE, 0,
                          (sockaddr*)addr_ptr, &addr_len);

        if (ret <= 0) return false;

        // 调试: 报告源地址
        const char* addrStr = inet_ntoa(addr_ptr->sin_addr);
        wprintf(L"[RECV][接收] 来自 %hs:%u (%d 字节)\n", addrStr, ntohs(addr_ptr->sin_port), ret);
        // 显示数据包头用于调试
        wprintf(L"[PKT][包头] flags=0x%02x seq=%hu ack=%hu wnd=%hu\n", pkt.flags, pkt.seq, pkt.ack, pkt.wnd_size);

        return true;
    }
    
    // 建立连接(客户端)
    bool connect(const char* host, int port) {
        setPeerAddr(host, port);
        
        if (conn_state != CLOSED) {
            wprintf(L"[ERROR][错误] 无效的状态\n");
            return false;
        }
        
        // 发送SYN
        Packet syn_pkt;
        memset(&syn_pkt, 0, sizeof(syn_pkt));
        syn_pkt.seq = send_seq;
        syn_pkt.flags = FLAG_SYN;
        syn_pkt.wnd_size = WINDOW_SIZE;
        syn_pkt.checksum = calculateChecksum(syn_pkt);
        
        if (!sendPacket(syn_pkt)) {
            wprintf(L"[ERROR][错误] 发送 SYN 失败\n");
            return false;
        }
        
        wprintf(L"[SEND][发送] SYN (seq=%u)\n", syn_pkt.seq);
        conn_state = SYN_SENT;
        send_seq++;
        
        // 等待SYN+ACK
        uint64_t start_time = getTimestampMs();
        while (getTimestampMs() - start_time < TIMEOUT_MS) {
            Packet resp_pkt;
            if (recvPacket(resp_pkt)) {
                if (!verifyChecksum(resp_pkt)) {
                    wprintf(L"[ERROR][错误] 校验和失败\n");
                    continue;
                }
                
                if ((resp_pkt.flags & FLAG_SYN) && (resp_pkt.flags & FLAG_ACK)) {
                    wprintf(L"[RECV][接收] SYN+ACK (seq=%u, ack=%u)\n", resp_pkt.seq, resp_pkt.ack);
                    recv_seq = resp_pkt.seq;
                    recv_base = recv_seq;
                    send_base = resp_pkt.ack;
                    send_next = send_base;
                    
                    // 发送ACK
                    Packet ack_pkt;
                    memset(&ack_pkt, 0, sizeof(ack_pkt));
                    ack_pkt.seq = send_seq;
                    ack_pkt.ack = recv_seq + 1;
                    ack_pkt.flags = FLAG_ACK;
                    ack_pkt.wnd_size = WINDOW_SIZE;
                    ack_pkt.checksum = calculateChecksum(ack_pkt);
                    
                    if (sendPacket(ack_pkt)) {
                        wprintf(L"[SEND][发送] ACK (seq=%u, ack=%u)\n", ack_pkt.seq, ack_pkt.ack);
                        recv_seq++;
                        // ensure recv_base reflects the next expected seq
                        recv_base = recv_seq;
                        conn_state = ESTABLISHED;
                        return true;
                    }
                    break;
                }
            }
            Sleep(10);
        }
        
        wprintf(L"[ERROR][错误] 连接超时\n");
        return false;
    }
    
    // 监听连接(服务器)
    bool listen(int port) {
        sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(port);
        local_addr.sin_addr.s_addr = INADDR_ANY;  // 绑定到所有接口
        
        if (::bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
            wprintf(L"[ERROR][错误] 绑定失败: %d\n", WSAGetLastError());
            return false;
        }
        conn_state = CLOSED;
        return true;
    }
    
    // 接受连接(服务器)
    bool accept() {
        if (conn_state != CLOSED) {
            wprintf(L"[ERROR][错误] 无效的状态\n");
            return false;
        }
        
        // 等待SYN
        uint64_t start_time = getTimestampMs();
        while (getTimestampMs() - start_time < TIMEOUT_MS * 10) {
            Packet syn_pkt;
            sockaddr_in from_addr;
            if (recvPacket(syn_pkt, &from_addr)) {
                if (!verifyChecksum(syn_pkt)) {
                    wprintf(L"[ERROR][错误] 校验和失败\n");
                    continue;
                }
                
                if (syn_pkt.flags & FLAG_SYN) {
                    wprintf(L"[RECV][接收] SYN (seq=%u)\n", syn_pkt.seq);
                    peer_addr = from_addr;
                    recv_seq = syn_pkt.seq;
                    recv_base = recv_seq;
                    
                    // 发送SYN+ACK
                    Packet synack_pkt;
                    memset(&synack_pkt, 0, sizeof(synack_pkt));
                    synack_pkt.seq = send_seq;
                    synack_pkt.ack = recv_seq + 1;
                    synack_pkt.flags = FLAG_SYN | FLAG_ACK;
                    synack_pkt.wnd_size = WINDOW_SIZE;
                    synack_pkt.checksum = calculateChecksum(synack_pkt);
                    
                    if (sendPacket(synack_pkt)) {
                        wprintf(L"[SEND][发送] SYN+ACK (seq=%u, ack=%u)\n", synack_pkt.seq, synack_pkt.ack);
                        send_base = synack_pkt.seq;
                        send_next = send_base;
                        send_seq++;
                        recv_seq++;
                        conn_state = SYN_RECV;
                    }
                    break;
                }
            }
            Sleep(10);
        }
        
        if (conn_state != SYN_RECV) {
            wprintf(L"[ERROR][错误] SYN 超时\n");
            return false;
        }
        
        // 等待ACK
        start_time = getTimestampMs();
        while (getTimestampMs() - start_time < TIMEOUT_MS) {
            Packet ack_pkt;
            if (recvPacket(ack_pkt)) {
                if (!verifyChecksum(ack_pkt)) {
                    wprintf(L"[ERROR][错误] 校验和失败\n");
                    continue;
                }
                
                if (ack_pkt.flags & FLAG_ACK) {
                    wprintf(L"[RECV][接收] ACK (seq=%u, ack=%u)\n", ack_pkt.seq, ack_pkt.ack);
                    conn_state = ESTABLISHED;
                    send_base = ack_pkt.ack;
                    send_next = send_base;
                    // ensure recv_base is synced with recv_seq after handshake
                    recv_base = recv_seq;
                    return true;
                }
            }
            Sleep(10);
        }
        
        wprintf(L"[ERROR][错误] ACK 超时\n");
        return false;
    }
    
    // 发送数据
    int sendData(const char* data, int len) {
        if (conn_state != ESTABLISHED) {
            wprintf(L"[ERROR][错误] 未连接\n");
            return -1;
        }
        
        int sent = 0;
        while (sent < len) {
            handleRetransmission();
            
            int window_used = (send_next - send_base) & 0xFFFF;
            if (window_used < WINDOW_SIZE) {
                int to_send = (len - sent) > PAYLOAD_SIZE ? PAYLOAD_SIZE : (len - sent);
                
                Packet data_pkt;
                memset(&data_pkt, 0, sizeof(data_pkt));
                data_pkt.seq = send_next;
                data_pkt.ack = recv_seq;
                data_pkt.flags = FLAG_DAT | FLAG_ACK;
                data_pkt.wnd_size = WINDOW_SIZE;
                memcpy(data_pkt.payload, data + sent, to_send);
                data_pkt.checksum = calculateChecksum(data_pkt);
                
                if (sendPacket(data_pkt)) {
                    int idx = (send_next - send_base) & (WINDOW_SIZE - 1);
                    send_window[idx].pkt = data_pkt;
                    send_window[idx].send_time = getTimestampMs();
                    send_window[idx].retrans_count = 0;
                    
                    wprintf(L"[SEND][发送] 数据 (seq=%u, len=%d)\n", send_next, to_send);
                    send_next++;
                    sent += to_send;
                } else {
                    break;
                }
            } else {
                Packet ack_pkt;
                if (recvPacket(ack_pkt)) {
                    processAck(ack_pkt);
                }
                Sleep(10);
            }
        }
        
        return sent;
    }
    
    // 接收数据
    int recvData(char* buffer, int max_len) {
        if (conn_state != ESTABLISHED) {
            wprintf(L"[ERROR][错误] 未连接\n");
            return -1;
        }
        
        int received = 0;
        memset(buffer, 0, max_len);
        
        uint64_t start_time = getTimestampMs();
        while (getTimestampMs() - start_time < TIMEOUT_MS * 2 && received < max_len) {
            Packet data_pkt;
            if (recvPacket(data_pkt)) {
                if (!verifyChecksum(data_pkt)) {
                    wprintf(L"[ERROR][错误] 校验和失败\n");
                    continue;
                }
                
                // Handle connection close (FIN)
                if (data_pkt.flags & FLAG_FIN) {
                    // Reply with FIN+ACK
                    Packet finack_pkt;
                    memset(&finack_pkt, 0, sizeof(finack_pkt));
                    finack_pkt.seq = send_next;
                    finack_pkt.ack = data_pkt.seq + 1;
                    finack_pkt.flags = FLAG_FIN | FLAG_ACK;
                    finack_pkt.wnd_size = WINDOW_SIZE;
                    finack_pkt.checksum = calculateChecksum(finack_pkt);
                    sendPacket(finack_pkt);
                    wprintf(L"[RECV][接收] FIN (seq=%u) - 已回复 FIN+ACK\n", data_pkt.seq);
                    conn_state = FIN_RECV;
                    continue;
                }

                if (data_pkt.flags & FLAG_DAT) {
                    if (isInWindow(data_pkt.seq, recv_base)) {
                        int idx = (data_pkt.seq - recv_base) & (WINDOW_SIZE - 1);
                        if (!recv_window[idx].received) {
                            recv_window[idx].pkt = data_pkt;
                            recv_window[idx].received = true;
                            wprintf(L"[RECV][接收] 数据 (seq=%u)\n", data_pkt.seq);
                        }
                    }
                    
                    sendSelectiveAck();
                    
                    while (recv_window[0].received) {
                        int pkt_len = 0;
                        for (int i = 0; i < PAYLOAD_SIZE; i++) {
                            if (recv_window[0].pkt.payload[i] != 0) {
                                pkt_len = i + 1;
                            }
                        }
                        
                        if (pkt_len == 0) {
                            for (int i = 0; i < PAYLOAD_SIZE && received < max_len; i++) {
                                if (recv_window[0].pkt.payload[i] != 0) {
                                    buffer[received++] = recv_window[0].pkt.payload[i];
                                }
                            }
                        } else {
                            for (int i = 0; i < pkt_len && received < max_len; i++) {
                                buffer[received++] = recv_window[0].pkt.payload[i];
                            }
                        }
                        
                        recv_window[0].received = false;
                        for (int i = 0; i < WINDOW_SIZE - 1; i++) {
                            recv_window[i] = recv_window[i + 1];
                        }
                        recv_window[WINDOW_SIZE - 1].received = false;
                        
                        recv_base++;
                    }
                    
                    if (received > 0) {
                        return received;
                    }
                }
            }
            
            if (data_pkt.flags & FLAG_ACK) {
                processAck(data_pkt);
            }
            
            Sleep(10);
        }
        
        return received;
    }
    
    // 处理ACK
    void processAck(const Packet& ack_pkt) {
        if (!verifyChecksum(ack_pkt)) {
            return;
        }
        
        wprintf(L"[RECV][接收] ACK (ack=%u)\n", ack_pkt.ack);
        
        if (ack_pkt.ack > send_base) {
            int newly_acked = (ack_pkt.ack - send_base) & 0xFFFF;
            
            if (cong_state == SLOW_START) {
                cwnd += newly_acked;
                if (cwnd >= ssthresh) {
                    cong_state = CONGESTION_AVOIDANCE;
                    wprintf(L"[CONG][拥塞] 拥塞避免 (cwnd=%d)\n", cwnd);
                }
            } else if (cong_state == CONGESTION_AVOIDANCE) {
                cwnd += (newly_acked / cwnd > 0) ? 1 : 0;
            }
            
            send_base = ack_pkt.ack;
            dup_ack_count = 0;
            
            wprintf(L"[CONG][拥塞] cwnd=%d\n", cwnd);
        } else if (ack_pkt.ack == send_base) {
            dup_ack_count++;
            if (dup_ack_count == 3 && cong_state != FAST_RECOVERY) {
                wprintf(L"[CONG][拥塞] 3 个重复 ACK，快速重传\n");
                ssthresh = cwnd / 2;
                cwnd = ssthresh + 3;
                cong_state = FAST_RECOVERY;
                retransmitPacket(send_base);
            }
        }
    }
    
    // 处理超时重传
    void handleRetransmission() {
        uint64_t now = getTimestampMs();
        
        for (int i = 0; i < WINDOW_SIZE; i++) {
            if (send_window[i].pkt.flags != 0) {
                uint16_t seq = send_window[i].pkt.seq;
                
                if (((seq - send_base) & 0xFFFF) >= ((send_next - send_base) & 0xFFFF)) {
                    continue;
                }
                
                if ((now - send_window[i].send_time) > TIMEOUT_MS) {
                    wprintf(L"[TIMEOUT][超时] seq=%u 超时\n", seq);
                    retransmitPacket(seq);
                }
            }
        }
    }
    
    // 重传指定序列号的数据包
    void retransmitPacket(uint16_t seq) {
        if (!isInWindow(seq, send_base)) {
            return;
        }
        
        int idx = (seq - send_base) & (WINDOW_SIZE - 1);
        Packet& pkt = send_window[idx].pkt;
        
        if (sendPacket(pkt)) {
            send_window[idx].send_time = getTimestampMs();
            send_window[idx].retrans_count++;
            
            if (send_window[idx].retrans_count == 1) {
                ssthresh = cwnd / 2;
                cwnd = 1;
                cong_state = SLOW_START;
                wprintf(L"[CONG][拥塞] 超时，慢启动 (cwnd=%d, ssthresh=%d)\n", cwnd, ssthresh);
            }
        }
    }
    
    // 发送选择确认
    void sendSelectiveAck() {
        Packet ack_pkt;
        memset(&ack_pkt, 0, sizeof(ack_pkt));
        ack_pkt.seq = send_next;
        ack_pkt.ack = recv_base;
        
        int sack_idx = 0;
        for (int i = 1; i < WINDOW_SIZE && sack_idx < PAYLOAD_SIZE; i++) {
            if (recv_window[i].received) {
                ack_pkt.payload[sack_idx++] = i;
            }
        }
        
        ack_pkt.flags = FLAG_ACK;
        ack_pkt.wnd_size = WINDOW_SIZE;
        ack_pkt.checksum = calculateChecksum(ack_pkt);
        
        sendPacket(ack_pkt);
    }
    
    // 关闭连接(客户端)
    bool closeConnection() {
        if (conn_state != ESTABLISHED) {
            wprintf(L"[ERROR][错误] 无效的状态\n");
            return false;
        }
        
        Packet fin_pkt;
        memset(&fin_pkt, 0, sizeof(fin_pkt));
        fin_pkt.seq = send_next;
        fin_pkt.ack = recv_seq;
        fin_pkt.flags = FLAG_FIN | FLAG_ACK;
        fin_pkt.wnd_size = WINDOW_SIZE;
        fin_pkt.checksum = calculateChecksum(fin_pkt);
        
        if (!sendPacket(fin_pkt)) {
            wprintf(L"[ERROR][错误] 发送 FIN 失败\n");
            return false;
        }
        
        wprintf(L"[SEND][发送] FIN (seq=%u)\n", fin_pkt.seq);
        conn_state = FIN_SENT;
        send_next++;
        
        uint64_t start_time = getTimestampMs();
        while (getTimestampMs() - start_time < TIMEOUT_MS) {
            Packet resp_pkt;
            if (recvPacket(resp_pkt)) {
                if (!verifyChecksum(resp_pkt)) {
                    continue;
                }
                
                if ((resp_pkt.flags & FLAG_FIN) && (resp_pkt.flags & FLAG_ACK)) {
                    wprintf(L"[RECV][接收] FIN+ACK (seq=%u, ack=%u)\n", resp_pkt.seq, resp_pkt.ack);
                    
                    Packet ack_pkt;
                    memset(&ack_pkt, 0, sizeof(ack_pkt));
                    ack_pkt.seq = send_next;
                    ack_pkt.ack = resp_pkt.seq + 1;
                    ack_pkt.flags = FLAG_ACK;
                    ack_pkt.wnd_size = WINDOW_SIZE;
                    ack_pkt.checksum = calculateChecksum(ack_pkt);
                    
                    sendPacket(ack_pkt);
                    wprintf(L"[SEND][发送] ACK\n");
                    
                    conn_state = CLOSED;
                    return true;
                }
            }
            Sleep(10);
        }
        
        wprintf(L"[ERROR][错误] 关闭超时\n");
        return false;
    }
    
    ConnectionState getState() const {
        return conn_state;
    }
};

#endif // RELIABLE_TRANSPORT_H
