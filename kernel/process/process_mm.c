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

#include <assert.h>
#include <cache.h>
#include <page.h>
#include <stdio.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <uaccess.h>

#define _GNU_SOURCE 1
#include <sys/mman.h>

#define KLOG_TAG "PROCESS_MM"
#include <klog.h>

static void process_mm_destroy(void *obj)
{
    struct mm *mm = obj;
    /* 不应该释放内核内存管理模块 */
    if (mm->mm_region_type == MM_REGION_TYPE_KERNEL)
    {
        return;
    }
    mm_destroy(obj);
    free(obj);
}

void process_mm_ref(pcb_t parent, pcb_t child)
{
    child->mm = process_obj_ref(child, parent->mm);
}

void process_mm_create(pcb_t child)
{
    struct mm *child_mm = calloc(1, sizeof(*child_mm));
    child_mm->mmu_table_base = 0;
    child_mm->asid = 0;
    child_mm->mm_region_list = LIST_HEAD_INIT(child_mm->mm_region_list);
    child_mm->mm_region_type = MM_REGION_TYPE_USER;
    child_mm->page_size = 0;
    child_mm->ops = NULL;
    mm_init(child_mm);
    child->mm = process_obj_create(child, child_mm, process_mm_destroy);

    /* 从内核到用户态不需要拷贝mmu表 */
}

static void foreach_region(struct mm *mm, struct mm_region *region, void *ctx)
{
    pcb_t child = (pcb_t)ctx;
    int i;

    write_unlock(&mm->mm_region_lock);

    struct mm_region *uregion = calloc(1, sizeof(struct mm_region));

    assert(uregion != NULL);
    memcpy(uregion, region, sizeof(struct mm_region));
    uregion->is_need_free = true;

    if (!(region->flags & MAP_SHARED))
    {
        /* 分配新物理内存 */
        uregion->page_order = page_bits(region->region_page_count << PAGE_SIZE_SHIFT);
        uregion->physical_address = pages_alloc(uregion->page_order, ZONE_ANYWHERE);

        if (uregion->physical_address == 0)
        {
            free(uregion);
            assert("No memory" && 0);
            write_lock(&mm->mm_region_lock);
            return;
        }
        uregion->is_page_free = true;
        /* 拷贝内存数据 这里是用的父进程的用户态地址
         * 所以必须要在父进程中调用 */
        if (!(uregion->mem_attr & MT_NO_ACCESS))
        {
            for (i = 0; i < uregion->region_page_count; i++)
            {
                irq_flags_t flags;
                virt_addr_t kaddr =
                    ktmp_access_start(uregion->physical_address + (i << PAGE_SIZE_SHIFT));
                memcpy((void *)kaddr,
                       (const void *)region->virtual_address + (i << PAGE_SIZE_SHIFT), PAGE_SIZE);
                /* 刷cache */
                cache_dcache_flush(kaddr, PAGE_SIZE);
                ktmp_access_end(kaddr);
            }
        }
        else
        {
            for (i = 0; i < uregion->region_page_count; i++)
            {
                irq_flags_t flags;
                virt_addr_t kaddr =
                    ktmp_access_start(uregion->physical_address + (i << PAGE_SIZE_SHIFT));
                memset((void *)kaddr, 0, PAGE_SIZE);
                /* 刷cache */
                cache_dcache_flush(kaddr, PAGE_SIZE);
                ktmp_access_end(kaddr);
            }
        }
    }
    else
    {
        if (region->is_page_free)
        {
            page_ref_inc(uregion->physical_address, uregion->page_order);
        }
    }

    if (uregion->flags & MAP_IS_FILE)
    {
        if (uregion->do_fork)
        {
            uregion->do_fork(uregion);
        }
        if (region->filepath)
        {
            uregion->filepath = strdup(region->filepath);
        }
        else
        {
            uregion->flags &= ~MAP_IS_FILE;
            KLOG_E("flags is file but not have filepath");
        }
    }

    /* 映射内存 */
    mm_region_map(get_process_mm(child), uregion);

    write_lock(&mm->mm_region_lock);
}

