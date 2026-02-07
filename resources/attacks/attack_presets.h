#ifndef ATTACK_PRESETS_H
#define ATTACK_PRESETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    const char *label;
    uint32_t duration_ms;
    uint32_t interval_ms;
    uint8_t channel;
} attack_preset_t;

typedef bool (*attack_preset_apply_cb_t)(const char *preset_id, const char *target, const attack_preset_t *preset);

const attack_preset_t *attack_preset_lookup(const char *preset_id);
const attack_preset_t *attack_preset_get_all(size_t *count);
esp_err_t attack_preset_register_handlers(httpd_handle_t server, attack_preset_apply_cb_t apply_cb);

#ifdef __cplusplus
}
#endif

#endif
