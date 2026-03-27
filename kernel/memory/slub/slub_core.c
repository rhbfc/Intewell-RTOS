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

#include <atomic.h>
#include <kmalloc.h>
#include <page.h>
#include <slub.h>
#include <spinlock.h>
#include <stdlib.h>
#include <string.h>
#include <ttosMM.h>
#include <ttosUtils.h>

#define KLOG_TAG "slub"
#include <klog.h>

#undef KLOG_D
#define KLOG_D(...)

/* 全局SLUB分配器实例 */
struct slub_allocator g_slub = {
    .cache_list = LIST_HEAD_INIT(g_slub.cache_list),
    .initialized = true,
    .cache_lock = __SPINLOCK_INITIALIZER(g_slub.cache_lock),
};

/**
 * 初始化页中的空闲对象链表
 */
static void init_page_freelist(struct page *page, struct kmem_cache *cache)
{
    char *addr = (char *)page_address(page_to_addr(page));
    void *last = NULL;

    /* 从页末尾开始，向前构建链表 */
    for (int i = cache->objects_per_page - 1; i >= 0; i--)
    {
        void *obj = addr + i * cache->size;
        *(void **)obj = last;
        last = obj;
    }
    /* 使用 tagged pointer 初始化 freelist，初始 tag 为 0 */
    atomic_write(&page->freelist, slub_ptr_tag(last, 0));
    atomic_write(&page->inuse, 0);
    atomic_write(&page->slab_state, SLAB_PAGE_FREE);
    page->objects = cache->objects_per_page;
    page->slab_cache = cache;
    page->cpu_cache = NULL;
    page->flags = PG_slab;

    KLOG_D("[SLUB_CORE] 初始化页 %llu，对象数量: %u，对象大小: %u",
           (unsigned long long)page_to_pfn(page), page->objects, cache->size);
}
/**
 * 尝试将页状态从 old_state 转换为 new_state
 * 返回 true 表示成功，false 表示失败
 */
static inline bool page_try_set_state(struct page *page, int old_state, int new_state)
{
    return atomic_cas(&page->slab_state, old_state, new_state);
}

/**
 * 将页加入到partial链表
 * 使用 tagged pointer 解决 ABA 问题
 * 返回 true 表示成功入队，false 表示页状态不允许入队
 */
static bool page_push_to_partial(struct kmem_cache *cache, struct page *page)
{
    /* 尝试将页状态设置为 PARTIAL，防止重复入队 */
    int cur_state = atomic_read(&page->slab_state);

    /* 只有 FULL 状态的页才能入队到 partial */
    if (cur_state != SLAB_PAGE_FULL)
    {
        return false;
    }

    if (!page_try_set_state(page, SLAB_PAGE_FULL, SLAB_PAGE_PARTIAL))
    {
        return false;
    }

    /* 确保状态转换对其他 CPU 可见 */
    smp_mb();

    /* 使用 tagged pointer 的 CAS 操作 */
    do
    {
        uintptr_t old_head = atomic_read(&cache->partial_list);
        struct page *old_ptr = slub_ptr_untag(old_head);

        page->partial_next = (uintptr_t)old_ptr;

        /* 写屏障：确保 partial_next 写入在 CAS 之前完成 */
        smp_wmb();

        /* 新的 tagged pointer 使用递增的 tag */
        uintptr_t new_head = slub_ptr_inc_tag(page, old_head);

        if (atomic_cas(&cache->partial_list, old_head, new_head))
        {
            break;
        }
    } while (1);

    return true;
}

/**
 * 从partial链表中取出一个页
 * 使用 tagged pointer 解决 ABA 问题
 */
static struct page *page_pop_from_partial(struct kmem_cache *cache)
{
    do
    {
        uintptr_t old_head = atomic_read(&cache->partial_list);

        /* 读屏障：确保读取 partial_list 后再读取页内容 */
        smp_rmb();

        struct page *page = slub_ptr_untag(old_head);

        if (page == NULL)
        {
            return NULL;
        }

        /* 读取 next 指针前，先验证页状态 */
        if (atomic_read(&page->slab_state) != SLAB_PAGE_PARTIAL)
        {
            /* 页已被其他 CPU 处理，重试 */
            continue;
        }

        uintptr_t next_ptr = page->partial_next;

        /* 新的 tagged pointer 使用递增的 tag，指向下一个节点 */
        uintptr_t new_head = slub_ptr_inc_tag((void *)next_ptr, old_head);

        if (atomic_cas(&cache->partial_list, old_head, new_head))
        {
            /* 成功弹出，将页状态改为 FULL（表示不在任何链表中） */
            atomic_write(&page->slab_state, SLAB_PAGE_FULL);

            /* 全屏障：确保状态变更对其他 CPU 可见 */
            smp_mb();

            page->partial_next = 0;
            return page;
        }
    } while (1);
}

