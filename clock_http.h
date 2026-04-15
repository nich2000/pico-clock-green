#ifndef CLOCK_HTTP_H
#define CLOCK_HTTP_H

#include <stdbool.h>
#include <stdint.h>

// Minimal HTTP server for Pico W (lwIP raw API).
// Serves:
//  - GET /          : HTML UI
//  - GET /api/state : JSON snapshot (display buffer hex)

bool http_server_start(uint16_t port);
void http_server_stop(void);
bool http_server_is_running(void);

#endif // CLOCK_HTTP_H

