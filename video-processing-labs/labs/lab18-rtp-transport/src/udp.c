/* lab18 UDP: WinSock loopback, SO_REUSEADDR + bind retry */
#include "udp.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>

static int g_wsa;

int net_init(void) {
    if (g_wsa) return 0;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
    g_wsa = 1;
    return 0;
}

void net_cleanup(void) {
    if (g_wsa) {
        WSACleanup();
        g_wsa = 0;
    }
}

int udp_open(udp_sock *u) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;
    u->fd = (intptr_t)s;
    return 0;
}

void udp_close(udp_sock *u) {
    if (u && u->fd) {
        closesocket((SOCKET)u->fd);
        u->fd = 0;
    }
}

int udp_bind(udp_sock *u, uint16_t port, uint16_t *bound_port) {
    int reuse = 1;
    setsockopt((SOCKET)u->fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    if (bind((SOCKET)u->fd, (struct sockaddr *)&a, sizeof(a)) != 0) return -1;
    if (bound_port) {
        struct sockaddr_in b;
        int blen = (int)sizeof(b);
        if (getsockname((SOCKET)u->fd, (struct sockaddr *)&b, &blen) == 0)
            *bound_port = ntohs(b.sin_port);
        else
            *bound_port = port;
    }
    return 0;
}

int udp_sendto(udp_sock *u, const uint8_t *buf, int n, uint16_t port) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    int r = sendto((SOCKET)u->fd, (const char *)buf, n, 0, (struct sockaddr *)&a, sizeof(a));
    return r;
}

int udp_recv(udp_sock *u, uint8_t *buf, int cap, int timeout_ms) {
    /* Windows SO_RCVTIMEO==0 means infinite; map 0 → 1ms non-blocking-ish */
    DWORD tv = (timeout_ms <= 0) ? 1u : (DWORD)timeout_ms;
    setsockopt((SOCKET)u->fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
    struct sockaddr_in from;
    int flen = (int)sizeof(from);
    int r = recvfrom((SOCKET)u->fd, (char *)buf, cap, 0, (struct sockaddr *)&from, &flen);
    return r;
}
