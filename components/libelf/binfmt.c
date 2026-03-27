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

#include "ttosMM.h"
#include <cache.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <ttosProcess.h>
#include <ttosUtils.h>
#include <unistd.h>

#undef KLOG_TAG
#define KLOG_TAG "elf"
#include <klog.h>

#define HWCAP_TLS (1 << 15)

#if ULONG_MAX == 0xffffffffUL
typedef Elf32_Sym Sym;
typedef Elf32_Ehdr Ehdr;
typedef Elf32_Phdr Phdr;
typedef Elf32_Shdr Shdr;
typedef Elf32_auxv_t aux_t;
#else
typedef Elf64_Ehdr Ehdr;
typedef Elf64_Phdr Phdr;
typedef Elf64_Shdr Shdr;
typedef Elf64_auxv_t aux_t;
#endif

struct elf_info
{
    struct file *f;
    Ehdr ehdr;
    Phdr *phdr;
    Shdr *shdr;
    char *interp_name;
    bool stack_can_exec;
};

struct load_info
{
    pcb_t pcb;
    struct elf_info elf_info;
    struct elf_info interp_info;
    int argc, envc;
    virt_addr_t interp_base;
    virt_addr_t exec_base;

    char **argv;
    char **envv;
};

#define ISELF32(elfFile) (((Ehdr *)elfFile)->e_ident[EI_CLASS] == ELFCLASS32)
#define ISELF64(elfFile) (((Ehdr *)elfFile)->e_ident[EI_CLASS] == ELFCLASS64)

static int get_strings_count(const char *argv[])
{
    int len;
    for (len = 0; argv[len] != NULL; len++)
        ;
    return len;
}

#define AUX_VALUE(type, v)                                                                         \
    (aux_t)                                                                                        \
    {                                                                                              \
        .a_type = (type), .a_un.a_val = (typeof((aux_t){}.a_un.a_val))(v)                          \
    }

static virt_addr_t copy_string(virt_addr_t addr, const char *string)
{
    strcpy((char *)addr, string);
    return addr + strlen(string) + 1;
}

static virt_addr_t copy_phdr(virt_addr_t addr, struct elf_info *info)
{
    size_t copy_size = info->ehdr.e_phnum * sizeof(Phdr);
    memcpy((void *)addr, info->phdr, copy_size);
    return copy_size + addr;
}

#define USER_STACK_PAGE_COUNT (32)
#define AUX_COUNT 52

static size_t get_env_size(struct load_info *info)
{
    size_t total_size = 0;
    /* argv表 */
    total_size += (info->argc + 1) * sizeof(long);
    /* env表 */
    total_size += (info->envc + 1) * sizeof(long);
    total_size += AUX_COUNT * sizeof(aux_t);
    /* argv字符串 */
    for (int i = 0; i < info->argc; i++)
    {
        total_size += strlen(info->argv[i]) + 1;
    }
    /* envv字符串 */
    for (int i = 0; i < info->envc; i++)
    {
        total_size += strlen(info->envv[i]) + 1;
    }
    /* Phdr */
    total_size += info->elf_info.ehdr.e_phnum * sizeof(Phdr);

    /* platform 字符串 */
    total_size += strlen(CONFIG_ARCH) + 1;
    total_size += sizeof(long);

    /* random */
    total_size += 16;

    return total_size;
}

