#ifndef ATTACK_SCHEDULER_H
#define ATTACK_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*attack_start_cb_t)(const char *target, uint32_t duration_ms);
typedef void (*attack_stop_cb_t)(void);

typedef struct {
    uint64_t start_time_ms;
    uint32_t duration_ms;
    char target[64];
} attack_schedule_t;

void attack_scheduler_init(attack_start_cb_t start_cb, attack_stop_cb_t stop_cb);
void attack_scheduler_set_time_base(uint64_t client_epoch_ms);
void attack_scheduler_set_whitelist(const char **targets, size_t target_count);
bool attack_scheduler_is_whitelisted(const char *target);
bool attack_scheduler_schedule(const attack_schedule_t *schedule);
void attack_scheduler_cancel(void);

#ifdef __cplusplus
}
#endif

#endif
