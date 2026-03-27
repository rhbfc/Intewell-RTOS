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

#include <process_obj.h>
#include <ttosProcess.h>

#include <spinlock.h>

static DEFINE_SPINLOCK (pobj_lock);

struct process_obj *process_obj_create (pcb_t pcb, void *obj,
                                        void (*destroy) (void *obj))
{
    struct process_obj *process_obj = calloc (1, sizeof *process_obj);

    uintptr_t flags;
    if (process_obj == NULL)
    {
        return NULL;
    }
    struct __process_obj_really *__process_obj_really
        = calloc (1, sizeof *__process_obj_really);
    if (__process_obj_really == NULL)
    {
        free (process_obj);
        return NULL;
    }
    __process_obj_really->ref     = 1;
    __process_obj_really->obj     = obj;
    __process_obj_really->destroy = destroy;
    process_obj->really           = __process_obj_really;
    process_obj->belong_pcb       = pcb;

    spin_lock_irqsave (&pobj_lock, flags);
    list_add (&process_obj->list, &pcb->obj_list);
    spin_unlock_irqrestore (&pobj_lock, flags);
    return process_obj;
}
struct process_obj *process_obj_ref (pcb_t pcb, struct process_obj *obj)
{
    struct process_obj *ref_obj = calloc (1, sizeof *ref_obj);

    if (ref_obj == NULL)
    {
        return NULL;
    }

    uintptr_t flags;
    obj->really->ref++;
    ref_obj->really     = obj->really;
    ref_obj->belong_pcb = pcb;
    spin_lock_irqsave (&pobj_lock, flags);
    list_add (&ref_obj->list, &pcb->obj_list);
    spin_unlock_irqrestore (&pobj_lock, flags);
    return ref_obj;
}

int process_obj_foreach (pcb_t pcb, void (*func) (struct process_obj *, void *),
                         void *param)
{
    struct process_obj *obj;
    struct process_obj *tmp;
    uintptr_t           flags;
    spin_lock_irqsave (&pobj_lock, flags);
    list_for_each_entry_safe (obj, tmp, &pcb->obj_list, list)
    {
        func (obj, param);
    }
    spin_unlock_irqrestore (&pobj_lock, flags);
    return 0;
}

int process_obj_destroy (struct process_obj *obj)
{
    uintptr_t flags;
    obj->really->ref--;
    if (obj->really->ref == 0)
    {
        obj->really->destroy (obj->really->obj);

        free (obj->really);
    }
    spin_lock_irqsave (&pobj_lock, flags);
    list_del (&obj->list);
    spin_unlock_irqrestore (&pobj_lock, flags);
    free (obj);
    return 0;
}
