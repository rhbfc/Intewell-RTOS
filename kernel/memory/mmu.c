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

#include <list.h>
#include <memblock.h>
#include <mmu.h>
#include <page.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <ttosMM.h>
#include <uaccess.h>

#include <system/kconfig.h>

#define KLOG_TAG "MM"
#include <klog.h>
extern int printk(const char *fmt, ...);
extern struct mmu_ops *arch_get_mmu_ops(void);

static struct mm g_kernel_mm = {.mmu_table_base = 0,
                                .asid = 0,
                                .mm_region_list = LIST_HEAD_INIT(g_kernel_mm.mm_region_list),
                                .mm_region_type = MM_REGION_TYPE_KERNEL,
                                .page_size = 0,
                                .ops = NULL};

struct mm *get_kernel_mm(void)
{
    return &g_kernel_mm;
}

phys_addr_t zone_normal_pvoffset(void)
{
    return g_kernel_mm.pv_offset;
}

static struct mm_region kernel_linear_map_region = {
    .list = LIST_HEAD_INIT(kernel_linear_map_region.list),
    .physical_address = 0,
    .mem_attr = MT_RWX_DATA | MT_KERNEL,
    .is_need_free = false,
    .is_page_free = false,
};

static inline int mm_kernel_region_map(struct mm_region *region);
static struct mm_region *va2region_r(struct list_head *head, virt_addr_t vaddr);
static void mm_region_remove_rbtree(struct mm *mm, struct mm_region *region);
static struct mm_region *va2region_rbtree(struct rb_root *tree, virt_addr_t vaddr);
static int mm_region_insert_rbtree(struct mm *mm, struct mm_region *region);

size_t kernel_mmu_get_page_size(void)
{
    return g_kernel_mm.page_size;
}

void kernel_mmu_set_pvoffset(uint32_t pa_l, uint32_t pa_h, virt_addr_t va)
{
    phys_addr_t pa = (phys_addr_t)pa_l | (phys_addr_t)pa_h << 32;
    g_kernel_mm.pv_offset = pa - va;
}

int mm_init(struct mm *mm)
{
    rw_spin_lock_init(&mm->mm_region_lock);
    spin_lock_init(&mm->ptable_lock);
    mm->mm_region_tree = RB_TREE_ROOT; /* 初始化红黑树 */
    mm->ops = arch_get_mmu_ops();
    return mm->ops->init(mm);
}

int mm_destroy(struct mm *mm)
{
    while (!list_empty(&mm->mm_region_list))
    {
        mm_region_unmap(mm, list_entry(list_first(&mm->mm_region_list), struct mm_region, list));
    }
    mm->ops->deinit(mm);
    return 0;
}

int mm_foreach_region(struct mm *mm,
                      void (*func)(struct mm *mm, struct mm_region *region, void *ctx), void *ctx)
{
    struct mm_region *region_pos;
    write_lock(&mm->mm_region_lock);
    list_for_each_entry(region_pos, &mm->mm_region_list, list)
    {
        func(mm, region_pos, ctx);
    }
    write_unlock(&mm->mm_region_lock);
    return 0;
}

int kernel_mmu_init(void)
{
    int ret = 0;
    mm_init(&g_kernel_mm);

    kernel_linear_map_region.physical_address = min_low_pfn << g_kernel_mm.page_size_shift;
    kernel_linear_map_region.virtual_address =
        kernel_linear_map_region.physical_address - g_kernel_mm.pv_offset;
    kernel_linear_map_region.region_page_count = (max_low_pfn - min_low_pfn + 1);

    ret = mm_kernel_region_map(&kernel_linear_map_region);

    if (ret != 0)
    {
        return ret;
    }

    g_kernel_mm.ops->apply_change(&g_kernel_mm);
    g_kernel_mm.ops->switch_space(NULL, &g_kernel_mm);

    return 0;
}

int mm_switch_space_to(struct mm *mm)
{
    return mm->ops->switch_space(NULL, mm);
}

