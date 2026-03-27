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

#include "sys/mman.h"
#include <errno.h>
#include <page.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sysv/shm.h>
#include <time/ktime.h>
#include <ttosBase.h>
#include <ttosMM.h>
#include <ttosProcess.h>
#include <ttosRBTree.h>

struct shm_tree
{
    struct rb_root root;
    MUTEX_ID mutex;
};

struct shm_node
{
    struct rb_node node;
    uint64_t key;
    int page_order;
    phys_addr_t addr;
    int shmid;
    bool deleted;
    struct shmid_ds ds;
};

struct shm_tree g_shm_tree = {.root = RB_TREE_ROOT, .mutex = NULL};

static inline void shm_lock()
{
    if (unlikely(g_shm_tree.mutex == NULL))
    {
        TTOS_CreateMutex(1, 0, &g_shm_tree.mutex);
    }
    TTOS_ObtainMutex(g_shm_tree.mutex, TTOS_MUTEX_WAIT_FOREVER);
}

static inline void shm_unlock()
{
    TTOS_ReleaseMutex(g_shm_tree.mutex);
}

static struct shm_node *_shm_search(uint64_t key)
{
    struct rb_node *node = g_shm_tree.root.rb_node;
    shm_lock();
    while (node)
    {
        struct shm_node *shm = rb_entry(node, struct shm_node, node);
        if (key < shm->key)
        {
            node = node->rb_left;
        }
        else if (key > shm->key)
        {
            node = node->rb_right;
        }
        else
        {
            shm_unlock();
            return shm;
        }
    }
    shm_unlock();
    return NULL;
}

static struct shm_node *_shm_create(uint64_t key, size_t size, mode_t mode)
{
    struct shm_node *shm = calloc(1, sizeof(struct shm_node));
    if (shm == NULL)
    {
        return NULL;
    }
    shm->key = key;
    shm->ds.shm_segsz = PAGE_ALIGN(size);
    ;
    shm->ds.shm_perm.mode = mode;
    shm->shmid = shm->key & UINT32_MAX;
    shm->page_order = page_bits(shm->ds.shm_segsz);
    shm->addr = pages_alloc(shm->page_order, ZONE_ANYWHERE);
    if (shm->addr == 0)
    {
        free(shm);
        return NULL;
    }
    int page_count = shm->ds.shm_segsz >> PAGE_SIZE_SHIFT;

    for (int i = 0; i < page_count; i++)
    {
        virt_addr_t vaddr = ktmp_access_start(shm->addr);
        memset((void *)vaddr, 0, PAGE_SIZE);
        ktmp_access_end(vaddr);
    }
    return shm;
}

static int _shm_resize(struct shm_node *shm, size_t size)
{
    size = PAGE_ALIGN(size);
    if (shm->ds.shm_segsz == size)
    {
        return 0;
    }
    if (shm->ds.shm_segsz > size)
    {
        shm->ds.shm_segsz = size;
        return 0;
    }
    if (shm->ds.shm_segsz < size)
    {
        int page_order = page_bits(size);
        phys_addr_t addr = pages_alloc(page_order, ZONE_ANYWHERE);
        if (addr == 0)
        {
            return -ENOMEM;
        }
        int old_page_count = PAGE_ALIGN(shm->ds.shm_segsz) >> PAGE_SIZE_SHIFT;
        int page_count = PAGE_ALIGN(size) >> PAGE_SIZE_SHIFT;
        for (int i = 0; i < old_page_count; i++)
        {
            virt_addr_t vaddr = ktmp_access_start(addr);
            virt_addr_t old_vaddr = ktmp_access_start(shm->addr);
            memcpy((void *)vaddr, (void *)old_vaddr, PAGE_SIZE);
            ktmp_access_end(old_vaddr);
            ktmp_access_end(vaddr);
        }
        for (int i = old_page_count; i < page_count; i++)
        {
            virt_addr_t vaddr = ktmp_access_start(addr);
            memset((void *)vaddr, 0, PAGE_SIZE);
            ktmp_access_end(vaddr);
        }
        pages_free(shm->addr, shm->page_order);
        shm->addr = addr;
        shm->page_order = page_order;
        shm->ds.shm_segsz = size;
        return 0;
    }
    return -EINVAL;
}

static int _shm_insert(struct shm_node *shm)
{
    struct rb_node **new = &(g_shm_tree.root.rb_node), *parent = NULL;
    shm_lock();
    while (*new)
    {
        struct shm_node *this = rb_entry(*new, struct shm_node, node);
        parent = *new;
        if (shm->key < this->key)
        {
            new = &((*new)->rb_left);
        }
        else if (shm->key > this->key)
        {
            new = &((*new)->rb_right);
        }
        else
        {
            shm_unlock();
            return -1;
        }
    }
    rb_link_node(&shm->node, parent, new);
    rb_insert_color(&shm->node, &g_shm_tree.root);
    shm_unlock();
    return 0;
}

