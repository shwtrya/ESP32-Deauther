#include "attack_presets.h"

#include "attack_scheduler.h"
#include "esp_http_server.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static attack_preset_apply_cb_t preset_apply_cb = NULL;

static const attack_preset_t attack_presets[] = {
    {
        .id = "deauth_short",
        .label = "Deauth Short",
        .duration_ms = 15000,
        .interval_ms = 200,
        .channel = 6
    },
    {
        .id = "beacon_burst",
        .label = "Beacon Burst",
        .duration_ms = 30000,
        .interval_ms = 100,
        .channel = 1
    },
    {
        .id = "stealth",
        .label = "Stealth",
        .duration_ms = 60000,
        .interval_ms = 750,
        .channel = 11
    }
};

static void url_decode(char *dest, size_t dest_len, const char *src) {
    if (!dest || dest_len == 0) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }

    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 1 < dest_len; ++si) {
        if (src[si] == '+') {
            dest[di++] = ' ';
            continue;
        }

        if (src[si] == '%' && isxdigit((unsigned char)src[si + 1]) && isxdigit((unsigned char)src[si + 2])) {
            char hex[3] = { src[si + 1], src[si + 2], '\0' };
            dest[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
            continue;
        }

        dest[di++] = src[si];
    }
    dest[di] = '\0';
}

static bool parse_form_value(const char *data, const char *key, char *dest, size_t dest_len) {
    if (!data || !key || !dest || dest_len == 0) {
        return false;
    }

    const size_t key_len = strlen(key);
    const char *cursor = data;

    while (cursor && *cursor) {
        const char *eq = strchr(cursor, '=');
        if (!eq) {
            break;
        }
        size_t name_len = (size_t)(eq - cursor);
        const char *value_start = eq + 1;
        const char *amp = strchr(value_start, '&');
        size_t value_len = amp ? (size_t)(amp - value_start) : strlen(value_start);

        if (name_len == key_len && strncmp(cursor, key, key_len) == 0) {
            char encoded[96] = {0};
            size_t copy_len = value_len < sizeof(encoded) - 1 ? value_len : sizeof(encoded) - 1;
            memcpy(encoded, value_start, copy_len);
            encoded[copy_len] = '\0';
            url_decode(dest, dest_len, encoded);
            return true;
        }

        cursor = amp ? amp + 1 : NULL;
    }

    return false;
}

static bool get_request_param(httpd_req_t *req, const char *key, char *dest, size_t dest_len) {
    if (!req || !key || !dest) {
        return false;
    }

    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, key, dest, dest_len) == ESP_OK) {
            return true;
        }
    }

    if (req->content_len <= 0) {
        return false;
    }

    char body[256] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        return false;
    }
    body[received] = '\0';

    return parse_form_value(body, key, dest, dest_len);
}

const attack_preset_t *attack_preset_lookup(const char *preset_id) {
    if (!preset_id || preset_id[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(attack_presets) / sizeof(attack_presets[0]); ++i) {
        if (strcmp(attack_presets[i].id, preset_id) == 0) {
            return &attack_presets[i];
        }
    }
    return NULL;
}

const attack_preset_t *attack_preset_get_all(size_t *count) {
    if (count) {
        *count = sizeof(attack_presets) / sizeof(attack_presets[0]);
    }
    return attack_presets;
}

static esp_err_t attack_preset_handle(httpd_req_t *req) {
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }

    char preset_id[32] = {0};
    char target[64] = {0};
    if (!get_request_param(req, "preset", preset_id, sizeof(preset_id))) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing preset parameter.");
    }
    if (!get_request_param(req, "target", target, sizeof(target))) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing target parameter.");
    }

    if (!attack_scheduler_is_whitelisted(target)) {
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Target not in whitelist.");
    }

    const attack_preset_t *preset = attack_preset_lookup(preset_id);
    if (!preset) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Preset not found.");
    }

    if (!preset_apply_cb) {
        return httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED, "Preset handler not available.");
    }

    if (!preset_apply_cb(preset_id, target, preset)) {
        return httpd_resp_send_err(req, HTTPD_409_CONFLICT, "Preset rejected by firmware.");
    }

    char response[196];
    snprintf(response, sizeof(response),
             "{\"preset\":\"%s\",\"target\":\"%s\",\"duration_ms\":%lu,\"interval_ms\":%lu,\"channel\":%u}",
             preset->id, target, (unsigned long)preset->duration_ms, (unsigned long)preset->interval_ms, preset->channel);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t attack_preset_register_handlers(httpd_handle_t server, attack_preset_apply_cb_t apply_cb) {
    if (!server) {
        return ESP_ERR_INVALID_ARG;
    }

    preset_apply_cb = apply_cb;

    httpd_uri_t preset_uri = {
        .uri = "/preset",
        .method = HTTP_POST,
        .handler = attack_preset_handle,
        .user_ctx = NULL
    };

    return httpd_register_uri_handler(server, &preset_uri);
}
