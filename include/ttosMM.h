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

#ifndef __TTOSMM_H__
#define __TTOSMM_H__

#include <commonTypes.h>
#include <list.h>
#include <mmu.h>
#include <spinlock.h>
#include <stdint.h>
#include <system/const.h>
#include <ttosRBTree.h>

#define DATA_SIGMENT_VADDR 0x80000000UL
#define LDSO_LOAD_VADDR 0x60000000UL
#define MMAP_START_VADDR 0x00001000UL
#define EXEC_LOAD_VADDR 0

#define MM_REGION_TYPE_USER 0
#define MM_REGION_TYPE_KERNEL 1
#define MM_REGION_TYPE_HYPER 2

#define ZONE_NORMAL 0
#define ZONE_DMA 1
#define ZONE_HIGH 2
#define ZONE_DMA32 3
#define ZONE_ANYWHERE 4

struct mm_region;
struct mm;

struct mmu_ops
{
    int (*init)(struct mm *mm);
    int (*switch_space)(struct mm *from, struct mm *to);
    int (*map)(struct mm *mm, struct mm_region *region);
    int (*modify)(struct mm *mm, struct mm_region *region);
    int (*unmap)(struct mm *mm, struct mm_region *region);
    phys_addr_t (*v2p)(struct mm *mm, virt_addr_t va);
    virt_addr_t (*valloc)(struct mm *mm, size_t page_count);
    int (*apply_change)(struct mm *mm);
    int (*print_table)(struct mm *mm, int (*output)(const char *str));
    int (*deinit)(struct mm *mm);
};

#define MAP_IS_STACK (0x01 << 24)
#define MAP_IS_HEAP (0x01 << 25)
#define MAP_IS_FILE (0x01 << 26)

/* struct mm_region 会被同时插入到 mm_region_list和 mm_region_tree中
 * mm_region_tree 用于 分配 释放 以及 快速查找
 * mm_region_list 用于遍历(显示maps等)
 */
struct mm_region
{
    struct rb_node rb_node;
    struct list_head list;
    virt_addr_t virtual_address;
    phys_addr_t physical_address;
    size_t region_page_count;

    /* Augmented Red-Black Tree - 该子树中最大的间隙
     * 用于 O(log n) 查找空闲区域，类似 Linux 的 rb_subtree_gap
     */
    virt_addr_t rb_subtree_gap;
    uint64_t mem_attr;
    int is_need_free;
    int is_page_free;
    int page_order;
    union {
        void *p;
        int i;
    } priv;
    int flags;
    virt_addr_t map_min;
    char *filepath;
    struct file *f;
    size_t offset;
    /* when fork call this */
    void (*do_fork)(struct mm_region *entry);
    // for file mmap
    int (*munmap)(struct mm_region *entry);
};

struct page_obj
{
    uintptr_t page_start;
    uintptr_t page_count;
};

struct mm
{
    phys_addr_t mmu_table_base;
    uint64_t mm_region_type;
    uint64_t asid;
    size_t page_size;
    size_t page_size_shift;
    phys_addr_t pv_offset;
    virt_addr_t start_data_segment;
    virt_addr_t end_data_segment;

    /* mm_region 管理：红黑树 + 有序链表 */
    struct rb_root mm_region_tree;   /* 快速查找 O(log n) */
    struct list_head mm_region_list; /* 有序遍历（地址递增） */
    rw_spinlock_t mm_region_lock;    /* 保护上述两个数据结构 */
    ttos_spinlock_t ptable_lock;     /* 保护该 mm 的页表遍历和修改 */

    struct mmu_ops *ops;
    int first_level; /* for arm Long-descriptor */
};

/*
 * Shifts and masks to access fields of an mmap attribute
 */
#define MT_TYPE_MASK U(0x7)
#define MT_TYPE(_attr) ((_attr)&MT_TYPE_MASK)
/* Access permissions (RO/RW) */
#define MT_PERM_SHIFT U(3)

