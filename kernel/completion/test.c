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

#include "completion.h"
#include <ttos.h>
#include <ttos_init.h>

// #define DEBUG_COMP
#ifdef DEBUG_COMP
static T_TTOS_CompletionControl comp_cb;

void task1 (void *param)
{
    T_TTOS_ReturnCode ret;

    while(1)
    {
        ret = TTOS_WaitCompletion(&comp_cb, TTOS_WAIT_FOREVER);
        if(ret == TTOS_OK)
        {
            printk("wait completion success\n");
        }
        else
        {
            printk("wait completion failed ret:%d\n", ret);
            while(1);
        }
    }
}

int completion_test(void)
{

    TASK_ID           tid1;

    TTOS_InitCompletion(&comp_cb);

    TTOS_CreateTaskEx ("test_comp1", 225, TRUE, TRUE, task1, NULL, 81920, &tid1);

    while(1)
    {
        sleep(1);
        TTOS_ReleaseCompletion(&comp_cb);
    }
    return 0;
}
USER_INIT_EXPORT(completion_test, "completion_test");
#endif
