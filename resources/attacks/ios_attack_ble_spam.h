#ifndef IOS_ATTACK_BLE_SPAM_H
#define IOS_ATTACK_BLE_SPAM_H

#include "event_log.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ios_attack_ble_spam_start(void);
void ios_attack_ble_spam_stop(void);
void ios_attack_ble_spam_start_with_context(const char *target, event_log_source_t source, uint32_t duration_ms);
void ios_attack_ble_spam_stop_with_source(event_log_source_t source);
void ios_attack_ble_spam_log_scan(event_log_source_t source, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
