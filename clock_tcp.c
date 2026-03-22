#include "clock_tcp.h"

app_state_t state = WIFI_DISCONNECTED;
struct tcp_pcb *client_pcb = NULL;

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p) {
        printf("Server closed connection\n");
        tcp_close(tpcb);
        client_pcb = NULL;
        state = TCP_DISCONNECTED;
        return ERR_OK;
    }

    // Получение данных
    char buffer[128] = {0};
    memcpy(buffer, p->payload, p->len);

    printf("Received: %s\n", buffer);

    // TODO: обработка команд
    // пример:
    if (strncmp(buffer, "LED ON", 6) == 0) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    } else if (strncmp(buffer, "LED OFF", 7) == 0) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }

    tcp_recved(tpcb, p->len);
    pbuf_free(p);

    return ERR_OK;
}

static err_t tcp_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    if (err != ERR_OK) {
        printf("TCP connect failed\n");
        state = TCP_DISCONNECTED;
        return err;
    }

    printf("TCP connected\n");

    tcp_recv(tpcb, tcp_recv_cb);
    state = TCP_CONNECTED;

    return ERR_OK;
}

bool wifi_connect()
{
    printf("Connecting to WiFi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            10000))
    {
        printf("WiFi connect failed\n");
        return false;
    }

    printf("WiFi connected\n");
    return true;
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

    printf("Connecting to server...\n");

    err_t err = tcp_connect(client_pcb, &server_ip, SERVER_PORT, tcp_connected_cb);
    if (err != ERR_OK) {
        printf("TCP connect error: %d\n", err);
        tcp_close(client_pcb);
        client_pcb = NULL;
        return false;
    }

    state = TCP_CONNECTING;
    return true;
}