void mm_region_init(struct mm_region *region, uintptr_t attr, phys_addr_t pa, size_t size)
{
    region->mem_attr = attr;
    region->physical_address = pa;
    region->region_page_count = PAGE_ALIGN(size) >> g_kernel_mm.page_size_shift;
    region->virtual_address = 0;
    region->flags = 0;
    region->map_min = 0;
    region->filepath = NULL;
    region->offset = 0;
    region->is_need_free = false;
    region->is_page_free = false;
    region->do_fork = NULL;
    region->munmap = NULL;
}

/**
 * @brief 获取红黑树中的下一个节点（中序后继）
 * @param node 当前节点
 * @return 下一个节点，如果没有则返回 NULL
 */
static struct rb_node *rb_next_node(struct rb_node *node)
{
    struct rb_node *parent;

    if (!node)
        return NULL;

    /* 如果有右子树，返回右子树的最左节点 */
    if (node->rb_right)
    {
        node = node->rb_right;
        while (node->rb_left)
            node = node->rb_left;
        return node;
    }

    /* 否则向上找第一个"当前节点在其左子树中"的祖先 */
    while ((parent = rb_parent(node)) && node == parent->rb_right)
        node = parent;

    return parent;
}

/**
 * @brief 获取红黑树中的前一个节点（中序前驱）
 * @param node 当前节点
 * @return 前一个节点，如果没有则返回 NULL
 */
static struct rb_node *rb_prev_node(struct rb_node *node)
{
    struct rb_node *parent;

    if (!node)
        return NULL;

    /* 如果有左子树，返回左子树的最右节点 */
    if (node->rb_left)
    {
        node = node->rb_left;
        while (node->rb_right)
            node = node->rb_right;
        return node;
    }

    /* 否则向上找第一个"当前节点在其右子树中"的祖先 */
    while ((parent = rb_parent(node)) && node == parent->rb_left)
        node = parent;

    return parent;
}

/**
 * @brief 更新节点的 rb_subtree_gap（增强红黑树的核心）
 * @param region 要更新的节点
 * @param search_end 搜索范围的结束地址（暂时未使用，保留用于未来扩展）
 *
 * rb_subtree_gap = max(左子树gap, 右子树gap, 当前节点到下一个节点的gap)
 */
static void update_subtree_gap(struct mm_region *region, virt_addr_t search_end)
{
    virt_addr_t max_gap = 0;
    struct mm_region *left_child = NULL;
    struct mm_region *right_child = NULL;
    struct mm_region *next = NULL;
    virt_addr_t region_end, gap_to_next;

    /* 获取左右子节点 */
    if (region->rb_node.rb_left)
        left_child = rb_entry(region->rb_node.rb_left, struct mm_region, rb_node);
    if (region->rb_node.rb_right)
        right_child = rb_entry(region->rb_node.rb_right, struct mm_region, rb_node);

    /* 获取下一个节点（红黑树的中序后继） */
    struct rb_node *next_node = rb_next_node(&region->rb_node);
    if (next_node)
        next = rb_entry(next_node, struct mm_region, rb_node);

    /* 计算当前节点到下一个节点的间隙 */
    region_end = region->virtual_address + region->region_page_count * PAGE_SIZE;
    if (next && next->virtual_address > region_end)
    {
        gap_to_next = next->virtual_address - region_end;
    }
    else
    {
        gap_to_next = 0;
    }

    /* max_gap = max(左子树gap, 右子树gap, gap_to_next) */
    max_gap = gap_to_next;
    if (left_child && left_child->rb_subtree_gap > max_gap)
        max_gap = left_child->rb_subtree_gap;
    if (right_child && right_child->rb_subtree_gap > max_gap)
        max_gap = right_child->rb_subtree_gap;

    region->rb_subtree_gap = max_gap;
}

