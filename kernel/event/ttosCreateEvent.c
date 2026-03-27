/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <ttos.h>

T_TTOS_EventControl *TTOS_CreateEvent(T_VOID)
{
    T_TTOS_EventControl *event_cb;

    event_cb = malloc(sizeof(T_TTOS_EventControl));
    if (event_cb == NULL)
    {
        return event_cb;
    }

    event_cb->wait_list = LIST_HEAD_INIT (event_cb->wait_list);
    event_cb->pendingEvents = 0;

    return event_cb;
}