/**
 * 从页中分配一个对象
 * 使用 tagged pointer 解决 ABA 问题
 */
static void *alloc_from_page(struct page *page)
{
    do
    {
        uintptr_t old_head = atomic_read(&page->freelist);

        /* 读屏障：确保读取 freelist 后再读取对象内容 */
        smp_rmb();

        void *obj = slub_ptr_untag(old_head);

        if (obj == NULL)
        {
            return NULL;
        }

        /* 获取下一个对象的指针 */
        void *next_obj = *(void **)obj;

        /* 创建新的 tagged pointer，递增 tag */
        uintptr_t new_head = slub_ptr_inc_tag(next_obj, old_head);

        if (atomic_cas(&page->freelist, old_head, new_head))
        {
            atomic_inc(&page->inuse);
            return obj;
        }
    } while (1);
}

/**
 * 将对象释放回页
 * 使用 tagged pointer 解决 ABA 问题
 */
static void free_to_page(struct page *page, void *object)
{
    if (!page || !object)
        return;

    /* 将对象添加到freelist头部 */
    do
    {
        uintptr_t old_head = atomic_read(&page->freelist);
        void *old_ptr = slub_ptr_untag(old_head);

        /* 设置对象的 next 指针 */
        *(void **)object = old_ptr;

        /* 写屏障：确保 next 指针写入在 CAS 之前完成 */
        smp_wmb();

        /* 创建新的 tagged pointer，递增 tag */
        uintptr_t new_head = slub_ptr_inc_tag(object, old_head);

        if (atomic_cas(&page->freelist, old_head, new_head))
        {
            break;
        }
    } while (1);

    atomic_dec(&page->inuse);

    KLOG_D("[SLUB_CORE] 释放对象到页 %llu，使用中对象: %u/%u",
           (unsigned long long)page_to_pfn(page), atomic_read(&page->inuse), page->objects);
}

/**
 * 释放slab页
 */
static void free_slab_page(struct kmem_cache *cache, struct page *page)
{
    if (!page)
        return;

    KLOG_D("[SLUB_CORE] 释放缓存 %s 的页 %llu", cache->name, (unsigned long long)page_to_pfn(page));

    /* 设置页状态为 FREE */
    atomic_write(&page->slab_state, SLAB_PAGE_FREE);

    /* 清理页标志 */
    page->flags &= ~(PG_slab);
    page->slab_cache = NULL;
    page->cpu_cache = NULL;
    atomic_set(&page->freelist, 0);
    atomic_set(&page->inuse, 0);
    page->objects = 0;

    /* 更新缓存统计 */
    atomic_dec(&cache->page_count);

    /* 释放页 */
    free_page(page);
}

/**
 * 将页设置为当前 CPU 的激活页
 * 注意：调用此函数前必须已经禁用抢占
 */
static void cpu_slab_set_active_page(struct kmem_cache *cache, struct page *page)
{
    struct page *acpage;
    struct kmem_cache_cpu *cpu_cache = &cache->__percpu[cpuid_get()];

    acpage = cpu_cache->page;
    if (acpage == page)
    {
        return;
    }

    /* 设置新页为激活页 */
    cpu_cache->page = page;
    page->cpu_cache = cpu_cache;
    atomic_write(&page->slab_state, SLAB_PAGE_CPU_ACTIVE);

    /* 当存在旧的激活页时需要处理旧页的归属 */
    if (acpage)
    {
        /* 归还空闲对象到旧页 */
        while (cpu_cache->freelist)
        {
            void *object = cpu_cache->freelist;
            cpu_cache->freelist = *(void **)object;
            free_to_page(acpage, object);
        }

        acpage->cpu_cache = NULL;

        /* 判断旧页的归属 */
        int inuse = atomic_read(&acpage->inuse);
        if (inuse == 0)
        {
            /* 页完全空闲，归还给页分配器 */
            free_slab_page(cache, acpage);
        }
        else if (inuse < (int)acpage->objects)
        {
            /* 页部分使用，先设置为 FULL 状态，然后尝试入队 */
            atomic_write(&acpage->slab_state, SLAB_PAGE_FULL);
            page_push_to_partial(cache, acpage);
        }
        else
        {
            /* 页已满，设置为 FULL 状态 */
            atomic_write(&acpage->slab_state, SLAB_PAGE_FULL);
        }
    }

    /* 从新页窃取 freelist，使用 tagged pointer */
    do
    {
        uintptr_t old_head = atomic_read(&page->freelist);
        void *free_ptr = slub_ptr_untag(old_head);

        /* 新的 tagged pointer 指向 NULL，递增 tag */
        uintptr_t new_head = slub_ptr_inc_tag(NULL, old_head);

        if (atomic_cas(&page->freelist, old_head, new_head))
        {
            cpu_cache->freelist = free_ptr;
            atomic_set(&page->inuse, page->objects);
            break;
        }
    } while (1);
}

