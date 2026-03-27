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

#ifndef __PROCESS_OBJ_H__
#define __PROCESS_OBJ_H__

#include <list.h>
struct T_TTOS_ProcessControlBlock;
typedef struct T_TTOS_ProcessControlBlock *pcb_t;
struct __process_obj_really {
    int   ref;
    void *obj;
    void (*destroy) (void *obj);
};
struct process_obj {
    struct list_head             list;
    pcb_t                        belong_pcb;
    struct __process_obj_really *really;
};

struct process_obj *process_obj_create (pcb_t pcb, void *obj,
                                        void (*destroy) (void *obj));
struct process_obj *process_obj_ref (pcb_t pcb, struct process_obj *obj);
int                 process_obj_destroy (struct process_obj *obj);
int process_obj_foreach (pcb_t pcb, void (*func) (struct process_obj *, void *),
                         void *param);

#define PROCESS_OBJ_GET(o, t) ((t)(o)->really->obj)

#endif /* __PROCESS_OBJ_H__ */
