#ifndef MTK_BRIDGE_H
#define MTK_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

bool mtk_bridge_start_task(void (*p_task)(int, void *),
                           int priority, int stack_size);
int32_t mtk_bridge_create_mutex(void);
void mtk_bridge_delete_mutex(int32_t mutex_id);
bool mtk_bridge_lock_mutex(int32_t mutex_id, bool b_wait);
void mtk_bridge_unlock_mutex(int32_t mutex_id);
void mtk_bridge_delay_ms(uint32_t delay_ms);
void mtk_bridge_sleep_forever(void);

#endif /* MTK_BRIDGE_H */