/* Physical address space (SECURE/NS/Root/Realm) */
#define MT_PAS_SHIFT U(4)
#define MT_PAS_MASK (U(3) << MT_PAS_SHIFT)
#define MT_PAS(_attr) ((_attr)&MT_PAS_MASK)

/* Access permissions for instruction execution (EXECUTE/EXECUTE_NEVER) */
#define MT_EXECUTE_SHIFT U(6)
/* In the EL1&0 translation regime, User (EL0) or Privileged (EL1). */
#define MT_USER_SHIFT U(7)

/* Shareability attribute for the memory region */
#define MT_SHAREABILITY_SHIFT U(8)
#define MT_SHAREABILITY_MASK (U(3) << MT_SHAREABILITY_SHIFT)
#define MT_SHAREABILITY(_attr) ((_attr)&MT_SHAREABILITY_MASK)

#define MT_ACCESS_FLAG_SHIFT U(12)

/* All other bits are reserved */

/*
 * Memory mapping attributes
 */

/*
 * Memory types supported.
 * These are organised so that, going down the list, the memory types are
 * getting weaker; conversely going up the list the memory types are getting
 * stronger.
 */
#define MT_DEV U(0)
#define MT_NON_CACHEABLE U(1)
#define MT_MEMORY U(2)
#define MT_WRITE_THROUGHOUT U(3)

/* Values up to 7 are reserved to add new memory types in the future */

#define MT_RO (U(0) << MT_PERM_SHIFT)
#define MT_RW (U(1) << MT_PERM_SHIFT)

#define MT_NO_ACCESS (U(1) << MT_ACCESS_FLAG_SHIFT)

#define MT_SECURE (U(0) << MT_PAS_SHIFT)
#define MT_NSECURE (U(1) << MT_PAS_SHIFT)

/*
 * Access permissions for instruction execution are only relevant for normal
 * read-only memory, i.e. MT_MEMORY | MT_RO. They are ignored (and potentially
 * overridden) otherwise:
 *  - Device memory is always marked as execute-never.
 *  - Read-write normal memory is always marked as execute-never.
 */
#define MT_EXECUTE (U(0) << MT_EXECUTE_SHIFT)
#define MT_EXECUTE_NEVER (U(1) << MT_EXECUTE_SHIFT)

/*
 * When mapping a region at EL0 or EL1, this attribute will be used to determine
 * if a User mapping (EL0) will be created or a Privileged mapping (EL1).
 */
#define MT_USER (U(1) << MT_USER_SHIFT)
#define MT_KERNEL (U(0) << MT_USER_SHIFT)

/*
 * Shareability defines the visibility of any cache changes to
 * all masters belonging to a shareable domain.
 *
 * MT_SHAREABILITY_ISH: For inner shareable domain
 * MT_SHAREABILITY_OSH: For outer shareable domain
 * MT_SHAREABILITY_NSH: For non shareable domain
 */
#define MT_SHAREABILITY_ISH (U(1) << MT_SHAREABILITY_SHIFT)
#define MT_SHAREABILITY_OSH (U(2) << MT_SHAREABILITY_SHIFT)
#define MT_SHAREABILITY_NSH (U(3) << MT_SHAREABILITY_SHIFT)

/* Compound attributes for most common usages */
#define MT_CODE (MT_MEMORY | MT_RO | MT_EXECUTE | MT_SHAREABILITY_ISH)
#define MT_RO_DATA (MT_MEMORY | MT_RO | MT_EXECUTE_NEVER | MT_SHAREABILITY_ISH)
#define MT_RW_DATA (MT_MEMORY | MT_RW | MT_EXECUTE_NEVER | MT_SHAREABILITY_ISH)
#define MT_RWX_DATA (MT_MEMORY | MT_RW | MT_EXECUTE | MT_SHAREABILITY_ISH)
#define MT_DEVICE (MT_DEV | MT_RW | MT_EXECUTE_NEVER | MT_SHAREABILITY_OSH)
#define MT_NACCESS (MT_MEMORY | MT_NO_ACCESS | MT_EXECUTE_NEVER)
#define MT_NCACHE (MT_NON_CACHEABLE | MT_RW | MT_EXECUTE_NEVER | MT_SHAREABILITY_OSH)

