#include "clock_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/sync.h"
#include "lwip/ip4_addr.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico/stdlib.h"

#include "clock_tcp.h"
#include "define.h"
#include "Pico-Clock-Green.h"

// From Pico-Clock-Green.c
extern unsigned char display_buffer[112];
extern app_state_t network_state;

static struct tcp_pcb *http_listen_pcb = NULL;
static bool http_running = false;
static bool http_static_responses_ready = false;

typedef struct http_conn_state {
    const uint8_t *resp;
    size_t resp_len;
    size_t resp_sent;
    bool owns_resp;
} http_conn_state_t;

static uint8_t INDEX_RESPONSE[512];
static size_t INDEX_RESPONSE_LEN = 0;
static uint8_t APP_CSS_RESPONSE[512];
static size_t APP_CSS_RESPONSE_LEN = 0;
static uint8_t APP_JS_RESPONSE[2048];
static size_t APP_JS_RESPONSE_LEN = 0;
static uint8_t NOT_FOUND_RESPONSE[256];
static size_t NOT_FOUND_RESPONSE_LEN = 0;

static const char INDEX_HTML[] =
    "<!doctype html><html><head><meta charset=\"utf-8\"/>"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
    "<title>Pico Clock</title>"
    "<link rel=\"stylesheet\" href=\"/app.css?v=" CLOCK_VERSION "\">"
    "</head><body>"
    "<canvas id=\"c\" width=\"220\" height=\"70\"></canvas>"
    "<script src=\"/app.js?v=" CLOCK_VERSION "\"></script>"
    "</body></html>";

static const char APP_CSS[] =
    "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;margin:16px;}"
    "canvas{image-rendering:pixelated;border:1px solid #ddd;border-radius:8px;"
    "display:block;"
    "width:min(1100px,96vw);"
    "aspect-ratio:22/7;"
    "height:auto;}";

static const char APP_JS[] =
    "const c=document.getElementById('c');"
    "const ctx=c.getContext('2d');"
    "let lastBuf=null;"
    "const GRID_W=22, GRID_H=7, CELL=10, PAD=1, PIX=8;"
    "const COLON_TOP_X=10, COLON_TOP_Y=1, COLON_BOTTOM_X=10, COLON_BOTTOM_Y=4;"
    "function fillPixel(x,y){"
    "  const ox=x*CELL+PAD;"
    "  const oy=y*CELL+PAD;"
    "  ctx.fillRect(ox,oy,PIX,PIX);"
    "}"
    "function draw(buf){"
    "  ctx.clearRect(0,0,c.width,c.height);"
    "  ctx.fillStyle='rgb(245,248,245)';"
    "  ctx.fillRect(0,0,c.width,c.height);"
    "  ctx.fillStyle='rgb(0,200,0)';"
    "  for(let x=0;x<3;x++){"
    "    for(let row=0;row<8;row++){"
    "      const b=buf[x*8+row];"
    "      for(let bit=0;bit<8;bit++){"
    "        const on=(b>>bit)&1;"
    "        const sx=(x*8+bit);"
    "        const sy=row;"
    "        if(sy===0) continue;"          // hide weekdays row
    "        if(sx<2) continue;"            // hide 2 tech columns
    "        const px=sx-2;"
    "        const py=sy-1;"
    "        if(on) fillPixel(px,py);"
    "      }"
    "    }"
    "  }"
    "  for(let x=COLON_TOP_X;x<COLON_TOP_X+2;x++){"
    "    for(let y=COLON_TOP_Y;y<COLON_TOP_Y+2;y++){"
    "      fillPixel(x,y);"
    "    }"
    "  }"
    "  for(let x=COLON_BOTTOM_X;x<COLON_BOTTOM_X+2;x++){"
    "    for(let y=COLON_BOTTOM_Y;y<COLON_BOTTOM_Y+2;y++){"
    "      fillPixel(x,y);"
    "    }"
    "  }"
    "}"
    "function repaint(){"
    "  if(lastBuf) draw(lastBuf);"
    "}"
    "async function tick(){"
    "  try{"
    "    const rbuf=await fetch('/api/buf',{cache:'no-store'});"
    "    const buf=new Uint8Array(await rbuf.arrayBuffer());"
    "    if(buf.length>=112){ lastBuf=buf; repaint(); }"
    "  }catch(e){"
    "    console.log(e);"
    "  }"
    "  setTimeout(tick,10000);"
    "}"
    "tick();";

