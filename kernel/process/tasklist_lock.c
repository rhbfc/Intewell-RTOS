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

#include <cpuid.h>
#include <tasklist_lock.h>
#include <ttos.h>
#include <ttos_init.h>
#include <assert.h>

#undef KLOG_TAG
#define KLOG_TAG "tasklist_lock"
#include <klog.h>

static MUTEX_ID task_lock;
static int task_lock_inited;

int tasklist_lock_init (void)
{
    if (TTOS_OK == TTOS_CreateMutex (1, 0, &task_lock))
    {
        return 0;
    }
    else
    {
        KLOG_E("fail at %s:%d\n",__FILE__,__LINE__);
        //不允许失败
        assert(0);
        return -1;
    }
}

void tasklist_lock(void)                                         
{    
    T_TTOS_ReturnCode ret;

    if (!task_lock_inited)
    {
        task_lock_inited = 1;
        tasklist_lock_init();
    }

    ret = TTOS_ObtainMutex (task_lock, TTOS_WAIT_FOREVER);
    if (TTOS_OK != ret && TTOS_INVALID_USER != ret)
    {
        // KLOG_E("fail at %s:%d ret:%d\n" ,__FILE__ ,__LINE__, ret);
    }                                      
}

void tasklist_unlock(void)                                    
{       
    T_TTOS_ReturnCode ret;

    ret = TTOS_ReleaseMutex (task_lock);
    if (TTOS_OK != ret && TTOS_NOT_OWNER_OF_RESOURCE != ret)
    {
        //  KLOG_E("fail at %s:%d\n",__FILE__,__LINE__);
    }                               
}