/**
 * @brief 基于增强红黑树查找空闲虚拟地址区域（vmalloc 核心函数）
 * @param mm 内存管理结构
 * @param page_count 需要的页数
 * @param search_start 搜索起始地址
 * @param search_end 搜索结束地址
 * @return 找到的虚拟地址，失败返回 0 或 -1
 *
 * 设计原则（Linux 式 Augmented Red-Black Tree）：
 * - 每个节点存储 rb_subtree_gap - 该子树中最大的间隙
 * - 利用 rb_subtree_gap 找到第一个可能包含足够大间隙的子树
 * - 然后使用红黑树的中序遍历（rb_next）线性扫描
 * - 时间复杂度：O(log n) 定位 + O(k) 扫描，k 是需要检查的节点数
 *
 * 算法：
 * 1. 使用 rb_subtree_gap 找到第一个可能的节点
 * 2. 从该节点开始，使用 rb_next 进行中序遍历
 * 3. 检查每个间隙，直到找到足够大的或遍历完成
 */
static virt_addr_t mm_vmalloc(struct mm *mm, size_t page_count, virt_addr_t search_start,
                              virt_addr_t search_end)
{
    struct rb_node *node;
    struct mm_region *area, *prev;
    uintptr_t needed_size;
    uintptr_t gap_start, gap_end;
    size_t size = page_count << PAGE_SIZE_SHIFT;

    if (size == 0)
        return 0;

    if (search_start == 0)
    {
        search_start = MMAP_START_VADDR;
    }

    search_start = ALIGN_UP(search_start, PAGE_SIZE);
    needed_size = ALIGN_UP(size, PAGE_SIZE);

    /* 树为空，整个范围都是空闲的 */
    if (RB_EMPTY_ROOT(&mm->mm_region_tree))
    {
        return ((search_end - search_start) >= needed_size) ? search_start : 0;
    }

    /* 从最左边的节点开始线性扫描，确保 search_start 生效 */
    node = mm->mm_region_tree.rb_node;
    while (node && node->rb_left)
        node = node->rb_left;
    prev = node ? rb_entry(node, struct mm_region, rb_node) : NULL;

    gap_start = search_start;
    if (prev)
    {
        /* 检查 [search_start, prev->start) */
        gap_end = prev->virtual_address;
        if (gap_end > search_end)
            gap_end = search_end;

        if (gap_start < gap_end && gap_end - gap_start >= needed_size)
        {
            return gap_start;
        }

        gap_start = prev->virtual_address + (prev->region_page_count << PAGE_SIZE_SHIFT);
        if (gap_start < search_start)
            gap_start = search_start;
    }

    /* 第三步：遍历后续节点，检查间隙 */
    while (prev)
    {
        /* 获取下一个节点 */
        node = rb_next_node(&prev->rb_node);
        if (!node)
            break;

        area = rb_entry(node, struct mm_region, rb_node);

        /* 检查 [gap_start, region->va) 之间的间隙 */
        gap_end = area->virtual_address;
        if (gap_end > search_end)
            gap_end = search_end;

        if (gap_start < gap_end && gap_end - gap_start >= needed_size)
        {
            return gap_start;
        }

        /* 更新 gap_start */
        gap_start = area->virtual_address + (area->region_page_count << PAGE_SIZE_SHIFT);
        if (gap_start < search_start)
            gap_start = search_start;

        /* 如果已经超出搜索范围，停止 */
        if (gap_start >= search_end)
            break;

        prev = area;
    }

    /* 检查最后一个间隙 [gap_start, search_end) */
    if (gap_start < search_end && search_end - gap_start >= needed_size)
    {
        return gap_start;
    }

    /* 没有找到足够大的间隙 */
    return -1;
}

