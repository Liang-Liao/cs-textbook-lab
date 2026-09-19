#ifndef LAB18_UDP_H
#define LAB18_UDP_H
#include <stddef.h>
#include <stdint.h>

/* WinSock UDP loopback helpers. Call net_init once before any udp_*. */
int net_init(void);
void net_cleanup(void);

typedef struct {
    intptr_t fd; /* SOCKET */
} udp_sock;

int udp_open(udp_sock *u);
void udp_close(udp_sock *u);
/* Bind 127.0.0.1:port with SO_REUSEADDR; port==0 → ephemeral.
 * bound_port receives the actual port on success. */
int udp_bind(udp_sock *u, uint16_t port, uint16_t *bound_port);
int udp_sendto(udp_sock *u, const uint8_t *buf, int n, uint16_t port);
/* timeout_ms < 0 blocks; returns bytes or -1 on timeout/error. */
int udp_recv(udp_sock *u, uint8_t *buf, int cap, int timeout_ms);

#endif