static int _shm_destroy(struct shm_node *shm)
{
    if (shm->ds.shm_nattch == 0)
    {
        pages_free(shm->addr, shm->page_order);
        shm_lock();
        rb_erase(&shm->node, &g_shm_tree.root);
        shm_unlock();
        free(shm);
        return 0;
    }
    else
    {
        shm->deleted = true;
    }
    return -1;
}

static uint64_t get_private_key()
{
    return (uint64_t)ttosProcessSelf()->pgid << 32;
}

int sysv_shmget(key_t key, size_t size, int shmflg)
{
    uint64_t key64 = key;
    if (key64 == IPC_PRIVATE)
    {
        key64 = get_private_key();
    }
    struct shm_node *shm = _shm_search(key);
    if (size == 0)
    {
        return -EINVAL;
    }
    if (IPC_CREAT & shmflg)
    {
        if (shm != NULL)
        {
            if (IPC_EXCL & shmflg)
            {
                return -EEXIST;
            }
            if (shm->ds.shm_segsz < size)
            {
                return -EINVAL;
            }
            shm->ds.shm_atime = kernel_time();
            return shm->shmid;
        }
        else
        {
            shm = _shm_create(key64, size, shmflg & 0777);
            if (shm == NULL)
            {
                return -ENOMEM;
            }
            if (_shm_insert(shm) != 0)
            {
                pages_free(shm->addr, shm->page_order);
                free(shm);
                return -1;
            }
            shm->ds.shm_ctime = kernel_time();
            shm->ds.shm_lpid = get_process_pid(ttosProcessSelf());
            shm->ds.shm_cpid = shm->ds.shm_lpid;
        }
    }
    else if (shm == NULL)
    {
        return -ENOENT;
    }

    if (shm->ds.shm_segsz < size)
    {
        return -EINVAL;
    }
    return shm->shmid;
}

int sysv_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    uint64_t key64 = shmid;
    if (key64 == IPC_PRIVATE)
    {
        key64 = get_private_key();
    }
    struct shm_node *shm = _shm_search(key64);
    if (shm == NULL)
    {
        return -ENOENT;
    }
    switch (cmd)
    {
    case IPC_RMID:
        return _shm_destroy(shm);
    case IPC_SET:
        if (buf == NULL)
        {
            return -EINVAL;
        }
        shm->ds.shm_perm.mode = buf->shm_perm.mode;
        shm->ds.shm_perm.uid = buf->shm_perm.uid;
        shm->ds.shm_perm.gid = buf->shm_perm.gid;
        if (buf->shm_segsz != 0)
        {
            _shm_resize(shm, buf->shm_segsz);
        }
        shm->ds.shm_ctime = kernel_time();
        shm->ds.shm_atime = shm->ds.shm_ctime;
        shm->ds.shm_lpid = get_process_pid(ttosProcessSelf());
        return 0;
    case IPC_INFO:
        return -ENOSYS;
    default:
        return -EINVAL;
    }
}

static int shm_munmap(struct mm_region *entry)
{
    struct shm_node *shm = entry->priv.p;
    shm->ds.shm_nattch--;
    if (shm->deleted && shm->ds.shm_nattch == 0)
    {
        _shm_destroy(shm);
    }
    return 0;
}

unsigned long sysv_shmat(int shmid, virt_addr_t shmaddr, int shmflg)
{
    uint64_t key64 = shmid;
    pcb_t pcb = ttosProcessSelf();
    if (key64 == IPC_PRIVATE)
    {
        key64 = get_private_key();
    }
    struct shm_node *shm = _shm_search(key64);
    if (shm == NULL)
    {
        return -ENOENT;
    }
    shmaddr = (virt_addr_t)PAGE_ALIGN_DOWN(shmaddr);
    int flag = MAP_ANON | MAP_SHARED;
    uint64_t attr = MT_USER;

    if (shmflg & SHM_RDONLY)
    {
        attr |= MT_RO_DATA;
    }
    else
    {
        attr |= MT_RW_DATA;
    }

    if (shmflg & SHM_EXEC)
    {
        attr |= MT_EXECUTE;
    }

    if (shmaddr != 0 && !(shmflg & SHM_RND))
    {
        flag |= MAP_FIXED;
    }
    process_mm_map(pcb, shm->addr, &shmaddr, attr, shm->ds.shm_segsz >> PAGE_SIZE_SHIFT, flag);

    struct mm_region *region = mm_va2region(get_process_mm(pcb), shmaddr);

    region->priv.p = shm;
    region->munmap = shm_munmap;

    shm->ds.shm_nattch++;

    return shmaddr;
}

int sysv_shmdt(virt_addr_t shmaddr)
{
    pcb_t pcb = ttosProcessSelf();
    return process_mm_unmap(pcb, (virt_addr_t)shmaddr);
}
