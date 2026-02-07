#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include "esp_http_server.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_LOG_SCAN = 0,
    EVENT_LOG_START,
    EVENT_LOG_STOP,
    EVENT_LOG_TARGET
} event_log_type_t;

typedef enum {
    EVENT_LOG_SOURCE_UI = 0,
    EVENT_LOG_SOURCE_TIMEOUT
} event_log_source_t;

typedef struct {
    uint64_t timestamp_ms;
    uint32_t duration_ms;
    event_log_type_t type;
    event_log_source_t source;
    char target[64];
} event_log_entry_t;

void event_log_init(void);
void event_log_add(event_log_type_t type, event_log_source_t source, const char *target, uint32_t duration_ms);
size_t event_log_format_csv(char *buffer, size_t buffer_len);
size_t event_log_format_json(char *buffer, size_t buffer_len);
esp_err_t event_log_handle_download(httpd_req_t *req);
esp_err_t event_log_register_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif
