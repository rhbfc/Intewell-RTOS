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

/**
 * @file： ttos_pic.h
 * @brief：
 *    <li>pic相关类型定义、外部声明/li>
 */
#ifndef _TTOS_PIC_H
#define _TTOS_PIC_H

/************************头文件********************************/
#include <list.h>
#include <stdbool.h>
#include <stdint.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/************************宏定义********************************/
#define CONFIG_IRQ_NAME_MAX 64

#ifdef CONFIG_X86_64
#define CONFIG_IRQ_NUM 193
#define CONFIG_IRQ_NUMBER_START 0
#else
#define CONFIG_IRQ_NUM 1024
#define CONFIG_IRQ_NUMBER_START 1024
#endif

/************************类型定义******************************/

struct device;
struct ttos_pic_ops;
struct ttos_irq_desc;
typedef struct ttos_irq_desc *ttos_irq_desc_t;
typedef void (*isr_handler_t)(uint32_t irq, void *param);

typedef enum ttos_pic_irq_type
{
    PIC_IRQ_TRIGGER_LEVEL = 1,
    PIC_IRQ_TRIGGER_EDGE  = 2,
    PIC_IRQ_POLARITY_LOW  = 4,
    PIC_IRQ_POLARITY_HI   = 8,
} ttos_pic_irq_type_t;

struct ttos_pic
{
    char *name;
    struct ttos_pic_ops *pic_ops;
    struct ttos_pic *next_node;

#define PIC_FLAG_CPU (1 << 0)
#define PIC_FLAG_PERIPHERAL (1 << 1)
    uint32_t flag;
    uint32_t virt_irq_rang[2];
    uint32_t hwirq_base;
    uint64_t count[CONFIG_MAX_CPUS];
    void *priv;
};

struct ttos_pic_ops
{
    int32_t (*pic_init)(struct ttos_pic *pic);
    int32_t (*pic_unmask)(struct ttos_pic *pic, uint32_t irq);
    int32_t (*pic_mask)(struct ttos_pic *pic, uint32_t irq);
    int32_t (*pic_ack)(struct ttos_pic *pic, uint32_t *src_cpu, uint32_t *irq);
    int32_t (*pic_eoi)(struct ttos_pic *pic, uint32_t irq, uint32_t src_cpu);
    int32_t (*pic_pending_set)(struct ttos_pic *pic, uint32_t irq);
    int32_t (*pic_pending_get)(struct ttos_pic *pic, uint32_t irq, bool *pending);
    int32_t (*pic_priority_set)(struct ttos_pic *pic, uint32_t irq, uint32_t priority);
    int32_t (*pic_priority_get)(struct ttos_pic *pic, uint32_t irq, uint32_t *priority);
    int32_t (*pic_affinity_set)(struct ttos_pic *pic, uint32_t irq, uint32_t cpu);
    int32_t (*pic_trigger_type_set)(struct ttos_pic *pic, uint32_t irq, ttos_pic_irq_type_t type);
    int32_t (*pic_ipi_send)(struct ttos_pic *pic, uint32_t irq, uint32_t cpu, uint32_t mode);
    int32_t (*pic_max_number_get)(struct ttos_pic *pic, uint32_t *max_number);
    int32_t (*pic_msi_alloc)(struct ttos_pic *pic, uint32_t device_id, uint32_t count);
    int32_t (*pic_msi_map)(struct ttos_pic *pic, uint32_t device_id, uint32_t event_id);
    int32_t (*pic_msi_free)(struct ttos_pic *pic, struct ttos_irq_desc *irq);
    int32_t (*pic_msi_send)(struct ttos_pic *pic, uint32_t irq);
    uint64_t (*pic_msi_address)(struct ttos_pic *pic);
    uint64_t (*pic_msi_data)(struct ttos_pic *pic, uint32_t index, ttos_irq_desc_t desc);
    int32_t (*pic_dtb_irq_parse)(struct ttos_pic *pic, uint32_t *value_table);
    void *(*pic_irq_desc_get)(struct ttos_pic *pic, int32_t hwirq);
};

