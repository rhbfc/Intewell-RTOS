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
#include <ttos_init.h>

// #define DEBUG_EVENT
#ifdef DEBUG_EVENT
static T_TTOS_EventControl *event_cb1;
static T_TTOS_EventControl *event_cb2;
static T_TTOS_EventControl *event_cb3;
static T_TTOS_EventControl *event_cb4;
static T_TTOS_EventControl *event_cb5;
static T_TTOS_EventControl *event_cb6;
static T_TTOS_EventControl *event_cb7;

void task1 (void *param)
{
    T_TTOS_EventSet eventOut = 0;

    printk("%s wait event\n", __func__);
    TTOS_ReceiveEvent(event_cb1, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ALL, TTOS_EVENT_WAIT_FOREVER, &eventOut);
    printk("%s received event:0x%x\n",__func__, eventOut);

    if(eventOut == (TTOS_EVENT_1 | TTOS_EVENT_2))
    {
        printk("test1 success\n");
    }
}

void task2 (void *param)
{
    T_TTOS_EventSet eventOut = 0;

    printk("%s wait event\n", __func__);
    TTOS_ReceiveEvent(event_cb2, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ANY, TTOS_EVENT_WAIT_FOREVER, &eventOut);
    printk("%s received event:0x%x\n",__func__, eventOut);

    if(eventOut & (TTOS_EVENT_1 | TTOS_EVENT_2))
    {
        printk("test2 success\n");
    }
}

void task3 (void *param)
{
    T_TTOS_EventSet eventOut = 0;

    printk("%s wait event\n", __func__);
    TTOS_ReceiveEvent(event_cb3, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ALL, TTOS_EVENT_WAIT_FOREVER, &eventOut);
    printk("%s received event:0x%x\n",__func__, eventOut);

    if(eventOut == (TTOS_EVENT_1 | TTOS_EVENT_2))
    {
        printk("test3 success\n");
    }
}

void task4 (void *param)
{
    T_TTOS_EventSet eventOut = 0;

    printk("%s wait event\n", __func__);
    TTOS_ReceiveEvent(event_cb4, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ANY, TTOS_EVENT_WAIT_FOREVER, &eventOut);
    printk("%s received event:0x%x\n",__func__, eventOut);

    if(eventOut & (TTOS_EVENT_1 | TTOS_EVENT_2))
    {
        printk("test4 success\n");
    }
}

void task5 (void *param)
{
    T_TTOS_EventSet eventOut = 0;
    T_TTOS_ReturnCode ret = TTOS_OK;

    printk("%s wait event\n", __func__);
    
    ret = TTOS_ReceiveEvent(event_cb5, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ANY, 0, &eventOut);

    if(ret == TTOS_UNSATISFIED)
    {
        printk("test5 success\n");
    }
}

void task6 (void *param)
{
    T_TTOS_EventSet eventOut = 0;
    T_TTOS_ReturnCode ret = TTOS_OK;

    printk("%s wait event\n", __func__);
    
    ret = TTOS_ReceiveEvent(event_cb6, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ANY, 10, &eventOut);
    if(ret == TTOS_TIMEOUT)
    {
        printk("test6 success\n");
    }
}

void task7 (void *param)
{
    T_TTOS_EventSet eventOut = 0;
    T_TTOS_ReturnCode ret = TTOS_OK;

    printk("%s wait event\n", __func__);
    
    ret = TTOS_ReceiveEvent(event_cb7, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ANY, TTOS_EVENT_WAIT_FOREVER, &eventOut);
    if(ret == TTOS_UNSATISFIED)
    {
        printk("test7 success\n");
    }
}

void task8 (void *param)
{
    T_TTOS_EventSet eventOut = 0;
    T_TTOS_ReturnCode ret = TTOS_OK;

    printk("%s wait event\n", __func__);
    
    ret = TTOS_ReceiveEvent(event_cb7, TTOS_EVENT_1 | TTOS_EVENT_2, TTOS_EVENT_ANY, TTOS_EVENT_WAIT_FOREVER, &eventOut);
    if(ret == TTOS_UNSATISFIED)
    {
        printk("test8 success\n");
    }
}

int event_test(void)
{
    printk("%s %d\n", __func__, __LINE__);
    event_cb1 = TTOS_CreateEvent();
    event_cb2 = TTOS_CreateEvent();
    event_cb3 = TTOS_CreateEvent();
    event_cb4 = TTOS_CreateEvent();
    event_cb5 = TTOS_CreateEvent();
    event_cb6 = TTOS_CreateEvent();
    event_cb7 = TTOS_CreateEvent();

    printk("send event to task3 task4\n");
    TTOS_SendEvent(event_cb3, TTOS_EVENT_1 | TTOS_EVENT_2);
    TTOS_SendEvent(event_cb4, TTOS_EVENT_1 | TTOS_EVENT_2);

    TASK_ID           tid1;
    TASK_ID           tid2;
    TASK_ID           tid3;
    TASK_ID           tid4;
    TASK_ID           tid5;
    TASK_ID           tid6;
    TASK_ID           tid7;
    TASK_ID           tid8;

    TTOS_CreateTaskEx ("test_event1", 225, TRUE, TRUE, task1, NULL, 81920, &tid1);
    TTOS_CreateTaskEx ("test_event2", 225, TRUE, TRUE, task2, NULL, 81920, &tid2);
    TTOS_CreateTaskEx ("test_event3", 225, TRUE, TRUE, task3, NULL, 81920, &tid3);
    TTOS_CreateTaskEx ("test_event4", 225, TRUE, TRUE, task4, NULL, 81920, &tid4);
    TTOS_CreateTaskEx ("test_event5", 225, TRUE, TRUE, task5, NULL, 81920, &tid5);
    TTOS_CreateTaskEx ("test_event6", 225, TRUE, TRUE, task6, NULL, 81920, &tid6);
    TTOS_CreateTaskEx ("test_event7", 225, TRUE, TRUE, task7, NULL, 81920, &tid7);
    TTOS_CreateTaskEx ("test_event8", 225, TRUE, TRUE, task8, NULL, 81920, &tid8);

    sleep(1);
    TTOS_DeleteEvent(event_cb7);

    sleep(5);
    printk("send event to task1 task2\n");
    TTOS_SendEvent(event_cb1, TTOS_EVENT_1 | TTOS_EVENT_2);
    TTOS_SendEvent(event_cb2, TTOS_EVENT_1 | TTOS_EVENT_2);

    while(1)
    {
        sleep(1);
    }
    return 0;
}
INIT_EXPORT_USER(event_test, "event_test");
#endif