/**
 * 为缓存分配新页
 */
static struct page *allocate_slab_page(struct kmem_cache *cache)
{
    struct page *page = alloc_page();
    if (!page)
    {
        KLOG_D("[SLUB_CORE] 错误：无法分配新页给缓存 %s", cache->name);
        return NULL;
    }

    /* 初始化页作为slab */
    init_page_freelist(page, cache);

    /* 更新缓存统计 */
    atomic_inc(&cache->page_count);

    KLOG_D("[SLUB_CORE] 为缓存 %s 分配新页 %llu", cache->name,
           (unsigned long long)page_to_pfn(page));
    return page;
}

/**
 * 慢路径：从 partial 链表或新页中分配对象
 * 注意：调用此函数前必须已经禁用抢占
 */
static void *kmem_cache_alloc_slowpath(struct kmem_cache *cache)
{
    struct page *page;
    void *object;

    /* 尝试从 partial 链表获取页 */
    while ((page = page_pop_from_partial(cache)) != NULL)
    {
        object = alloc_from_page(page);
        if (object == NULL)
        {
            /* 页已空，跳过继续尝试下一个 */
            continue;
        }

        int inuse = atomic_read(&page->inuse);
        if (inuse == (int)page->objects)
        {
            /* 页已满，标记为 FULL 状态 */
            atomic_write(&page->slab_state, SLAB_PAGE_FULL);
        }
        else if (inuse < (int)page->objects / 2)
        {
            /* 页使用率低，设为当前 CPU 的激活页 */
            cpu_slab_set_active_page(cache, page);
        }
        else
        {
            /* 页使用率中等，放回 partial 链表 */
            atomic_write(&page->slab_state, SLAB_PAGE_FULL);
            page_push_to_partial(cache, page);
        }

        atomic_add(&cache->slow_alloc_count, 1);
        return object;
    }

    /* partial 链表为空，分配新页 */
    page = allocate_slab_page(cache);
    if (page)
    {
        object = alloc_from_page(page);
        cpu_slab_set_active_page(cache, page);
        atomic_add(&cache->slow_alloc_count, 1);
        return object;
    }

    return NULL;
}

/**
 * 从缓存分配对象 - 主要接口
 */
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
    void *object;
    (void)flags;

    atomic_add(&cache->alloc_count, 1);

    /* 禁用抢占，避免切核 */
    TTOS_DisablePreempt();

    struct kmem_cache_cpu *cpu_cache = &cache->__percpu[cpuid_get()];

    /* 快速路径1：从 CPU 本地 freelist 分配 */
    if (cpu_cache->freelist)
    {
        object = cpu_cache->freelist;
        cpu_cache->freelist = *(void **)object;
        TTOS_EnablePreempt();
        return object;
    }

    /* 快速路径2：从当前 CPU 的激活页分配 */
    if (cpu_cache->page)
    {
        object = alloc_from_page(cpu_cache->page);
        if (object)
        {
            TTOS_EnablePreempt();
            return object;
        }
    }

    /* 慢路径 */
    object = kmem_cache_alloc_slowpath(cache);

    TTOS_EnablePreempt();
    return object;
}
/**
 * 释放对象到缓存 - 主要接口
 */
void kmem_cache_free(struct kmem_cache *cache, void *object)
{
    if (!cache || !object)
        return;

    struct page *page = ptr_to_page(object);

    /* 验证对象归属 */
    if (page->slab_cache != cache)
    {
        KLOG_E("[SLUB_CORE] 错误：对象不属于指定的缓存 %s", cache->name);
        return;
    }

    atomic_add(&cache->free_count, 1);

    /* 禁用抢占，避免切核 */
    TTOS_DisablePreempt();

    struct kmem_cache_cpu *cpu_cache = &cache->__percpu[cpuid_get()];

    /* 快速路径：对象属于当前 CPU 的激活页 */
    if (page == cpu_cache->page)
    {
        *(void **)object = cpu_cache->freelist;
        cpu_cache->freelist = object;
        TTOS_EnablePreempt();
        return;
    }

    TTOS_EnablePreempt();

    /* 慢路径：对象不属于当前 CPU */
    free_to_page(page, object);

    /*
     * 根据页状态决定是否需要入队到 partial 链表
     * 只有 FULL 状态的页才能入队，page_push_to_partial 内部会处理状态转换
     */
    int state = atomic_read(&page->slab_state);

    if (state == SLAB_PAGE_CPU_ACTIVE)
    {
        /* 页是某个 CPU 的激活页，由该 CPU 负责管理，不入队 */
        return;
    }

    if (state == SLAB_PAGE_PARTIAL)
    {
        /* 页已经在 partial 链表中，不重复入队 */
        return;
    }

    /* 尝试将 FULL 状态的页入队到 partial 链表 */
    page_push_to_partial(cache, page);
}