typedef struct ttos_irq_node
{
    struct list_node node;
    char name[CONFIG_IRQ_NAME_MAX];
    isr_handler_t handler;
    void *arg;
} * ttos_irq_node_t;

typedef struct ttos_irq_desc
{
    char name[CONFIG_IRQ_NAME_MAX];
    void *arg;
    isr_handler_t handler;
    bool used;
    uint32_t virt_irq;
    uint32_t hw_irq;

#define IRQ_SHARED (1 << 0)
    uint32_t flags;
    uint32_t count;
    uint32_t priority;
    uint64_t cpumap;
    ttos_pic_irq_type_t trigger_mode;
    struct list_head irq_shared_list;
    struct ttos_pic *pic;
    void *pirv;
} * ttos_irq_desc_t;

/************************外部声明******************************/
struct ttos_pic *ttos_pic_get_cpu_pic(uint32_t hw_irq);
int32_t ttos_pic_register(struct ttos_pic *pic);
int32_t ttos_pic_init(struct ttos_pic *pic);
int32_t ttos_pic_irq_affinity_set(uint32_t irq, uint32_t cpu);
int32_t ttos_pic_pending_set(uint32_t irq);
int32_t ttos_pic_pending_get(uint32_t irq, bool *pending);
int32_t ttos_pic_irq_priority_set(uint32_t irq, uint32_t priority);
int32_t ttos_pic_irq_priority_get(uint32_t irq, uint32_t *priority);
int32_t ttos_pic_irq_trigger_mode_set(uint32_t irq, ttos_pic_irq_type_t type);
int32_t ttos_pic_irq_mask(uint32_t irq);
int32_t ttos_pic_irq_unmask(uint32_t irq);
int32_t ttos_pic_irq_ack(uint32_t *src_cpu, uint32_t *irq);
int32_t ttos_pic_irq_eoi(uint32_t irq, uint32_t src_cpu);
int32_t ttos_pic_ipi_send(uint32_t hw_irq, uint32_t cpu, uint32_t mode);
int32_t ttos_pic_irq_map(uint32_t virt_irq, struct ttos_pic **pic);
int32_t ttos_pic_irq_parse(struct ttos_pic *pic, uint32_t *value_table);
int32_t ttos_pic_msi_alloc(struct ttos_pic *pic, uint32_t device_id, uint32_t count);
uint32_t ttos_pic_msi_map(struct ttos_pic *pic, uint32_t device_id, uint32_t event_id);
uint32_t ttos_pic_msi_send(struct ttos_pic *pic, uint32_t irq);
uint64_t ttos_pic_msi_address(struct ttos_pic *pic);
uint64_t ttos_pic_msi_data(struct ttos_pic *pic, uint32_t index, ttos_irq_desc_t desc);

ttos_irq_desc_t ttos_pic_cpu_irq_desc_alloc(uint32_t hw_irq);
ttos_irq_desc_t ttos_pic_dev_irq_desc_alloc(struct device *dev);
ttos_irq_desc_t ttos_pic_irq_desc_get(uint32_t virt_irq);
int32_t ttos_pic_irq_desc_free(uint32_t irq);

int32_t ttos_pic_irq_install(uint32_t irq, isr_handler_t handler, void *arg, uint32_t flags,
                             const char *name);
int32_t ttos_pic_irq_uninstall(uint32_t irq, const char *name);
void ttos_pic_irq_handle(uint32_t irq);
uint32_t ttos_pic_irq_alloc(struct device *dev, uint32_t cpu_hw_irq);
uint32_t ttos_pic_irq_alloc_by_pic (struct ttos_pic *pic, uint32_t cpu_hw_irq);
ttos_irq_desc_t ttos_pic_msi_desc_init(uint32_t irq);
void irq_show(void);

void irqdesc_foreach(void (*func) (ttos_irq_desc_t, void *), void *par);
uint64_t ttos_pic_irq_count_get (uint32_t cpuid);

int ttos_pic_ipitest_hwirq_get(void);
uint64_t ttos_pic_irq_count_get(uint32_t cpuid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _TTOS_PIC_H */