static virt_addr_t get_phdr_addr(struct elf_info *info)
{
    for (int i = 0; i < info->ehdr.e_phnum; i++)
    {
        if (info->phdr[i].p_type == PT_PHDR)
        {
            return info->phdr[i].p_vaddr;
        }
    }
    return 0;
}
#include <assert.h>
extern pfn_t max_low_pfn;
int print_inliner_map(void);
extern virt_addr_t vdso_map(pcb_t pcb) __attribute__((weak));
static void build_envp(struct load_info *info)
{
    uintptr_t map_start;
    uintptr_t kernel_map_start;
    phys_addr_t paddr;
    struct mm_region region;
    virt_addr_t copy_addr;
    size_t total_size;
    virt_addr_t phdr_addr;

    struct mm_region kregion;

    char **argv;
    char **envv;
    aux_t *aux;
    aux_t *aux_start = NULL;
    virt_addr_t vdso_ehdr = 0;

    total_size = get_env_size(info);

    map_start = USER_SPACE_END + 1 -
                (PAGE_SIZE * USER_STACK_PAGE_COUNT + PAGE_SIZE + PAGE_ALIGN(total_size));
    paddr = process_mm_map(info->pcb, 0, (virt_addr_t *)&map_start,
                           (info->elf_info.stack_can_exec ? MT_RWX_DATA : MT_RW_DATA) | MT_USER,
                           USER_STACK_PAGE_COUNT + 1 + PAGE_ALIGN(total_size) / ttosGetPageSize(),
                           MAP_IS_STACK);
    /* 设置用户栈保护 */
    mm_region_init(&region, MT_NACCESS | MT_USER, mm_v2p(get_process_mm(info->pcb), map_start),
                   PAGE_SIZE);
    region.virtual_address = map_start;
    mm_region_modify(get_process_mm(info->pcb), &region, false);

    paddr += PAGE_SIZE;
    map_start += PAGE_SIZE;

    kernel_map_start = map_start;

    // kernel_map_start = page_address(paddr);

    // /* 高位地址 */
    // if (kernel_map_start == 0)
    // {
    //     mm_region_init(&kregion, MT_KERNEL_MEM, paddr,
    //                    (USER_STACK_PAGE_COUNT)*PAGE_SIZE + PAGE_ALIGN(total_size));
    //     mm_region_map(get_kernel_mm(), &kregion);
    //     kernel_map_start = kregion.virtual_address;
    // }

    memset((void *)kernel_map_start, 0, USER_STACK_PAGE_COUNT * ttosGetPageSize());

    /* map_start 向上为ENVP 向下为栈空间 */
    kernel_map_start += USER_STACK_PAGE_COUNT * ttosGetPageSize();
    map_start += USER_STACK_PAGE_COUNT * ttosGetPageSize();

    info->pcb->userStack = (void *)map_start;
    info->pcb->args = (void *)map_start;

    *(long *)kernel_map_start = info->argc;

    argv = (void *)(kernel_map_start + sizeof(long));
    envv = argv + info->argc + 1;
    aux = (void *)(envv + info->envc + 1);
    aux_start = aux;

    info->pcb->auxvp = mm_kernel_v2p((virt_addr_t)aux);

    if (vdso_map)
    {
        vdso_ehdr = vdso_map(info->pcb);
    }

    copy_addr = (virt_addr_t)(aux + AUX_COUNT);

    /* *aux++ =AUX_VALUE(AT_IGNORE, 0); */
    /* *aux++ =AUX_VALUE(AT_EXECFD, 0); */

    phdr_addr = get_phdr_addr(&info->elf_info);
    if (phdr_addr != 0)
    {
        /* PH表指针 */
        *aux++ = AUX_VALUE(AT_PHDR, phdr_addr + info->exec_base);
    }
    else
    {
        /* PH表指针 */
        *aux++ = AUX_VALUE(AT_PHDR, (copy_addr - kernel_map_start) + map_start);
        copy_addr = copy_phdr(copy_addr, &info->elf_info);
    }
    *aux++ = AUX_VALUE(AT_PLATFORM, (copy_addr - kernel_map_start) + map_start);
    copy_addr = copy_string(copy_addr, CONFIG_ARCH);

    for (int i = 0; i < info->argc; i++)
    {
        argv[i] = (void *)((copy_addr - kernel_map_start) + map_start);
        copy_addr = copy_string(copy_addr, info->argv[i]);
    }
    argv[info->argc] = NULL;

    for (int i = 0; i < info->envc; i++)
    {
        envv[i] = (void *)((copy_addr - kernel_map_start) + map_start);
        copy_addr = copy_string(copy_addr, info->envv[i]);
    }
    envv[info->envc] = NULL;

    *aux++ = AUX_VALUE(AT_PHENT, sizeof(Phdr));
    *aux++ = AUX_VALUE(AT_PHNUM, info->elf_info.ehdr.e_phnum);
    *aux++ = AUX_VALUE(AT_PAGESZ, ttosGetPageSize());
    *aux++ = AUX_VALUE(AT_MINSIGSTKSZ, TTOS_MINIMUM_STACK_SIZE); /* minimal stack size */
    *aux++ = AUX_VALUE(AT_SECURE, 0);                            /* 设置为0 */
    struct file f;
    int ret = file_open(&f, "/dev/urandom", O_RDONLY);
    if (ret == 0)
    {
        file_read(&f, (void *)copy_addr, 16);

        file_close(&f);

        *aux++ = AUX_VALUE(AT_RANDOM, (copy_addr - kernel_map_start) + map_start);
    }

    *aux++ = AUX_VALUE(AT_EXECFN, argv[0]); /* 执行文件字符串的路径（字符串指针）
                                             */
    *aux++ = AUX_VALUE(AT_BASE, info->interp_base);
    *aux++ = AUX_VALUE(AT_FLAGS, 0);
    *aux++ = AUX_VALUE(AT_ENTRY, (uintptr_t)info->elf_info.ehdr.e_entry + info->exec_base);
    /* *aux++ =AUX_VALUE(AT_NOTELF, 0); */
    *aux++ = AUX_VALUE(AT_UID, 0);
    *aux++ = AUX_VALUE(AT_EUID, 0);
    *aux++ = AUX_VALUE(AT_GID, 0);
    *aux++ = AUX_VALUE(AT_EGID, 0);

    /* *aux++ =AUX_VALUE(AT_CLKTCK, 0); */
    /* musl 必须支持TLS */
    *aux++ = AUX_VALUE(AT_HWCAP, HWCAP_TLS);
    /* *aux++ =AUX_VALUE(AT_FPUCW, 0); */
    /* *aux++ =AUX_VALUE(AT_DCACHEBSIZE, 0);
     *aux++ =AUX_VALUE(AT_ICACHEBSIZE, 0);
     *aux++ =AUX_VALUE(AT_UCACHEBSIZE, 0); */
    /* *aux++ =AUX_VALUE(AT_IGNOREPPC, 0); */
    /* *aux++ =AUX_VALUE(AT_BASE_PLATFORM, 0); */

    *aux++ = AUX_VALUE(AT_HWCAP2, 0);

    *aux++ = AUX_VALUE(AT_SYSINFO, 0);              /* vsyscall entry for 32bit arch*/
    *aux++ = AUX_VALUE(AT_SYSINFO_EHDR, vdso_ehdr); /* vDSO */

    /* *aux++ =AUX_VALUE(AT_L1I_CACHESHAPE, 0);
     *aux++ =AUX_VALUE(AT_L1D_CACHESHAPE, 0);
     *aux++ =AUX_VALUE(AT_L2_CACHESHAPE, 0);
     *aux++ =AUX_VALUE(AT_L3_CACHESHAPE, 0); */
    *aux++ = AUX_VALUE(AT_NULL, 0x666);

    info->pcb->aux_len = (aux - aux_start) * sizeof(aux_t);

    /* 用户态和内核态使用的是不同的地址去访问此段内存所以需要刷新cache
     */
    // cache_dcache_flush(kernel_map_start, total_size);

    // if (!page_address(paddr))
    // {
    //     mm_region_unmap(get_kernel_mm(), &kregion);
    // }
}