static void bytes_to_hex(const uint8_t *in, size_t in_len, char *out, size_t out_len)
{
    static const char *HEX = "0123456789abcdef";
    if (out_len < (in_len * 2 + 1)) {
        // Truncate safely
        in_len = (out_len - 1) / 2;
    }
    for (size_t i = 0; i < in_len; i++) {
        out[i * 2 + 0] = HEX[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = HEX[in[i] & 0xF];
    }
    out[in_len * 2] = '\0';
}

static const char *network_state_to_str(app_state_t s)
{
    switch (s) {
        case WIFI_DISCONNECTED: return "WIFI_DISCONNECTED";
        case WIFI_CONNECTING:   return "WIFI_CONNECTING";
        case WIFI_CONNECTED:    return "WIFI_CONNECTED";
        case TCP_DISCONNECTED:  return "TCP_DISCONNECTED";
        case TCP_CONNECTING:    return "TCP_CONNECTING";
        case TCP_CONNECTED:     return "TCP_CONNECTED";
        default:                return "UNKNOWN";
    }
}

static void http_conn_state_free(http_conn_state_t *st)
{
    if (!st) return;
    if (st->owns_resp && st->resp) {
        free((void *)st->resp);
    }
    free(st);
}

static err_t http_conn_close(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    if (!tpcb) return ERR_OK;
    tcp_arg(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_err(tpcb, NULL);
    err_t e = tcp_close(tpcb);
    if (e != ERR_OK) {
        tcp_abort(tpcb);
    }
    http_conn_state_free(st);
    return ERR_OK;
}

static void http_err_cb(void *arg, err_t err)
{
    http_conn_state_t *st = (http_conn_state_t *)arg;
    (void)err;
    http_conn_state_free(st);
}

static err_t http_try_send_more(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    if (!st || !st->resp) return ERR_VAL;
    while (st->resp_sent < st->resp_len) {
        u16_t space = tcp_sndbuf(tpcb);
        if (!space) break;
        size_t remaining = st->resp_len - st->resp_sent;
        u16_t chunk = (remaining > space) ? space : (u16_t)remaining;
        err_t e = tcp_write(tpcb, st->resp + st->resp_sent, chunk, TCP_WRITE_FLAG_COPY);
        if (e == ERR_MEM) break;
        if (e != ERR_OK) return e;
        st->resp_sent += chunk;
    }
    err_t out = tcp_output(tpcb);
    if (out != ERR_OK) return out;
    if (st->resp_sent >= st->resp_len) {
        return http_conn_close(tpcb, st);
    }
    return ERR_OK;
}

static err_t http_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    (void)len;
    http_conn_state_t *st = (http_conn_state_t *)arg;
    return http_try_send_more(tpcb, st);
}

static err_t http_poll_cb(void *arg, struct tcp_pcb *tpcb)
{
    http_conn_state_t *st = (http_conn_state_t *)arg;
    if (!st) {
        return ERR_OK;
    }
    // Try to continue sending if we were previously blocked.
    return http_try_send_more(tpcb, st);
}

static uint8_t *http_build_response_ex(const char *content_type,
                                       const char *cache_control,
                                       const uint8_t *body,
                                       size_t body_len,
                                       size_t *out_len)
{
    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: %s\r\n"
                      "Cache-Control: %s\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %u\r\n"
                      "\r\n",
                      content_type,
                      cache_control ? cache_control : "no-store",
                      (unsigned)body_len);
    if (hn <= 0) return NULL;

    size_t total = (size_t)hn + body_len;
    uint8_t *resp = (uint8_t *)malloc(total);
    if (!resp) return NULL;
    memcpy(resp, hdr, (size_t)hn);
    if (body_len) memcpy(resp + hn, body, body_len);
    if (out_len) *out_len = total;
    return resp;
}

static bool http_build_static_response(uint8_t *dest,
                                       size_t dest_size,
                                       const char *content_type,
                                       const char *cache_control,
                                       const uint8_t *body,
                                       size_t body_len,
                                       size_t *out_len)
{
    char hdr[256];
    int hn = snprintf(hdr, sizeof(hdr),
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: %s\r\n"
                      "Cache-Control: %s\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %u\r\n"
                      "\r\n",
                      content_type,
                      cache_control ? cache_control : "no-store",
                      (unsigned)body_len);
    if (hn <= 0) return false;

    size_t total = (size_t)hn + body_len;
    if (total > dest_size) return false;

    memcpy(dest, hdr, (size_t)hn);
    if (body_len) memcpy(dest + hn, body, body_len);
    if (out_len) *out_len = total;
    return true;
}