int mm_region_map(struct mm *mm, struct mm_region *region)
{
    unsigned long flags;
    int find_vaild_varegion = 0;
    int ret = -1;
    if (region->flags & MAP_FIXED)
    {
        struct mm_region *old_region = mm_va2region(mm, region->map_min);
        if (old_region != NULL)
        {
            region->virtual_address = region->map_min;
            mm_region_modify(mm, region, true);
        }
    }

    flags = write_lock_irqsave(&mm->mm_region_lock);

    if ((region->virtual_address == 0) && (mm->mm_region_type == MM_REGION_TYPE_USER) &&
        page_allocer_inited() && ((region->flags & MAP_FIXED) == 0))
    {
        virt_addr_t va_start = region->map_min ? region->map_min : USER_SPACE_START;
        if (va_start < MMAP_START_VADDR)
        {
            va_start = MMAP_START_VADDR;
        }
        virt_addr_t va_end = USER_SPACE_END + 1;

        region->virtual_address = mm_vmalloc(mm, region->region_page_count, va_start, va_end);
        ret = mm->ops->map(mm, region);
    }
    else
    {
        ret = mm->ops->map(mm, region);
    }

    if (ret == 0)
    {
        /* 插入红黑树和有序链表 */
        if (mm_region_insert_rbtree(mm, region) != 0)
        {
            /* 地址冲突，取消映射 */
            mm->ops->unmap(mm, region);
            ret = -1;
        }
    }
    write_unlock_irqrestore(&mm->mm_region_lock, flags);
    return ret;
}
extern uintptr_t g_tmp_fp;
int mm_region_unmap(struct mm *mm, struct mm_region *region)
{
    if (region == NULL || !kernel_access_check(region, sizeof(*region), UACCESS_R))
    {
        return -1;
    }
    if (region->list.next == NULL ||
        !kernel_access_check(region->list.next, sizeof(*region->list.next), UACCESS_R))
    {
        return -1;
    }
    int ret = mm->ops->unmap(mm, region);
    if (ret == 0)
    {
        unsigned long flags;
        flags = write_lock_irqsave(&mm->mm_region_lock);
        mm_region_remove_rbtree(mm, region);
        write_unlock_irqrestore(&mm->mm_region_lock, flags);
    }
    if (region->munmap)
    {
        region->munmap(region);
    }
    if (region->flags & MAP_IS_FILE)
    {
        if (region->filepath)
        {
            region->flags = 0;
            free(region->filepath);
            region->filepath = NULL;
        }
    }
    if (region->is_page_free && region->physical_address)
    {
        pages_free(region->physical_address, region->page_order);
    }
    if (region->is_need_free)
    {
        free(region);
    }
    return ret;
}

