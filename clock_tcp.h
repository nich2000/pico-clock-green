#ifndef CLOCK_TCP_H
#define CLOCK_TCP_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"

// #define SERVER_IP     "192.168.1.64"
#define SERVER_IP     "192.168.1.45"
#define SERVER_PORT   45678

#define RECONNECT_DELAY_MS      1000
#define WIFI_RECONNECT_DELAY_MS 5000
#define WIFI_TIEOUT_MS          5000
#define WIFI_TRY_LIMIT          30

typedef enum {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    TCP_DISCONNECTED,
    TCP_CONNECTING,
    TCP_CONNECTED
} app_state_t;

bool wifi_connect();
bool tcp_client_connect();

#endif // CLOCK_TCP_H
