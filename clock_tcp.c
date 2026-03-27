#include "clock_tcp.h"
#include "Pico-Clock-Green.h"

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

    printf("WiFi connected\n");
    return true;
}

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p) {
        printf("Server closed connection\n");
        tcp_close_connection(tpcb);
        return ERR_OK;
    }

    // Получение данных
    char buffer[128] = {0};
    uint16_t copy_len = p->len;
    if (copy_len >= sizeof(buffer)) {
        copy_len = sizeof(buffer) - 1;
    }
    memcpy(buffer, p->payload, copy_len);
    buffer[copy_len] = '\0';

    printf("Received: %s\n", buffer);

    // TODO: обработка команд
    // пример:
    unsigned char minute = 0;
    unsigned char second = 0;
    if (sscanf(buffer, "COUNTDOWN ON %hhu %hhu", &minute, &second) == 2) {
        switch_on_countdown_mode(minute, second);
    } else if (strncmp(buffer, "COUNTDOWN ON", 12) == 0) {
        switch_on_countdown_mode(0, 10);
    } else if (strncmp(buffer, "COUNTDOWN OFF", 13) == 0) {
        switch_off_countdown_mode();
    }

    tcp_recved(tpcb, p->len);
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

    static const char hello_msg[] = "HELLO\r\n\0";

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
    printf("Sent: %s\n", hello_msg);
    
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

    printf("Connecting to server %s...\n", SERVER_IP);

    err_t err = tcp_connect(client_pcb, &server_ip, SERVER_PORT, tcp_connected_cb);
    if (err != ERR_OK) {
        printf("Connecting error: %d\n", err);
        tcp_close_connection(client_pcb);
        return false;
    }

    return true;
}