static bool http_prepare_static_responses(void)
{
    if (http_static_responses_ready) return true;

    static const uint8_t BODY_404[] = "Not Found";

    bool ok = true;
    ok = ok && http_build_static_response(INDEX_RESPONSE, sizeof(INDEX_RESPONSE),
                                          "text/html; charset=utf-8", "no-store",
                                          (const uint8_t *)INDEX_HTML, sizeof(INDEX_HTML) - 1,
                                          &INDEX_RESPONSE_LEN);
    ok = ok && http_build_static_response(APP_CSS_RESPONSE, sizeof(APP_CSS_RESPONSE),
                                          "text/css; charset=utf-8", "public, max-age=86400",
                                          (const uint8_t *)APP_CSS, sizeof(APP_CSS) - 1,
                                          &APP_CSS_RESPONSE_LEN);
    ok = ok && http_build_static_response(APP_JS_RESPONSE, sizeof(APP_JS_RESPONSE),
                                          "application/javascript; charset=utf-8", "public, max-age=86400",
                                          (const uint8_t *)APP_JS, sizeof(APP_JS) - 1,
                                          &APP_JS_RESPONSE_LEN);
    ok = ok && http_build_static_response(NOT_FOUND_RESPONSE, sizeof(NOT_FOUND_RESPONSE),
                                          "text/plain; charset=utf-8", "no-store",
                                          BODY_404, sizeof(BODY_404) - 1,
                                          &NOT_FOUND_RESPONSE_LEN);
    http_static_responses_ready = ok;
    return ok;
}

static uint8_t *http_build_response(const char *content_type, const uint8_t *body, size_t body_len, size_t *out_len)
{
    return http_build_response_ex(content_type, "no-store", body, body_len, out_len);
}

static bool http_set_response_and_send(struct tcp_pcb *tpcb, http_conn_state_t *st, uint8_t *resp, size_t resp_len, bool owns)
{
    if (!resp || !resp_len) return false;
    st->resp = resp;
    st->resp_len = resp_len;
    st->resp_sent = 0;
    st->owns_resp = owns;
    tcp_sent(tpcb, http_sent_cb);
    // Also register poll to handle cases where sndbuf is temporarily 0
    // and no tcp_sent callback will ever fire (because nothing was queued yet).
    tcp_poll(tpcb, http_poll_cb, 2);
    err_t e = http_try_send_more(tpcb, st);
    return (e == ERR_OK);
}

static bool http_queue_404(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    if (!http_prepare_static_responses()) return false;
    return http_set_response_and_send(tpcb, st, NOT_FOUND_RESPONSE, NOT_FOUND_RESPONSE_LEN, false);
}

static bool http_queue_index(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    if (!http_prepare_static_responses()) return false;
    return http_set_response_and_send(tpcb, st, INDEX_RESPONSE, INDEX_RESPONSE_LEN, false);
}

static bool http_queue_app_css(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    if (!http_prepare_static_responses()) return false;
    return http_set_response_and_send(tpcb, st, APP_CSS_RESPONSE, APP_CSS_RESPONSE_LEN, false);
}

static bool http_queue_app_js(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    if (!http_prepare_static_responses()) return false;
    return http_set_response_and_send(tpcb, st, APP_JS_RESPONSE, APP_JS_RESPONSE_LEN, false);
}

static bool http_queue_state(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    uint8_t snap[sizeof(display_buffer)];
    uint32_t save = save_and_disable_interrupts();
    memcpy(snap, display_buffer, sizeof(snap));
    restore_interrupts(save);

    char hex[sizeof(snap) * 2 + 1];
    bytes_to_hex(snap, sizeof(snap), hex, sizeof(hex));

    char body[1024];
    int bn = snprintf(body, sizeof(body),
                      "{"
                      "\"version\":\"%s\","
                      "\"network_state\":\"%s\","
                      "\"display_buffer_len\":%u,"
                      "\"display_buffer_hex\":\"%s\""
                      "}\n",
                      CLOCK_VERSION,
                      network_state_to_str(network_state),
                      (unsigned)sizeof(snap),
                      hex);
    if (bn < 0) return false;
    if ((size_t)bn >= sizeof(body)) return false;

    size_t resp_len = 0;
    uint8_t *resp = http_build_response("application/json; charset=utf-8",
                                        (const uint8_t *)body,
                                        (size_t)bn,
                                        &resp_len);
    if (!resp) return false;
    return http_set_response_and_send(tpcb, st, resp, resp_len, true);
}

static bool http_queue_buf(struct tcp_pcb *tpcb, http_conn_state_t *st)
{
    uint8_t snap[sizeof(display_buffer)];
    uint32_t save = save_and_disable_interrupts();
    memcpy(snap, display_buffer, sizeof(snap));
    restore_interrupts(save);

    size_t resp_len = 0;
    uint8_t *resp = http_build_response_ex("application/octet-stream", "no-store",
                                          snap, sizeof(snap), &resp_len);
    if (!resp) return false;
    return http_set_response_and_send(tpcb, st, resp, resp_len, true);
}