static int elf_load_ehdr(struct elf_info *info)
{
    size_t read_len;
    /* 读取ELF头 */
    file_seek(info->f, 0, SEEK_SET);
    read_len = file_read(info->f, &info->ehdr, sizeof info->ehdr);
    if (read_len != sizeof info->ehdr)
    {
        return -ENOEXEC;
    }

    /* 检查ELF头是否有效 */
    if (info->ehdr.e_ident[EI_MAG0] != ELFMAG0 || info->ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        info->ehdr.e_ident[EI_MAG2] != ELFMAG2 || info->ehdr.e_ident[EI_MAG3] != ELFMAG3)
    {
        return -ENOEXEC; /* not an elf file */
    }

    /* 检查ELF文件是否是对应位数的elf文件 */
#if ULONG_MAX == 0xffffffffUL
    if (info->ehdr.e_ident[EI_CLASS] != ELFCLASS32)
    {
        KLOG_E("not a 32bits elf filed!");
        return -ENOEXEC;
    }
#else
    if (info->ehdr.e_ident[EI_CLASS] != ELFCLASS64)
    {
        KLOG_E("not a 64bits elf filed!");
        return -ENOEXEC;
    }
#endif
    /* 检查ELF版本 应当为1 */
    if (info->ehdr.e_ident[EI_VERSION] != 1)
    { /* ver not 1 */
        KLOG_E("elf Version not 1,ver:%d!", info->ehdr.e_ident[EI_VERSION]);
        return -ENOEXEC;
    }
    return 0;
}

