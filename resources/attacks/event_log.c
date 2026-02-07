#include "event_log.h"

#include "esp_http_server.h"
#include "esp_timer.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define EVENT_LOG_MAX_ENTRIES 128
#define EVENT_LOG_RESPONSE_CAPACITY 4096

static event_log_entry_t log_entries[EVENT_LOG_MAX_ENTRIES];
static size_t log_head = 0;
static size_t log_count = 0;
static bool log_initialized = false;

static const char *event_type_to_string(event_log_type_t type) {
    switch (type) {
        case EVENT_LOG_SCAN:
            return "scan";
        case EVENT_LOG_START:
            return "start";
        case EVENT_LOG_STOP:
            return "stop";
        case EVENT_LOG_TARGET:
            return "target";
        default:
            return "unknown";
    }
}

static const char *event_source_to_string(event_log_source_t source) {
    switch (source) {
        case EVENT_LOG_SOURCE_UI:
            return "ui";
        case EVENT_LOG_SOURCE_TIMEOUT:
            return "timeout";
        default:
            return "unknown";
    }
}

void event_log_init(void) {
    if (log_initialized) {
        return;
    }
    memset(log_entries, 0, sizeof(log_entries));
    log_head = 0;
    log_count = 0;
    log_initialized = true;
}

void event_log_add(event_log_type_t type, event_log_source_t source, const char *target, uint32_t duration_ms) {
    if (!log_initialized) {
        event_log_init();
    }

    event_log_entry_t *entry = &log_entries[log_head];
    entry->timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000);
    entry->duration_ms = duration_ms;
    entry->type = type;
    entry->source = source;

    if (target && target[0] != '\0') {
        snprintf(entry->target, sizeof(entry->target), "%s", target);
    } else {
        snprintf(entry->target, sizeof(entry->target), "%s", "-");
    }

    log_head = (log_head + 1) % EVENT_LOG_MAX_ENTRIES;
    if (log_count < EVENT_LOG_MAX_ENTRIES) {
        log_count++;
    }
}

static size_t log_index_to_storage(size_t index) {
    if (log_count < EVENT_LOG_MAX_ENTRIES) {
        return index;
    }
    return (log_head + index) % EVENT_LOG_MAX_ENTRIES;
}

size_t event_log_format_csv(char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return 0;
    }

    size_t offset = 0;
    int written = snprintf(buffer + offset, buffer_len - offset,
                           "timestamp_ms,event,source,target,duration_ms\n");
    if (written < 0) {
        return 0;
    }
    offset += (size_t)written;

    for (size_t i = 0; i < log_count && offset < buffer_len; i++) {
        const event_log_entry_t *entry = &log_entries[log_index_to_storage(i)];
        written = snprintf(buffer + offset, buffer_len - offset,
                           "%llu,%s,%s,%s,%lu\n",
                           (unsigned long long)entry->timestamp_ms,
                           event_type_to_string(entry->type),
                           event_source_to_string(entry->source),
                           entry->target,
                           (unsigned long)entry->duration_ms);
        if (written < 0) {
            break;
        }
        offset += (size_t)written;
    }

    if (offset >= buffer_len) {
        buffer[buffer_len - 1] = '\0';
        return buffer_len - 1;
    }

    return offset;
}

size_t event_log_format_json(char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return 0;
    }

    size_t offset = 0;
    int written = snprintf(buffer + offset, buffer_len - offset, "[\n");
    if (written < 0) {
        return 0;
    }
    offset += (size_t)written;

    for (size_t i = 0; i < log_count && offset < buffer_len; i++) {
        const event_log_entry_t *entry = &log_entries[log_index_to_storage(i)];
        const char *suffix = (i + 1 < log_count) ? "," : "";
        written = snprintf(buffer + offset, buffer_len - offset,
                           "  {\"timestamp_ms\":%llu,\"event\":\"%s\",\"source\":\"%s\",\"target\":\"%s\",\"duration_ms\":%lu}%s\n",
                           (unsigned long long)entry->timestamp_ms,
                           event_type_to_string(entry->type),
                           event_source_to_string(entry->source),
                           entry->target,
                           (unsigned long)entry->duration_ms,
                           suffix);
        if (written < 0) {
            break;
        }
        offset += (size_t)written;
    }

    written = snprintf(buffer + offset, buffer_len - offset, "]\n");
    if (written > 0) {
        offset += (size_t)written;
    }

    if (offset >= buffer_len) {
        buffer[buffer_len - 1] = '\0';
        return buffer_len - 1;
    }

    return offset;
}

esp_err_t event_log_handle_download(httpd_req_t *req) {
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }

    char query[64] = {0};
    char format[16] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "format", format, sizeof(format));
    }

    bool use_json = strcmp(format, "json") == 0;
    static char response_buffer[EVENT_LOG_RESPONSE_CAPACITY];
    size_t response_size = 0;

    if (use_json) {
        response_size = event_log_format_json(response_buffer, sizeof(response_buffer));
        httpd_resp_set_type(req, "application/json");
    } else {
        response_size = event_log_format_csv(response_buffer, sizeof(response_buffer));
        httpd_resp_set_type(req, "text/csv");
    }

    if (response_size >= sizeof(response_buffer) - 1) {
        httpd_resp_set_hdr(req, "X-Log-Truncated", "true");
    }

    return httpd_resp_send(req, response_buffer, response_size);
}

esp_err_t event_log_register_handlers(httpd_handle_t server) {
    if (!server) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t log_uri = {
        .uri = "/logs",
        .method = HTTP_GET,
        .handler = event_log_handle_download,
        .user_ctx = NULL
    };

    return httpd_register_uri_handler(server, &log_uri);
}