int mm_region_modify(struct mm *mm, struct mm_region *region, bool is_unmap)
{
    virt_addr_t border1, border2;
    struct mm_region *old_region;
    irq_flags_t flags;

    size_t maped1_page_count, maped2_page_count;

    /* 计算修改边界1 */
    border1 = region->virtual_address;

    /* 计算修改边界2 */
    border2 = region->virtual_address + region->region_page_count * ttosGetPageSize();

    flags = write_lock_irqsave(&mm->mm_region_lock);

    /* 获取原来的映射区域（使用红黑树查找） */
    old_region = va2region_rbtree(&mm->mm_region_tree, border1);

    /* 检查修改区域是否在同一映射区域内 */
    if (old_region != va2region_rbtree(&mm->mm_region_tree, border2 - 1))
    {
        write_unlock_irqrestore(&mm->mm_region_lock, flags);
        return -1;
    }

    /* 检查是否已映射 */
    if (old_region == NULL)
    {
        write_unlock_irqrestore(&mm->mm_region_lock, flags);
        return -1;
    }

    if (is_unmap)
    {
        /* 后面会对页的引用计数减1 此时不能释放，否则页就没了 */
        mm->ops->unmap(mm, region);

        /* 从红黑树和链表移除旧的区域 */
        mm_region_remove_rbtree(mm, old_region);
    }
    else
    {
        /* 申请新的映射区域 */
        struct mm_region *map_region = malloc(sizeof(struct mm_region));
        if (map_region == NULL)
        {
            write_unlock_irqrestore(&mm->mm_region_lock, flags);
            return -1;
        }

        /* 拷贝新的映射区域 */
        memcpy(map_region, old_region, sizeof(struct mm_region));
        map_region->is_need_free = true;
        map_region->region_page_count = region->region_page_count;
        map_region->virtual_address = region->virtual_address;
        map_region->physical_address = region->physical_address;
        map_region->mem_attr = region->mem_attr;
        map_region->map_min = map_region->virtual_address;

        if (map_region->flags & MAP_IS_FILE)
        {
            map_region->filepath = strdup(old_region->filepath);
        }

        /* 执行属性修改 */
        mm->ops->modify(mm, map_region);

        /* 移除旧的区域，插入新的区域 */
        mm_region_remove_rbtree(mm, old_region);
        mm_region_insert_rbtree(mm, map_region);
    }

    /* 计算原区域边界1前的页大小 */
    maped1_page_count = (border1 - old_region->virtual_address) / ttosGetPageSize();

    if (maped1_page_count)
    {
        struct mm_region *map_region1 = malloc(sizeof(struct mm_region));
        if (map_region1 == NULL)
        {
            write_unlock_irqrestore(&mm->mm_region_lock, flags);
            return -1;
        }
        memcpy(map_region1, old_region, sizeof(struct mm_region));
        map_region1->is_need_free = true;
        map_region1->region_page_count = maped1_page_count;

        if (map_region1->is_page_free)
        {
            /* 有边界1映射，将页组的引用计数+1 */
            page_ref_inc(map_region1->physical_address, map_region1->page_order);
        }

        if (map_region1->flags & MAP_IS_FILE)
        {
            map_region1->filepath = strdup(old_region->filepath);
            map_region1->offset =
                PAGE_ALIGN_DOWN(old_region->offset) + (border1 - old_region->virtual_address);
        }

        /* 插入区域1到红黑树和链表 */
        mm_region_insert_rbtree(mm, map_region1);
    }

    /* 计算原区域边界2后的页大小 */
    maped2_page_count = (old_region->virtual_address +
                         old_region->region_page_count * ttosGetPageSize() - border2) /
                        ttosGetPageSize();
    if (maped2_page_count)
    {
        struct mm_region *map_region2 = malloc(sizeof(struct mm_region));
        if (map_region2 == NULL)
        {
            write_unlock_irqrestore(&mm->mm_region_lock, flags);
            return -1;
        }
        memcpy(map_region2, old_region, sizeof(struct mm_region));
        map_region2->is_need_free = true;
        map_region2->region_page_count = maped2_page_count;
        map_region2->map_min = map_region2->virtual_address = border2;
        map_region2->physical_address =
            old_region->physical_address + (border2 - old_region->virtual_address);

        if (map_region2->flags & MAP_IS_FILE)
        {
            map_region2->filepath = strdup(old_region->filepath);
            map_region2->offset =
                PAGE_ALIGN_DOWN(old_region->offset) + (border2 - old_region->virtual_address);
        }

        if (map_region2->is_page_free)
        {
            /* 有边界2映射，将页组的引用计数+1 */
            page_ref_inc(map_region2->physical_address, map_region2->page_order);
        }

        /* 插入区域2到红黑树和链表 */
        mm_region_insert_rbtree(mm, map_region2);
    }

    if (is_unmap && old_region->is_page_free)
    {
        /* 如果是 unmap 则需要对引用计数-1 */
        pages_free(old_region->physical_address, old_region->page_order);
    }

    if (old_region->is_need_free)
    {
        if (old_region->flags & MAP_IS_FILE)
        {
            if (old_region->filepath)
            {
                free(old_region->filepath);
            }
        }
        free(old_region);
        old_region = NULL;
    }

    write_unlock_irqrestore(&mm->mm_region_lock, flags);

    return 0;
}

static inline int mm_kernel_region_map(struct mm_region *region)
{
    return mm_region_map(&g_kernel_mm, region);
}

static inline int mm_kernel_region_unmap(struct mm_region *region)
{
    return mm_region_unmap(&g_kernel_mm, region);
}

/**
 * @brief 将 mm_region 插入红黑树，同时维护有序链表
 * @param mm 内存管理结构
 * @param region 要插入的区域
 * @return 0 成功，-1 失败（地址冲突）
 *
 * 设计原则：
 * - 红黑树按 virtual_address 排序
 * - 链表顺序由红黑树的中序遍历决定（地址递增）
 * - 插入时同时更新红黑树和链表
 *
 * 关键优化（Linus 式 "好品味"）：
 * - 利用红黑树遍历时的 parent 节点直接定位链表插入位置
 * - 如果插入左子树 → parent 是后继 → 插入到 parent 之前
 * - 如果插入右子树 → parent 是前驱 → 插入到 parent 之后
 * - 避免了 O(n) 的链表遍历，总复杂度保持 O(log n)
 */