#define MT_KERNEL_MEM (MT_RW_DATA | MT_KERNEL)
#define MT_KERNEL_IO (MT_DEVICE | MT_KERNEL)

#define MT_ATTR_SET_TYPE(mm, type) (((mm)->mem_attr & ~MT_TYPE_MASK) | MT_TYPE(type))

#define MMAP_SET_ATTR_TYPE(mm, type) MT_ATTR_SET_TYPE(mm, type)

#define PAGE_ALIGN(s) ALIGN_UP(s, PAGE_SIZE)
#define PAGE_ALIGN_DOWN(s) ALIGN_DOWN(s, PAGE_SIZE)

bool page_allocer_inited(void);
virt_addr_t page_address(phys_addr_t addr);
static inline bool memory_is_zone(phys_addr_t addr, int zone)
{
    switch (zone)
    {
    case ZONE_NORMAL:
        return !!page_address(addr);
    case ZONE_DMA:
        return true;
    case ZONE_HIGH:
        return !page_address(addr);
    case ZONE_DMA32:
        if (addr > UINT32_MAX)
        {
            return false;
        }
        return !!page_address(addr);
    case ZONE_ANYWHERE:
        return true;
    default:
        return false;
    }
    return false;
}

T_EXTERN int32_t ttosMemoryManagerInit(void);
T_EXTERN struct page_obj *ttosPageAllocObj(uint32_t count);
T_EXTERN T_ULONG ttosGetPageSize(void);
T_EXTERN int32_t ttosPageGetInfo(uintptr_t *total_nr, uintptr_t *free_nr);
T_EXTERN int ttosSetPageAttribute(virt_addr_t va, size_t size, uint64_t attr);
phys_addr_t mm_kernel_v2p(virt_addr_t va);
phys_addr_t mm_v2p(struct mm *mm, virt_addr_t va);
virt_addr_t mm_kernel_p2v(phys_addr_t pa);
volatile void *ioremap(phys_addr_t io_base, size_t size);
void iounmap(virt_addr_t vaddr);
struct mm *get_kernel_mm(void);
int mm_switch_space_to(struct mm *mm);
int mm_init(struct mm *mm);
int mm_destroy(struct mm *mm);
struct mm_region *mm_va2region(struct mm *mm, virt_addr_t vaddr);
phys_addr_t zone_normal_pvoffset(void);
virt_addr_t fix_map_set(int idx, phys_addr_t phy, uint64_t prot);
virt_addr_t fix_map_vaddr(int idx);
virt_addr_t ktmp_access_start(phys_addr_t paddr);
void ktmp_access_end(virt_addr_t vaddr);
int kmap(virt_addr_t va, phys_addr_t pa, size_t size, uint64_t attr);
static inline virt_addr_t mm_valloc(struct mm *mm, size_t size)
{
    return mm->ops->valloc(mm, ALIGN_UP(size, PAGE_SIZE) >> PAGE_SIZE_SHIFT);
}

static inline virt_addr_t mm_vpagealloc(struct mm *mm, size_t page_count)
{
    return mm->ops->valloc(mm, page_count);
}

void mm_region_init(struct mm_region *region, uintptr_t attr, phys_addr_t pa, size_t size);
int mm_region_map(struct mm *mm, struct mm_region *region);
int mm_region_unmap(struct mm *mm, struct mm_region *region);
int mm_region_modify(struct mm *mm, struct mm_region *region, bool is_unmap);

int mm_foreach_region(struct mm *mm,
                      void (*func)(struct mm *mm, struct mm_region *region, void *ctx), void *ctx);

#endif /* __TTOSMM_H__ */