void process_mm_copy(pcb_t parent, pcb_t child)
{
    struct mm *parent_mm = get_process_mm(parent);

    process_mm_create(child);

    /* 从内核到用户态不需要拷贝mmu表 */
    if (parent_mm->mm_region_type == MM_REGION_TYPE_KERNEL)
    {
        return;
    }

    mm_foreach_region(parent_mm, foreach_region, child);

    get_process_mm(child)->end_data_segment = parent_mm->end_data_segment;
    get_process_mm(child)->start_data_segment = parent_mm->start_data_segment;
}

phys_addr_t process_mm_map(pcb_t pcb, phys_addr_t paddr, virt_addr_t *vaddr, uintptr_t attr,
                           size_t page_count, int flags)
{
    /* 创建用户态映射 */

    struct mm_region *uregion = (struct mm_region *)calloc(1, sizeof(struct mm_region));
    if (uregion == NULL)
    {
        KLOG_E("%s %d: no memory!", __func__, __LINE__);
        return 0;
    }
    /* 初始化映射区域信息 */

    mm_region_init(uregion, attr, 0, page_count << get_process_mm(pcb)->page_size_shift);
    if (paddr == 0)
    {
        uregion->page_order = page_bits(page_count << PAGE_SIZE_SHIFT);
        paddr = pages_alloc(uregion->page_order, ZONE_ANYWHERE);
        if (paddr == 0)
        {
            KLOG_E("%s %d: no memory!", __func__, __LINE__);
            free(uregion);
            return 0;
        }
        uregion->is_page_free = true;
    }
    uregion->physical_address = paddr;
    uregion->is_need_free = true;
    uregion->flags = flags;
    if (vaddr)
    {
        uregion->map_min = *vaddr;
    }

    /* 执行映射 */
    mm_region_map(get_process_mm(pcb), uregion);
    if (vaddr)
    {
        *vaddr = uregion->virtual_address;
    }

    return paddr;
}

int process_mm_unmap(pcb_t pcb, virt_addr_t vaddr)
{
    struct mm_region *region = mm_va2region(get_process_mm(pcb), vaddr);
    if (region == NULL)
    {
        return -EFAULT;
    }
    mm_region_unmap(get_process_mm(pcb), region);
    return 0;
}

uint64_t process_prot_to_attr(unsigned long prot)
{
    uint64_t attr = MT_USER | MT_MEMORY | MT_SHAREABILITY_ISH;
    if (prot & PROT_READ)
    {
        // do nothing
    }
    else if (prot == PROT_NONE)
    {
        attr |= MT_NO_ACCESS;
    }

    if (prot & PROT_WRITE)
    {
        attr |= MT_RW;
    }
    else
    {
        attr |= MT_RO;
    }

    if (prot & PROT_EXEC)
    {
        attr |= MT_EXECUTE;
    }
    else
    {
        attr |= MT_EXECUTE_NEVER;
    }
    return attr;
}

unsigned long process_attr_to_prot(uint64_t attr)
{
    unsigned long prot = PROT_READ;
    if (attr & MT_NO_ACCESS)
    {
        return PROT_NONE;
    }
    if (attr & MT_RW)
    {
        prot |= PROT_WRITE;
    }
    if (!(attr & MT_EXECUTE_NEVER))
    {
        prot |= PROT_EXEC;
    }
    return prot;
}

