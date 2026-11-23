#include "mtk_bridge.h"

#include <stddef.h>

#include <tk/tkernel.h>

bool
mtk_bridge_start_task(void (*p_task)(int, void *),
                      int priority, int stack_size)
{
    bool b_is_started = false;
    ID task_id = 0;
    T_CTSK task_config = {0};

    task_config.itskpri = priority;
    task_config.stksz = stack_size;
    task_config.task = p_task;
    task_config.tskatr = TA_HLNG | TA_RNG3;
    task_id = tk_cre_tsk(&task_config);
    if (E_OK < task_id)
    {
        if (E_OK == tk_sta_tsk(task_id, 0))
        {
            b_is_started = true;
        }
        else
        {
            (void)tk_del_tsk(task_id);
        }
    }

    return (b_is_started);
}

int32_t
mtk_bridge_create_mutex(void)
{
    T_CMTX mutex_config = {0};

    mutex_config.mtxatr = TA_INHERIT;
    return ((int32_t)tk_cre_mtx(&mutex_config));
}

void
mtk_bridge_delete_mutex(int32_t mutex_id)
{
    (void)tk_del_mtx((ID)mutex_id);
}

bool
mtk_bridge_lock_mutex(int32_t mutex_id, bool b_wait)
{
    TMO timeout = TMO_POL;

    if (true == b_wait)
    {
        timeout = TMO_FEVR;
    }

    return (E_OK == tk_loc_mtx((ID)mutex_id, timeout));
}

void
mtk_bridge_unlock_mutex(int32_t mutex_id)
{
    (void)tk_unl_mtx((ID)mutex_id);
}

void
mtk_bridge_delay_ms(uint32_t delay_ms)
{
    (void)tk_dly_tsk((RELTIM)delay_ms);
}

void
mtk_bridge_sleep_forever(void)
{
    (void)tk_slp_tsk(TMO_FEVR);
}
