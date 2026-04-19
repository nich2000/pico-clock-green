#include "clock_tcp.h"
#include "Pico-Clock-Green.h"
#include <strings.h>
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"

app_state_t network_state = WIFI_DISCONNECTED;
struct tcp_pcb *client_pcb = NULL;

static void tcp_reset_connection_state(void)
{
    client_pcb = NULL;
    network_state = TCP_DISCONNECTED;
}

static void tcp_close_connection(struct tcp_pcb *tpcb)
{
    if (!tpcb) {
        tcp_reset_connection_state();
        return;
    }

    tcp_arg(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_err(tpcb, NULL);

    err_t close_err = tcp_close(tpcb);
    if (close_err != ERR_OK) {
        printf("tcp_close failed: %d, aborting\n", close_err);
        tcp_abort(tpcb);
    }

    tcp_reset_connection_state();
}

static void tcp_err_cb(void *arg, err_t err)
{
    printf("TCP error: %d\n", err);
    tcp_reset_connection_state();
}

static void trim_command(char *command)
{
    size_t len;

    while (*command == ' ' || *command == '\t') {
        memmove(command, command + 1, strlen(command));
    }

    len = strlen(command);
    while (len > 0) {
        char ch = command[len - 1];
        if (ch != ' ' && ch != '\t') {
            break;
        }
        command[len - 1] = '\0';
        len--;
    }
}

static void process_tcp_command(char *command)
{
    unsigned char hour = 0;
    unsigned char minute = 0;
    unsigned char second_value = 0;
    unsigned char second = 0;
    unsigned char repeat = 0;
    unsigned short duration = 0;

    trim_command(command);
    if (command[0] == '\0') {
        return;
    }

    if (sscanf(command, "COUNTDOWN ON %hhu %hhu", &minute, &second) == 2) {
        printf("%s\n", command);
        switch_on_countdown_mode(minute, second);
        return;
    }

    if (strncasecmp(command, "COUNTDOWN OFF", 13) == 0) {
        printf("%s\n", command);
        switch_off_countdown_mode();
        return;
    }

    if (sscanf(command, "beep %hhu %hu", &repeat, &duration) == 2 ||
        sscanf(command, "BEEP %hhu %hu", &repeat, &duration) == 2) {
        printf("BEEP from TCP: repeat=%u duration=%u\n", repeat, duration);
        beep_start(repeat, duration);
        return;
    }

    if (sscanf(command, "clock %hhu %hhu %hhu", &hour, &minute, &second_value) == 3 ||
        sscanf(command, "CLOCK %hhu %hhu %hhu", &hour, &minute, &second_value) == 3) {
        if (set_clock_time_from_network(hour, minute, second_value)) {
            printf("RTC updated from TCP: %02u:%02u:%02u\n", hour, minute, second_value);
        } else {
            printf("Invalid clock command: %s\n", command);
        }
        return;
    }

    if (sscanf(command, "clock %hhu %hhu", &hour, &minute) == 2 ||
        sscanf(command, "CLOCK %hhu %hhu", &hour, &minute) == 2) {
        if (set_clock_time_from_network(hour, minute, 0)) {
            printf("RTC updated from TCP: %02u:%02u:%02u\n", hour, minute, 0);
        } else {
            printf("Invalid clock command: %s\n", command);
        }
        return;
    }

    // if(command == NULL || strncasecmp(command, "PING", 4) == 0) {
    //     return;
    // }

    // printf("Unknown TCP command: %s\n", command);
}

bool wifi_connect()
{
    printf("Connecting to WiFi %s...\n", WIFI_SSID);

    int err = cyw43_arch_wifi_connect_timeout_ms(
                        WIFI_SSID,
                        WIFI_PASSWORD,
                        CYW43_AUTH_WPA2_AES_PSK,
                        WIFI_TIEOUT_MS); 
    if (err != ERR_OK) 
    {
        printf("WiFi connect failed: %d\n", err);
        return false;
    }

    const ip4_addr_t *ip = netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA]);
    printf("WiFi connected, IP: %s\n", ip4addr_ntoa(ip));
    return true;
}

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p) {
        printf("Server closed connection\n");
        tcp_close_connection(tpcb);
        return ERR_OK;
    }

    char buffer[128] = {0};
    uint16_t copy_len = p->tot_len;
    if (copy_len >= sizeof(buffer)) {
        copy_len = sizeof(buffer) - 1;
    }
    pbuf_copy_partial(p, buffer, copy_len, 0);
    buffer[copy_len] = '\0';

    char *line = buffer;
    while (line && *line != '\0') {
        char *next = strpbrk(line, "\r\n");
        if (next) {
            *next = '\0';
            next++;
            while (*next == '\r' || *next == '\n') {
                next++;
            }
        }
        process_tcp_command(line);
        line = next;
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static err_t tcp_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    if (err != ERR_OK) 
    {
        printf("TCP connect failed: %d\n", err);
        tcp_close_connection(tpcb);
        return err;
    }

    printf("TCP_CONNECTED\n");

    tcp_recv(tpcb, tcp_recv_cb);

    static const char hello_msg[] = "HELLO " CLOCK_VERSION "\r\n\0";

    err_t write_err = tcp_write(tpcb, hello_msg, sizeof(hello_msg) - 1, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("Failed to queue HELLO: %d\n", write_err);
        network_state = TCP_DISCONNECTED;
        return write_err;
    }

    err_t output_err = tcp_output(tpcb);
    if (output_err != ERR_OK) {
        printf("Failed to send HELLO: %d\n", output_err);
        network_state = TCP_DISCONNECTED;
        return output_err;
    }
    printf("%s\n", hello_msg);

    network_state = TCP_CONNECTED;

    return ERR_OK;
}

bool tcp_client_connect()
{
    ip_addr_t server_ip;
    ipaddr_aton(SERVER_IP, &server_ip);

    client_pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!client_pcb) {
        printf("Failed to create PCB\n");
        return false;
    }

    tcp_arg(client_pcb, NULL);
    tcp_err(client_pcb, tcp_err_cb);

    printf("Connecting to server %s:%d...\n", SERVER_IP, SERVER_PORT);

    err_t err = tcp_connect(client_pcb, &server_ip, SERVER_PORT, tcp_connected_cb);
    if (err != ERR_OK) {
        printf("Connecting error: %d\n", err);
        tcp_close_connection(client_pcb);
        return false;
    }

    return true;
}
