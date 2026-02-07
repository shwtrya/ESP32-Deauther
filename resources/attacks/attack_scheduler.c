#include "attack_scheduler.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

#ifndef ATTACK_SCHEDULER_MAX_WHITELIST
#define ATTACK_SCHEDULER_MAX_WHITELIST 32
#endif

static attack_start_cb_t start_callback = NULL;
static attack_stop_cb_t stop_callback = NULL;
static esp_timer_handle_t start_timer = NULL;
static esp_timer_handle_t stop_timer = NULL;
static uint64_t epoch_offset_ms = 0;

static struct {
    char entries[ATTACK_SCHEDULER_MAX_WHITELIST][64];
    size_t count;
} whitelist_store = { .entries = {{0}}, .count = 0 };

static attack_schedule_t pending_schedule = { .start_time_ms = 0, .duration_ms = 0, .target = {0} };

static uint64_t scheduler_now_ms(void) {
    return (uint64_t)(esp_timer_get_time() / 1000) + epoch_offset_ms;
}

static void schedule_stop_cb(void *arg) {
    (void)arg;
    if (stop_callback) {
        stop_callback();
    }
}

static void schedule_start_cb(void *arg) {
    (void)arg;
    if (start_callback) {
        start_callback(pending_schedule.target, pending_schedule.duration_ms);
    }

    if (stop_timer && pending_schedule.duration_ms > 0) {
        esp_timer_stop(stop_timer);
        esp_timer_start_once(stop_timer, (uint64_t)pending_schedule.duration_ms * 1000);
    }
}

void attack_scheduler_init(attack_start_cb_t start_cb, attack_stop_cb_t stop_cb) {
    start_callback = start_cb;
    stop_callback = stop_cb;

    if (!start_timer) {
        const esp_timer_create_args_t start_args = {
            .callback = schedule_start_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "attack_start"
        };
        esp_timer_create(&start_args, &start_timer);
    }

    if (!stop_timer) {
        const esp_timer_create_args_t stop_args = {
            .callback = schedule_stop_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "attack_stop"
        };
        esp_timer_create(&stop_args, &stop_timer);
    }
}

void attack_scheduler_set_time_base(uint64_t client_epoch_ms) {
    uint64_t uptime_ms = (uint64_t)(esp_timer_get_time() / 1000);
    epoch_offset_ms = client_epoch_ms > uptime_ms ? (client_epoch_ms - uptime_ms) : 0;
}

void attack_scheduler_set_whitelist(const char **targets, size_t target_count) {
    whitelist_store.count = 0;
    if (!targets || target_count == 0) {
        return;
    }

    size_t count = target_count > ATTACK_SCHEDULER_MAX_WHITELIST
                       ? ATTACK_SCHEDULER_MAX_WHITELIST
                       : target_count;

    for (size_t i = 0; i < count; ++i) {
        if (!targets[i]) {
            continue;
        }
        snprintf(whitelist_store.entries[whitelist_store.count], sizeof(whitelist_store.entries[whitelist_store.count]), "%s", targets[i]);
        whitelist_store.count++;
    }
}

bool attack_scheduler_is_whitelisted(const char *target) {
    if (!target || target[0] == '\0') {
        return false;
    }

    for (size_t i = 0; i < whitelist_store.count; ++i) {
        if (strcasecmp(whitelist_store.entries[i], target) == 0) {
            return true;
        }
    }
    return false;
}

bool attack_scheduler_schedule(const attack_schedule_t *schedule) {
    if (!schedule || schedule->duration_ms == 0 || schedule->target[0] == '\0') {
        return false;
    }

    if (!attack_scheduler_is_whitelisted(schedule->target)) {
        return false;
    }

    pending_schedule = *schedule;
    if (start_timer) {
        esp_timer_stop(start_timer);
    }
    if (stop_timer) {
        esp_timer_stop(stop_timer);
    }

    uint64_t now_ms = scheduler_now_ms();
    uint64_t start_ms = schedule->start_time_ms > now_ms ? schedule->start_time_ms : now_ms;
    uint64_t delay_ms = start_ms - now_ms;

    if (start_timer) {
        esp_timer_start_once(start_timer, delay_ms * 1000);
    }
    return true;
}

void attack_scheduler_cancel(void) {
    if (start_timer) {
        esp_timer_stop(start_timer);
    }
    if (stop_timer) {
        esp_timer_stop(stop_timer);
    }
    memset(&pending_schedule, 0, sizeof(pending_schedule));
}
