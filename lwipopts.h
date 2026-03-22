#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Без RTOS
#define NO_SYS                          1

// Используем Pico таймеры
#define LWIP_TIMERS                     1

// Сокеты (можно отключить если не нужны)
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0

// Память
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        4000

// TCP
#define LWIP_TCP                        1
#define TCP_TTL                         255
#define TCP_QUEUE_OOSEQ                 0

// Буферы
#define TCP_MSS                         1460
#define TCP_SND_BUF                     (2 * TCP_MSS)
#define TCP_WND                         (2 * TCP_MSS)

// ARP/IP
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_IPV4                       1

// DHCP (важно для WiFi)
#define LWIP_DHCP                       1

// DNS
#define LWIP_DNS                        1

// ICMP (ping)
#define LWIP_ICMP                       1

#endif // LWIPOPTS_H