int process_mmap(pcb_t pcb, unsigned long *addr, unsigned long len, unsigned long prot,
                 unsigned long flags, struct file *f, off_t off, int file_len)
{
    int ret = 0;
    uint64_t attr = process_prot_to_attr(prot);
    struct mm_region *entry;
    int i;
    off_t off_in_page = off - ALIGN_DOWN(off, PAGE_SIZE);
    pcb_t caller = ttosProcessSelf();
    bool need_zero = false;
    off_t file_pos = 0;

    len = PAGE_ALIGN(len + off_in_page);

    entry = calloc(1, sizeof(struct mm_region));
    if (entry == NULL)
    {
        return -ENOMEM;
    }
    mm_region_init(entry, attr, 0, len);

    if ((flags & MAP_FIXED) != 0)
    {
        if (*addr >= USER_SPACE_END)
        {
            free(entry);
            return -EINVAL;
        }
    }
    entry->map_min = *addr;
    entry->map_min = PAGE_ALIGN_DOWN(entry->map_min);

    if ((flags & MAP_FIXED) && ((*addr - entry->map_min) != off_in_page))
    {
        free(entry);
        return -EINVAL;
    }
    entry->offset = off;
    entry->f = f;
    entry->flags = flags;
    entry->is_need_free = true;

    // map file
    if ((flags & MAP_ANONYMOUS) == 0)
    {
        if (f == NULL)
        {
            free(entry);
            return -EBADFD;
        }
        char *p = process_getfilefullpath(f, "");
        if (p)
        {
            entry->filepath = strdup(p);
            entry->flags |= MAP_IS_FILE;
            entry->offset = off;
            free(p);
        }

        if (f->f_inode && f->f_inode->u.i_ops->mmap != NULL)
        {
            ret = f->f_inode->u.i_ops->mmap(f, entry);
            if (ret < 0 && ret != -ENOTTY)
            {
                free(entry);
                return ret;
            }
        }
        /* 获取文件当前位置 */
        file_pos = file_seek(f, 0, SEEK_CUR);
        if (MT_TYPE(entry->mem_attr) != MT_DEV)
        {
            /* 设置文件位置 */
            ret = file_seek(f, off, SEEK_SET);
            if (ret < 0)
            {
                goto error_out;
            }
        }
    }

    if (entry->physical_address == 0)
    {
        entry->page_order = page_bits(len);
        entry->physical_address = pages_alloc(entry->page_order, ZONE_ANYWHERE);
        if (entry->physical_address == 0)
        {
            free(entry);
            if (f != NULL)
            {
                /* 设置文件位置 */
                file_seek(f, file_pos, SEEK_SET);
            }
            return -ENOMEM;
        }
        entry->is_page_free = true;
        need_zero = true;
    }

    size_t read_len = 0;
    size_t read_size = file_len;

    if (MT_TYPE(entry->mem_attr) != MT_DEV)
    {
        for (i = 0; i < entry->region_page_count; i++)
        {
            irq_flags_t irq_flags;
            virt_addr_t kernel_addr =
                ktmp_access_start(entry->physical_address + (i << PAGE_SIZE_SHIFT));

            if (need_zero)
            {
                /* 清空对应页 */
                memset((void *)(kernel_addr), 0, PAGE_SIZE);
            }

            if (((flags & MAP_ANONYMOUS) == 0) && need_zero)
            {
                if (read_size > read_len)
                {
                    size_t read_once_len =
                        ((read_size - read_len) > PAGE_SIZE) ? PAGE_SIZE : (read_size - read_len);
                    uint8_t *buf = (uint8_t *)kernel_addr;
                    if (off_in_page > 0 && i == 0)
                    {
                        buf += off_in_page;
                        if (read_once_len > (PAGE_SIZE - off_in_page))
                        {
                            read_once_len = PAGE_SIZE - off_in_page;
                        }
                    }
                    int r = file_read(f, buf, read_once_len);
                    off += r;
                    read_len += r;
                    if (r < 0)
                    {
                        ktmp_access_end(kernel_addr);
                        goto error_out;
                    }
                    /* 文件读完了 */
                    if (r < read_once_len)
                    {
                        read_len = read_size;
                    }
                }
            }
            /* 用户态和内核态使用的是不同的地址去访问此段内存所以需要刷新cache
             */
            cache_dcache_flush(kernel_addr, PAGE_SIZE);
            ktmp_access_end(kernel_addr);
        }
    }

    ret = mm_region_map(get_process_mm(pcb), entry);
    if (ret < 0)
    {
        goto error_out;
    }
    *addr = entry->virtual_address + off_in_page;

    if (!(MT_NO_ACCESS & entry->mem_attr))
    {
        if (!(MT_EXECUTE_NEVER & entry->mem_attr))
        {
            cache_text_update(entry->virtual_address, entry->region_page_count << PAGE_SIZE_SHIFT);
        }
    }

    if (f != NULL)
    {
        /* 设置文件位置 */
        ret = file_seek(f, file_pos, SEEK_SET);
        if (ret < 0)
        {
            goto error_out;
        }
    }
    return ret;
error_out:
    if (entry->is_page_free)
    {
        pages_free(entry->physical_address, entry->page_order);
    }
    if (entry->munmap)
    {
        entry->munmap(entry);
    }
    free(entry);
    if (f != NULL)
    {
        /* 设置文件位置 */
        int ret2 = file_seek(f, file_pos, SEEK_SET);
        if (ret2 < 0)
        {
            return ret2;
        }
    }
    return ret;
}