int kmem_cache_init(struct kmem_cache *cache, const char *name, size_t size, size_t align,
                    unsigned long flags, void (*ctor)(void *))
{
    (void)align; /* 暂时未使用对齐参数 */

    /* 设置基本属性 */
    strncpy(cache->name, name, sizeof(cache->name) - 1);
    cache->object_size = size;
    cache->size = (size + 7) & ~7; /* 8字节对齐 */
    cache->flags = flags;
    cache->objects_per_page = PAGE_SIZE / cache->size;
    cache->ctor = ctor;

    /* 初始化统计计数 */
    atomic_write(&cache->alloc_count, 0);
    atomic_write(&cache->slow_alloc_count, 0);
    atomic_write(&cache->free_count, 0);
    atomic_write(&cache->page_count, 0);

    /* 初始化 partial 链表为空（tagged pointer 格式，初始 tag 为 0）*/
    atomic_write(&cache->partial_list, slub_ptr_tag(NULL, 0));

    /* 初始化 per-CPU 缓存 */
    for (int i = 0; i < CONFIG_MAX_CPUS; i++)
    {
        memset(&cache->__percpu[i], 0, sizeof(struct kmem_cache_cpu));
    }

    INIT_LIST_HEAD(&cache->list);

    /* 添加到全局缓存链表 */
    spin_lock(&g_slub.cache_lock);
    list_add(&cache->list, &g_slub.cache_list);
    spin_unlock(&g_slub.cache_lock);

    KLOG_D("[SLUB_CORE] 创建缓存 '%s'，对象大小: %zu，每页对象数: %u", name, size,
           cache->objects_per_page);

    return 0;
}

/**
 * 创建新的kmem_cache
 */
struct kmem_cache *kmem_cache_create(const char *name, size_t size, size_t align,
                                     unsigned long flags, void (*ctor)(void *))
{
    if (!name || size == 0 || size > MAX_OBJECT_SIZE)
    {
        KLOG_E("[SLUB_CORE] 错误：无效的缓存参数\n");
        return NULL;
    }

    /* 分配缓存结构 */
    struct kmem_cache *cache = kzalloc(sizeof(struct kmem_cache), GFP_KERNEL);
    if (!cache)
    {
        KLOG_E("[SLUB_CORE] 错误：无法分配缓存结构\n");
        return NULL;
    }

    kmem_cache_init(cache, name, size, align, flags, ctor);

    return cache;
}

/**
 * 销毁kmem_cache
 */
void kmem_cache_destroy(struct kmem_cache *cache)
{
    if (!cache)
        return;
    // todo
    return;
}

/**
 * 获取缓存的对象大小
 */
size_t kmem_cache_size(struct kmem_cache *cache)
{
    return cache ? cache->object_size : 0;
}

/**
 * 获取CPU缓存统计信息
 */
void get_percpu_stats(struct kmem_cache *cache, int cpu_id, int *freelist_count, bool *has_page)
{
    // todo
}

/**
 * 获取SLUB分配器状态信息
 */
void slub_get_info(int *total_caches, int *total_pages, int *free_pages)
{
    if (!g_slub.initialized)
    {
        if (total_caches)
            *total_caches = 0;
        if (total_pages)
            *total_pages = 0;
        if (free_pages)
            *free_pages = 0;
        return;
    }

    /* 统计缓存数量 */
    int cache_count = 0;
    spin_lock(&g_slub.cache_lock);
    struct kmem_cache *cache;
    list_for_each_entry(cache, &g_slub.cache_list, list)
    {
        cache_count += atomic_read(&cache->page_count);
    }
    spin_unlock(&g_slub.cache_lock);

    uintptr_t total_nr, free_nr;

    page_get_info(&total_nr, &free_nr);

    if (total_caches)
        *total_caches = cache_count;
    if (total_pages)
    {
        *total_pages = total_nr;
    }
    if (free_pages)
    {
        *free_pages = free_nr;
    }
}