static int mm_region_insert_rbtree(struct mm *mm, struct mm_region *region)
{
    struct rb_node **new_node = &mm->mm_region_tree.rb_node;
    struct rb_node *parent = NULL;
    struct mm_region *parent_region = NULL;
    struct mm_region *this;
    virt_addr_t region_end = region->virtual_address + region->region_page_count * PAGE_SIZE;
    virt_addr_t this_end;
    bool insert_left = false;

    // if(region->virtual_address == 0)
    // {
    //     while(1);
    // }

    /* 查找插入位置 */
    while (*new_node)
    {
        this = rb_entry(*new_node, struct mm_region, rb_node);
        this_end = this->virtual_address + this->region_page_count * PAGE_SIZE;
        parent = *new_node;
        parent_region = this;

        /* 检查地址冲突 */
        if (region->virtual_address < this_end && region_end > this->virtual_address)
        {
            /* 地址范围重叠 */
            return -1;
        }

        if (region->virtual_address < this->virtual_address)
        {
            new_node = &((*new_node)->rb_left);
            insert_left = true;
        }
        else
        {
            new_node = &((*new_node)->rb_right);
            insert_left = false;
        }
    }

    /* 初始化新节点的 rb_subtree_gap */
    region->rb_subtree_gap = 0;

    /* 插入红黑树 */
    rb_link_node(&region->rb_node, parent, new_node);
    rb_insert_color(&region->rb_node, &mm->mm_region_tree);

    /* 维护有序链表：利用红黑树的 parent 节点直接定位 */
    if (parent_region == NULL)
    {
        /* 树为空，直接添加到链表头 */
        list_add(&region->list, &mm->mm_region_list);
    }
    else if (insert_left)
    {
        /* 插入到左子树 → parent 是后继节点 → 插入到 parent 之前 */
        list_add_tail(&region->list, &parent_region->list);
    }
    else
    {
        /* 插入到右子树 → parent 是前驱节点 → 插入到 parent 之后 */
        list_add(&region->list, &parent_region->list);
    }

    /* 更新从插入节点到根节点路径上所有节点的 rb_subtree_gap */
    struct mm_region *curr = region;
    while (curr)
    {
        update_subtree_gap(curr, (virt_addr_t)-1);

        /* 向上遍历到父节点 */
        struct rb_node *parent_node = rb_parent(&curr->rb_node);
        if (!parent_node)
            break;
        curr = rb_entry(parent_node, struct mm_region, rb_node);
    }

    return 0;
}

/**
 * @brief 从红黑树和链表中删除 mm_region
 * @param mm 内存管理结构
 * @param region 要删除的区域
 */
static void mm_region_remove_rbtree(struct mm *mm, struct mm_region *region)
{
    struct rb_node *parent_node;
    struct mm_region *parent_region = NULL;

    /* 保存父节点，用于后续更新 rb_subtree_gap */
    parent_node = rb_parent(&region->rb_node);
    if (parent_node)
        parent_region = rb_entry(parent_node, struct mm_region, rb_node);

    /* 从红黑树删除 */
    rb_erase(&region->rb_node, &mm->mm_region_tree);

    /* 从链表删除 */
    list_del(&region->list);

    /* 更新从父节点到根节点路径上所有节点的 rb_subtree_gap */
    while (parent_region)
    {
        update_subtree_gap(parent_region, (virt_addr_t)-1);

        parent_node = rb_parent(&parent_region->rb_node);
        if (!parent_node)
            break;
        parent_region = rb_entry(parent_node, struct mm_region, rb_node);
    }
}

/**
 * @brief 基于红黑树查找虚拟地址对应的 mm_region
 * @param tree 红黑树根节点
 * @param vaddr 虚拟地址
 * @return 找到返回 mm_region 指针，否则返回 NULL
 *
 * 时间复杂度：O(log n)
 */
static struct mm_region *va2region_rbtree(struct rb_root *tree, virt_addr_t vaddr)
{
    struct rb_node *node = tree->rb_node;
    struct mm_region *region;
    virt_addr_t region_end;