int process_mremap(virt_addr_t addr, unsigned long old_len, unsigned long new_len,
                   unsigned long flags, virt_addr_t *new_addr)
{
    struct mm *mm = get_process_mm(ttosProcessSelf());
    struct mm_region *oldregion;
    bool need_move = false;

    /* 由于MREMAP_DONTUNMAP依赖缺页异常，实时系统不存在缺页异常 所以不支持 */
    if (flags & MREMAP_DONTUNMAP)
    {
        return -ENOSYS;
    }

    /* 检查new_addr参数合法性 */
    if (new_addr == NULL)
    {
        return -EINVAL;
    }
    /* 检查地址对齐 */
    if (addr & (PAGE_SIZE - 1))
    {
        return -EINVAL;
    }

    /* 检查参数 MREMAP_FIXED 被设置时 必须设置 MREMAP_MAYMOVE */
    if (flags & MREMAP_FIXED)
    {
        if (!(flags & MREMAP_MAYMOVE))
        {
            return -EINVAL;
        }
        /* 检查地址对齐 */
        if (*new_addr & (PAGE_SIZE - 1))
        {
            return -EINVAL;
        }
        need_move = true;
    }
    else
    {
        /* 如果 旧的大小等于新的大小则无需做处理 直接返回 */
        if (old_len == new_len)
        {
            *new_addr = addr;
            return 0;
        }
    }

    /* 查找旧的区域 */
    oldregion = mm_va2region(mm, addr);
    if (oldregion == NULL)
    {
        return -EFAULT;
    }

    /* 如果 old_len 的值为零，并且 addr 指向一个可共享的映射，则 mremap()
    将创建一个相同页面的新映射。 new_size 将是新映射的大小，并且新映射的位置可以通过 new_address
    来指定； 请参阅下面的 MREMAP_FIXED 描述。如果通过这种方法请求新的映射，则必须同时指定
    MREMAP_MAYMOVE 标志。*/
    if (old_len == 0 && (oldregion->flags & MAP_SHARED))
    {
        if (!(flags & MREMAP_MAYMOVE))
        {
            return -EINVAL;
        }
        struct mm_region *region_modify = malloc(sizeof(struct mm_region));
        if (region_modify == NULL)
        {
            return -ENOMEM;
        }
        memcpy(region_modify, oldregion, sizeof(struct mm_region));

        region_modify->flags = oldregion->flags & (~MAP_FIXED);
        if (flags & MREMAP_FIXED)
        {
            region_modify->flags |= MAP_FIXED;
            region_modify->map_min = region_modify->virtual_address = *new_addr;
        }
        else
        {
            region_modify->map_min = region_modify->virtual_address = 0;
        }

        region_modify->physical_address =
            oldregion->physical_address + (addr - oldregion->virtual_address);
        region_modify->region_page_count = new_len >> PAGE_SIZE_SHIFT;
        region_modify->is_need_free = true;
        if (region_modify->is_page_free)
        {
            /* 增加引用计数 */
            page_ref_inc(region_modify->physical_address, region_modify->page_order);
        }
        if (region_modify->flags & MAP_IS_FILE)
        {
            region_modify->filepath = strdup(region_modify->filepath);
        }
        mm_region_map(mm, region_modify);
    }

    /* 缩小内存 */
    if (new_len < old_len)
    {
        /* 强制移动内存 */
        if (need_move)
        {
            struct mm_region *region_modify = malloc(sizeof(struct mm_region));
            if (region_modify == NULL)
            {
                return -ENOMEM;
            }
            memcpy(region_modify, oldregion, sizeof(struct mm_region));
            region_modify->virtual_address = *new_addr;
            region_modify->physical_address =
                oldregion->physical_address + (addr - oldregion->virtual_address);
            region_modify->region_page_count = new_len >> PAGE_SIZE_SHIFT;
            region_modify->is_need_free = true;
            if (region_modify->is_page_free)
            {
                /* 增加引用计数 */
                page_ref_inc(region_modify->physical_address, region_modify->page_order);
            }
            if (region_modify->flags & MAP_IS_FILE)
            {
                region_modify->filepath = strdup(region_modify->filepath);
            }
            mm_region_map(mm, region_modify);

            *new_addr = region_modify->virtual_address;
            /* 解除旧区域的映射 */
            struct mm_region unmap_region;

            mm_region_init(&unmap_region, 0, 0, old_len);
            unmap_region.virtual_address = addr;
            mm_region_modify(mm, &unmap_region, true);
        }
        else
        {
            /* 解除多余区域的映射 */
            struct mm_region unmap_region;

            mm_region_init(&unmap_region, 0, 0, old_len - new_len);
            unmap_region.virtual_address = addr + new_len;
            mm_region_modify(mm, &unmap_region, true);

            *new_addr = addr;
        }
        return 0;
    }

    /* 扩展内存 */
    if (!need_move)
    {
        struct mm_region *region_modify = mm_va2region(mm, addr + new_len - 1);
        /* 扩展区域已映射 受阻 */
        if (region_modify != NULL)
        {
            /* 如果扩展区域与旧区域在同一区域则表示已映射 直接返回成功 */
            if (region_modify == oldregion)
            {
                *new_addr = addr;
                return 0;
            }
            /* 如果允许移动区域 则进行区域移动 */
            else if (flags & MREMAP_MAYMOVE)
            {
                need_move = true;
            }
            /* 否则返回无空间 */
            else
            {
                return -ENOMEM;
            }
        }
        else
        {
            /* 不需要移动 直接扩展 */
            struct mm_region *region_modify = malloc(sizeof(struct mm_region));
            if (region_modify == NULL)
            {
                return -ENOMEM;
            }
            memcpy(region_modify, oldregion, sizeof(struct mm_region));
            region_modify->region_page_count = new_len >> PAGE_SIZE_SHIFT;
            region_modify->flags |= MAP_FIXED;
            region_modify->page_order = page_bits(new_len);
            region_modify->physical_address = pages_alloc(region_modify->page_order, ZONE_ANYWHERE);
            if (region_modify->physical_address == 0)
            {
                free(region_modify);
                return -ENOMEM;
            }
            region_modify->is_page_free = true;
            region_modify->is_need_free = true;
            for (int i = 0; i < (old_len >> PAGE_SIZE_SHIFT); i++)
            {
                irq_flags_t irq_flags;
                virt_addr_t kernel_addr =
                    ktmp_access_start(region_modify->physical_address + (i << PAGE_SIZE_SHIFT));

                memcpy((void *)kernel_addr, (const void *)addr + (i << PAGE_SIZE_SHIFT), PAGE_SIZE);

                /* 用户态和内核态使用的是不同的地址去访问此段内存所以需要刷新cache
                 */
                cache_dcache_flush(kernel_addr, PAGE_SIZE);
                ktmp_access_end(kernel_addr);
            }
            /* 解除旧区域的映射 */
            struct mm_region unmap_region;

            mm_region_init(&unmap_region, 0, 0, old_len);
            unmap_region.virtual_address = addr;
            mm_region_modify(mm, &unmap_region, true);

            mm_region_map(mm, region_modify);
            *new_addr = region_modify->virtual_address;
            return 0;
        }
    }

    if (need_move)
    {
        unsigned long mapflags = oldregion->flags & (~MAP_FIXED);
        if (flags & MREMAP_FIXED)
        {
            mapflags |= MAP_FIXED;
        }
        else
        {
            *new_addr = 0;
        }
        int ret = process_mmap(ttosProcessSelf(), new_addr, new_len,
                               process_attr_to_prot(oldregion->mem_attr), mapflags, NULL, 0, 0);
        if (ret < 0)
        {
            return ret;
        }
        if (*new_addr)
        {
            memcpy((void *)*new_addr, (void *)addr, new_len);
        }

        /* 解除旧区域的映射 */
        struct mm_region unmap_region;

        mm_region_init(&unmap_region, 0, 0, old_len);
        unmap_region.virtual_address = addr;
        mm_region_modify(mm, &unmap_region, true);

        return 0;
    }
    return 0;
}