static int elf_load_phdr(struct elf_info *info)
{
    size_t read_len;
    size_t load_size = info->ehdr.e_phnum * sizeof(*info->phdr);
    info->phdr = malloc(load_size);
    if (info->phdr == NULL)
    {
        return -ENOMEM;
    }
    file_seek(info->f, info->ehdr.e_phoff, SEEK_SET);
    read_len = file_read(info->f, info->phdr, load_size);
    if (read_len != load_size)
    {
        return -ENOEXEC;
    }
    return 0;
}
static int elf_load_shdr(struct elf_info *info)
{
    size_t read_len;
    size_t shdr_num = info->ehdr.e_shnum;
    size_t load_size = shdr_num * info->ehdr.e_shentsize;

    if (info->ehdr.e_shnum < 1)
    {
        KLOG_E("No sections(?)\n");
        return -EINVAL;
    }

    info->shdr = malloc(load_size);
    if (!info->shdr)
    {
        return -ENOMEM;
    }

    /* 一次读取所有section信息到info->shdr */
    file_seek(info->f, info->ehdr.e_shoff, SEEK_SET);
    read_len = file_read(info->f, info->shdr, load_size);
    if (read_len != load_size)
    {
        return -ENOEXEC;
    }

    return 0;
}

static int elf_set_stack_exec(struct elf_info *info)
{
    info->stack_can_exec = true;
    return 0;
}

static int elf_load_interp_name(struct elf_info *info)
{
    size_t read_len;
    for (int i = 0; i < info->ehdr.e_phnum; i++)
    {
        if (info->phdr[i].p_type == PT_INTERP)
        {
            info->interp_name = malloc(info->phdr[i].p_filesz);
            if (info->interp_name == NULL)
            {
                return -ENOMEM;
            }
            file_seek(info->f, info->phdr[i].p_offset, SEEK_SET);
            read_len = file_read(info->f, info->interp_name, info->phdr[i].p_filesz);
            if (read_len != info->phdr[i].p_filesz)
            {
                KLOG_E("file size error!");
                return -ENOEXEC;
            }
            break;
        }
    }
    return 0;
}

static int elf_map_file(struct elf_info *info, pcb_t pcb, virt_addr_t map_addr)
{
    uintptr_t map_vstart;
    uintptr_t attr;
    int flags, i;
    int ret;

    if (map_addr & PAGE_SIZE_MASK)
    {
        return -EINVAL;
    }

    for (i = 0; i < info->ehdr.e_phnum; i++)
    {
        if (info->phdr[i].p_type == PT_LOAD)
        {
            /* 由于MMU只能页对齐分配，所以这里将需要map的起始和结束地址
             * 按页向下对齐 */
            map_vstart = info->phdr[i].p_vaddr;
            map_vstart += map_addr;

            /* 用户态映射 */
            attr = PROT_READ;

            /* 判断读写属性 */
            if (info->phdr[i].p_flags & PF_W)
            {
                attr |= PROT_WRITE;
            }

            /* 判断执行属性 */
            if (info->phdr[i].p_flags & PF_X)
            {
                attr |= PROT_EXEC;
            }
            flags = MAP_FIXED | MAP_DENYWRITE | MAP_PRIVATE;

            ret = process_mmap(pcb, (unsigned long *)&map_vstart, info->phdr[i].p_memsz, attr,
                               flags, info->f, info->phdr[i].p_offset, info->phdr[i].p_filesz);
            if (ret < 0)
            {
                return ret;
            }
        }
    }
    return 0;
}

int check_exec(const char *path)
{
    struct load_info load_info;
    struct file f;
    int ret;
    if (access(path, F_OK) != 0)
    {
        return -ENOENT;
    }
    if (access(path, X_OK) != 0)
    {
        return -EACCES;
    }
    ret = file_open(&f, path, O_RDONLY);
    if (ret < 0)
    {
        return ret;
    }
    load_info.elf_info.f = &f;
    ret = elf_load_ehdr(&load_info.elf_info);
    if (ret < 0)
    {
        file_close(load_info.elf_info.f);
        return ret;
    }
    if (load_info.elf_info.ehdr.e_type != ET_EXEC && load_info.elf_info.ehdr.e_type != ET_DYN)
    {
        file_close(load_info.elf_info.f);
        return -ENOEXEC;
    }
    file_close(load_info.elf_info.f);
    return 0;
}