    while (node)
    {
        region = rb_entry(node, struct mm_region, rb_node);
        region_end = region->virtual_address + region->region_page_count * PAGE_SIZE;

        if (vaddr < region->virtual_address)
        {
            node = node->rb_left;
        }
        else if (vaddr >= region_end)
        {
            node = node->rb_right;
        }
        else
        {
            /* vaddr 在 [region->virtual_address, region_end) 范围内 */
            return region;
        }
    }
    return NULL;
}

/**
 * @brief 基于链表查找虚拟地址对应的 mm_region（兼容旧代码）
 * @deprecated 使用 va2region_rbtree 替代
 */
static struct mm_region *va2region_r(struct list_head *head, virt_addr_t vaddr)
{
    struct list_head *pos;
    struct mm_region *region;
    if (list_empty(head))
    {
        return NULL;
    }
    list_for_each(pos, head)
    {
        region = list_entry(pos, struct mm_region, list);
        if (region->virtual_address <= vaddr &&
            (region->virtual_address + region->region_page_count * PAGE_SIZE) > vaddr)
        {
            return region;
        }
    }
    return NULL;
}

struct mm_region *mm_va2region(struct mm *mm, virt_addr_t vaddr)
{
    struct mm_region *ret;
    unsigned long flags;
    flags = read_lock_irqsave(&mm->mm_region_lock);
    ret = va2region_rbtree(&mm->mm_region_tree, vaddr);
    read_unlock_irqrestore(&mm->mm_region_lock, flags);
    return ret;
}

volatile void *ioremap(phys_addr_t io_base, size_t size)
{
    struct mm_region *region;

    phys_addr_t io_map_base = PAGE_ALIGN_DOWN(io_base);

    region = (struct mm_region *)malloc(sizeof(struct mm_region));

    mm_region_init(region, MT_DEVICE | MT_KERNEL, io_map_base, PAGE_ALIGN(size));
    region->is_need_free = true;
    mm_kernel_region_map(region);

    return (volatile void *)region->virtual_address + (io_base - io_map_base);
}

void iounmap(virt_addr_t vaddr)
{
    struct mm_region *region;
    region = mm_va2region(&g_kernel_mm, vaddr);
    if (region != NULL)
    {
        mm_kernel_region_unmap(region);
    }
}

phys_addr_t mm_v2p(struct mm *mm, virt_addr_t va)
{
    return g_kernel_mm.ops->v2p(mm, va);
}

#define VA_RAM_BASE ((virt_addr_t)KERNEL_SPACE_START)
#define VA_RAM_END (VA_RAM_BASE + ((max_low_pfn - min_low_pfn + 1) << PAGE_SIZE_SHIFT))

phys_addr_t mm_kernel_v2p(virt_addr_t va)
{
    if (va < VA_RAM_END && va >= VA_RAM_BASE)
        return (phys_addr_t)va + g_kernel_mm.pv_offset;
    return mm_v2p(&g_kernel_mm, va);
}

virt_addr_t mm_kernel_p2v(phys_addr_t pa)
{
    if (page_address(PAGE_ALIGN_DOWN(pa)))
        return pa - g_kernel_mm.pv_offset;
    return 0;
}

int ttosSetPageAttribute(virt_addr_t va, size_t size, uint64_t attr)
{
    struct mm_region region;
    mm_region_init(&region, attr, 0, size);
    region.virtual_address = va;
    region.physical_address = mm_kernel_v2p(va);

    return mm_region_modify(&g_kernel_mm, &region, false);
}

#ifdef CONFIG_SHELL

#include <shell.h>

static int shell_out(const char *str)
{
    int ret;
    Shell *shell = shellGetCurrent();
    SHELL_LOCK(shell);
    ret = shellWriteString(shell, str);
    SHELL_UNLOCK(shell);
    return ret;
}

int mmu_print_table_command(int argc, const char *argv[])
{
    return g_kernel_mm.ops->print_table(&g_kernel_mm, shell_out);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 mmu, mmu_print_table_command, print mmu kernel table);
#endif
