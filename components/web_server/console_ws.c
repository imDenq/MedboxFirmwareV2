#include "console_ws.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#define CONSOLE_RINGBUF_SIZE 4096
#define CONSOLE_LINE_MAX 256
#define CONSOLE_MAX_CLIENTS 8

static httpd_handle_t s_server = NULL;
static RingbufHandle_t s_ringbuf = NULL;
static vprintf_like_t s_orig_vprintf = NULL;

// Hook de log : sortie serie normale ET copie de la ligne dans le ring buffer.
static int console_vprintf(const char *fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    // 1) sortie serie normale (consomme args)
    int ret = s_orig_vprintf ? s_orig_vprintf(fmt, args) : vprintf(fmt, args);

    // 2) copie best-effort vers le ring buffer (non bloquant, drop si plein)
    if (s_ringbuf != NULL) {
        char line[CONSOLE_LINE_MAX];
        int n = vsnprintf(line, sizeof(line), fmt, args_copy);
        if (n > 0) {
            size_t len = (n < (int)sizeof(line)) ? (size_t)n : sizeof(line) - 1;
            xRingbufferSend(s_ringbuf, line, len, 0);
        }
    }

    va_end(args_copy);
    return ret;
}

// Tache d'envoi : draine le ring buffer et pousse chaque ligne aux clients WS.
static void console_sender_task(void *arg)
{
    (void)arg;
    while (1) {
        size_t len = 0;
        char *item = (char *)xRingbufferReceive(s_ringbuf, &len, portMAX_DELAY);
        if (item == NULL) {
            continue;
        }

        if (s_server != NULL) {
            size_t num = CONSOLE_MAX_CLIENTS;
            int client_fds[CONSOLE_MAX_CLIENTS];
            if (httpd_get_client_list(s_server, &num, client_fds) == ESP_OK) {
                httpd_ws_frame_t frame = {
                    .type = HTTPD_WS_TYPE_TEXT,
                    .payload = (uint8_t *)item,
                    .len = len,
                };
                for (size_t i = 0; i < num; i++) {
                    if (httpd_ws_get_fd_info(s_server, client_fds[i]) ==
                        HTTPD_WS_CLIENT_WEBSOCKET) {
                        httpd_ws_send_frame_async(s_server, client_fds[i], &frame);
                    }
                }
            }
        }

        vRingbufferReturnItem(s_ringbuf, item);
    }
}

void console_ws_start(void)
{
    if (s_ringbuf != NULL) {
        return;  // deja demarre
    }
    s_ringbuf = xRingbufferCreate(CONSOLE_RINGBUF_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (s_ringbuf == NULL) {
        return;
    }
    s_orig_vprintf = esp_log_set_vprintf(console_vprintf);
    xTaskCreate(console_sender_task, "console_ws", 4096, NULL, 3, NULL);
}

// --- Handlers HTTP ---

// Page /console : terminal minimal qui se connecte en ws:// et affiche les logs.
static esp_err_t console_page_handler(httpd_req_t *req)
{
    static const char PAGE[] =
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Console Medbox</title><style>"
        "body{margin:0;background:#0b0f14;color:#c8d6e5;font:12px/1.5 monospace}"
        "#bar{position:sticky;top:0;background:#111827;padding:6px 10px;color:#9ca3af}"
        "#log{padding:8px 10px;white-space:pre-wrap;word-break:break-word}"
        "</style></head><body>"
        "<div id=bar>Console Medbox &mdash; <span id=st>connexion...</span></div>"
        "<div id=log></div><script>"
        "var l=document.getElementById('log'),s=document.getElementById('st');"
        "function a(t){var d=document.createElement('div');d.textContent=t;l.appendChild(d);"
        "while(l.childNodes.length>500)l.removeChild(l.firstChild);"
        "window.scrollTo(0,document.body.scrollHeight);}"
        "function c(){var w=new WebSocket('ws://'+location.host+'/ws/console');"
        "w.onopen=function(){s.textContent='connecte';};"
        "w.onmessage=function(e){a(e.data);};"
        "w.onclose=function(){s.textContent='deconnecte, reconnexion...';setTimeout(c,2000);};}"
        "c();</script></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

// Endpoint WebSocket : la connexion est juste enregistree, l'entrant est ignore.
static esp_err_t console_ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // Handshake gere par le framework — rien a faire.
        return ESP_OK;
    }
    // Vider une eventuelle trame entrante (pas de commandes acceptees ici).
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    if (frame.len > 0 && frame.len < CONSOLE_LINE_MAX) {
        uint8_t buf[CONSOLE_LINE_MAX];
        frame.payload = buf;
        httpd_ws_recv_frame(req, &frame, frame.len);
    }
    return ESP_OK;
}

esp_err_t console_ws_register(httpd_handle_t server)
{
    s_server = server;

    httpd_uri_t page = {
        .uri = "/console",
        .method = HTTP_GET,
        .handler = console_page_handler,
    };
    esp_err_t ret = httpd_register_uri_handler(server, &page);
    if (ret != ESP_OK) {
        return ret;
    }

    httpd_uri_t ws = {
        .uri = "/ws/console",
        .method = HTTP_GET,
        .handler = console_ws_handler,
        .is_websocket = true,
    };
    return httpd_register_uri_handler(server, &ws);
}