static err_t http_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    http_conn_state_t *st = (http_conn_state_t *)arg;
    if (err != ERR_OK) {
        if (p) pbuf_free(p);
        http_conn_close(tpcb, st);
        return ERR_OK;
    }

    if (!p) {
        http_conn_close(tpcb, st);
        return ERR_OK;
    }

    if (!st) {
        pbuf_free(p);
        http_conn_close(tpcb, NULL);
        return ERR_OK;
    }

    char req[512];
    size_t copy = p->tot_len;
    if (copy >= sizeof(req)) copy = sizeof(req) - 1;
    pbuf_copy_partial(p, req, (u16_t)copy, 0);
    req[copy] = '\0';

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    // Very small parser: only "GET <path> HTTP/1.x"
    if (strncmp(req, "GET ", 4) != 0) {
        http_queue_404(tpcb, st);
        return ERR_OK;
    }

    const char *path = req + 4;
    const char *sp = strchr(path, ' ');
    if (!sp) {
        http_queue_404(tpcb, st);
        return ERR_OK;
    }

    size_t path_len = (size_t)(sp - path);
    if (path_len == 0 || path_len > 256) {
        http_queue_404(tpcb, st);
        return ERR_OK;
    }

    // Some clients may send an absolute URI in the request line:
    //   GET http://host/path?query HTTP/1.1
    // Normalize to just "/path?query".
    if (path_len > 7 && strncmp(path, "http://", 7) == 0) {
        const char *slash = strstr(path + 7, "/");
        if (slash && slash < sp) {
            path = slash;
            path_len = (size_t)(sp - path);
        }
    } else if (path_len > 8 && strncmp(path, "https://", 8) == 0) {
        const char *slash = strstr(path + 8, "/");
        if (slash && slash < sp) {
            path = slash;
            path_len = (size_t)(sp - path);
        }
    }

    // Accept optional query string (?v=...), match only the path portion.
    size_t path_only_len = path_len;
    for (size_t i = 0; i < path_len; i++) {
        if (path[i] == '?') { path_only_len = i; break; }
    }

    if (path_only_len == 1 && path[0] == '/') {
        http_queue_index(tpcb, st);
        return ERR_OK;
    }

    if (path_only_len == strlen("/app.css") && strncmp(path, "/app.css", path_only_len) == 0) {
        http_queue_app_css(tpcb, st);
        return ERR_OK;
    }
    if (path_only_len == strlen("/app.js") && strncmp(path, "/app.js", path_only_len) == 0) {
        http_queue_app_js(tpcb, st);
        return ERR_OK;
    }
    if (path_only_len == strlen("/api/buf") && strncmp(path, "/api/buf", path_only_len) == 0) {
        http_queue_buf(tpcb, st);
        return ERR_OK;
    }
    if (path_only_len == strlen("/api/state") && strncmp(path, "/api/state", path_only_len) == 0) {
        http_queue_state(tpcb, st);
        return ERR_OK;
    }

    http_queue_404(tpcb, st);
    return ERR_OK;
}

static err_t http_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK || !newpcb) return ERR_VAL;
    http_conn_state_t *st = (http_conn_state_t *)calloc(1, sizeof(http_conn_state_t));
    if (!st) {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }
    tcp_arg(newpcb, st);
    tcp_err(newpcb, http_err_cb);
    tcp_sent(newpcb, http_sent_cb);
    tcp_poll(newpcb, http_poll_cb, 2);
    tcp_recv(newpcb, http_recv_cb);
    return ERR_OK;
}

bool http_server_start(uint16_t port)
{
    if (http_running) return true;

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!pcb) return false;

    err_t e = tcp_bind(pcb, IP_ANY_TYPE, port);
    if (e != ERR_OK) {
        tcp_close(pcb);
        return false;
    }

    http_listen_pcb = tcp_listen_with_backlog(pcb, 2);
    if (!http_listen_pcb) {
        tcp_close(pcb);
        return false;
    }

    tcp_accept(http_listen_pcb, http_accept_cb);
    http_running = true;
    return true;
}

void http_server_stop(void)
{
    if (!http_running) return;
    if (http_listen_pcb) {
        tcp_accept(http_listen_pcb, NULL);
        tcp_close(http_listen_pcb);
        http_listen_pcb = NULL;
    }
    http_running = false;
}

bool http_server_is_running(void)
{
    return http_running;
}