int load_elf(struct file *f, pcb_t pcb, const char *argv[], const char *envp[])
{
    int ret = 0;

    struct load_info load_info;
    struct file f_inte;

    memset(&load_info, 0, sizeof(load_info));

    load_info.pcb = pcb;
    load_info.elf_info.f = f;
    load_info.interp_info.f = NULL;

    ret = elf_load_ehdr(&load_info.elf_info);
    if (ret < 0)
    {
        goto free_out;
    }

    /* 检查ELF是否为动态链接库或可执行文件 */
    if ((load_info.elf_info.ehdr.e_type != ET_DYN) && (load_info.elf_info.ehdr.e_type != ET_EXEC))
    {
        KLOG_E("elf type not pie or exec, type:%d!", load_info.elf_info.ehdr.e_type);
        ret = -ENOEXEC;
        goto free_out;
    }

    ret = elf_load_phdr(&load_info.elf_info);
    if (ret < 0)
    {
        goto free_out;
    }

    ret = elf_load_shdr(&load_info.elf_info);
    if (ret < 0)
    {
        goto free_out;
    }
    elf_set_stack_exec(&load_info.elf_info);

    ret = elf_load_interp_name(&load_info.elf_info);
    if (ret < 0)
    {
        goto free_out;
    }

    /* 存在解释器 */
    if (load_info.elf_info.interp_name != NULL)
    {
        /* 加载解释器 */
        ret = file_open(&f_inte, load_info.elf_info.interp_name, O_RDONLY);
        if (ret < 0)
        {
            printk("no interp[%s]: %s\n", load_info.elf_info.interp_name, strerror(-ret));
            ret = -errno;
            goto free_out;
        }
        load_info.interp_info.f = &f_inte;
    }

    virt_addr_t exec_addr = EXEC_LOAD_VADDR;

    // if(!(ADDR_NO_RANDOMIZE & pcb->personality) && load_info.elf_info.ehdr.e_type == ET_DYN)
    // {
    //     exec_addr += (rand() % 100) << PAGE_SIZE_SHIFT;
    // }

    ret = elf_map_file(&load_info.elf_info, pcb, exec_addr);
    if (ret < 0)
    {
        goto free_out;
    }

    pcb->entry = (int (*)(void *))load_info.elf_info.ehdr.e_entry + exec_addr;
    load_info.exec_base = exec_addr;

    if (load_info.interp_info.f != NULL)
    {
        ret = elf_load_ehdr(&load_info.interp_info);
        if (ret < 0)
        {
            goto free_out;
        }
        /* 检查ELF是否为动态链接库 */
        if ((load_info.interp_info.ehdr.e_type != ET_DYN))
        {
            KLOG_E("elf type not dyn, type:%d!", load_info.interp_info.ehdr.e_type);
            ret = -ENOEXEC;
            goto free_out;
        }

        ret = elf_load_phdr(&load_info.interp_info);
        if (ret < 0)
        {
            goto free_out;
        }
        virt_addr_t ldso_addr = LDSO_LOAD_VADDR;
        if (!(ADDR_NO_RANDOMIZE & pcb->personality))
        {
            ldso_addr += (rand() % 100) << PAGE_SIZE_SHIFT;
        }
        ret = elf_map_file(&load_info.interp_info, pcb, ldso_addr);
        if (ret < 0)
        {
            goto free_out;
        }
        pcb->entry = (int (*)(void *))load_info.interp_info.ehdr.e_entry + ldso_addr;
        load_info.interp_base = ldso_addr;
        file_close(load_info.interp_info.f);
        load_info.interp_info.f = NULL;
    }
    else
    {
        load_info.interp_base = load_info.exec_base;
    }

    /* 构建 启动环境 */
    load_info.argc = get_strings_count(argv);
    load_info.envc = get_strings_count(envp);
    load_info.argv = (char **)argv;
    load_info.envv = (char **)envp;

    /* 构建启动参数 */
    build_envp(&load_info);

    struct mm *mm = get_process_mm(pcb);

    mm->start_data_segment = mm->end_data_segment = DATA_SIGMENT_VADDR;

free_out:
    /* 释放 info资源 */
    if (load_info.elf_info.phdr)
    {
        free(load_info.elf_info.phdr);
    }
    if (load_info.elf_info.shdr)
    {
        free(load_info.elf_info.shdr);
    }
    if (load_info.elf_info.interp_name)
    {
        free(load_info.elf_info.interp_name);
    }

    if (load_info.interp_info.f != NULL)
    {
        file_close(load_info.interp_info.f);
    }

    if (load_info.interp_info.phdr)
    {
        free(load_info.interp_info.phdr);
    }

    return ret;
}
