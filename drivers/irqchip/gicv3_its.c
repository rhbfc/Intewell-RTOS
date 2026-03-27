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

#include <cache.h>
#include <cpuid.h>
#include <driver/devbus.h>
#include <driver/device.h>
#include <driver/driver.h>
#include <driver/gicv3.h>
#include <driver/of.h>
#include <driver/platform.h>
#include <inttypes.h>
#include <io.h>
#include <malloc.h>
#include <system/types.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>
#include <ttos_time.h>

// #define KLOG_LEVEL KLOG_WARN
#define KLOG_TAG "GICv3-ITS"
#include <klog.h>

#define ARM_GIC_CPU_NUM 1
#define GIC_LPI_INTID_START 8192

/* GITS_CTLR */
#define GITS_CTLR_Enabled (1 << 0)
#define GITS_CTLR_Quiescent (1 << 31)

/* ITS registers */
#define GITS_CTLR (0x0000)
#define GITS_IIDR (0x0004)
#define GITS_TYPER (0x0008)
#define GITS_FCTLR (0x0020)
#define GITS_CBASER (0x0080)
#define GITS_CWRITER (0x0088)
#define GITS_CREADR (0x0090)
#define GITS_BASER(n) (0x0100 + ((n)*8))
#define GITS_TRANSLATER_ADDR(hw_base) ((hw_base) + 0x10040)

/* LPI and ITS */
#define ITS_CMD_QUEUE_SIZE 1024 * 64
#define ITS_CMD_QUEUE_NR_ENTRIES (ITS_CMD_QUEUE_SIZE / sizeof(struct its_cmd_block))

#define GITS_BASER_TYPE_NONE 0
#define GITS_BASER_TYPE_DEVICE 1
#define GITS_BASER_TYPE_COLLECTION 4

/* cmd */
#define GITS_CMD_ID_MOVI 0x01
#define GITS_CMD_ID_INT 0x03
#define GITS_CMD_ID_CLEAR 0x04
#define GITS_CMD_ID_SYNC 0x05
#define GITS_CMD_ID_MAPD 0x08
#define GITS_CMD_ID_MAPC 0x09
#define GITS_CMD_ID_MAPTI 0x0a
#define GITS_CMD_ID_MAPI 0x0b
#define GITS_CMD_ID_INV 0x0c
#define GITS_CMD_ID_INVALL 0x0d
#define GITS_CMD_ID_MOVALL 0x0e
#define GITS_CMD_ID_DISCARD 0x0f

#define GITS_CMD_ID_OFFSET 0
#define GITS_CMD_ID_SHIFT 0
#define GITS_CMD_ID_MASK 0xffUL

#define GITS_CMD_DEVICEID_OFFSET 0
#define GITS_CMD_DEVICEID_SHIFT 32
#define GITS_CMD_DEVICEID_MASK 0xffffffffUL

#define GITS_CMD_SIZE_OFFSET 1
#define GITS_CMD_SIZE_SHIFT 0
#define GITS_CMD_SIZE_MASK 0x1fUL

#define GITS_CMD_EVENTID_OFFSET 1
#define GITS_CMD_EVENTID_SHIFT 0
#define GITS_CMD_EVENTID_MASK 0xffffffffUL

#define GITS_CMD_PINTID_OFFSET 1
#define GITS_CMD_PINTID_SHIFT 32
#define GITS_CMD_PINTID_MASK 0xffffffffUL

#define GITS_CMD_ICID_OFFSET 2
#define GITS_CMD_ICID_SHIFT 0
#define GITS_CMD_ICID_MASK 0xffffUL

#define GITS_CMD_ITTADDR_OFFSET 2
#define GITS_CMD_ITTADDR_SHIFT 8
#define GITS_CMD_ITTADDR_MASK 0xffffffffffUL
#define GITS_CMD_ITTADDR_ALIGN GITS_CMD_ITTADDR_SHIFT
#define GITS_CMD_ITTADDR_ALIGN_SZ (BIT(0) << GITS_CMD_ITTADDR_ALIGN)

#define GITS_CMD_RDBASE_OFFSET 2
#define GITS_CMD_RDBASE_SHIFT 16
#define GITS_CMD_RDBASE_MASK 0xffffffffUL
#define GITS_CMD_RDBASE_ALIGN GITS_CMD_RDBASE_SHIFT

#define GITS_CMD_VALID_OFFSET 2
#define GITS_CMD_VALID_SHIFT 63
#define GITS_CMD_VALID_MASK 0x1UL

/* Cache and Share ability for ITS & Redistributor LPI state tables */
#define GIC_BASER_CACHE_NGNRNE 0x0ULL     /* Device-nGnRnE */
#define GIC_BASER_CACHE_INNERLIKE 0x0ULL  /* Same as Inner Cacheability. */
#define GIC_BASER_CACHE_NCACHEABLE 0x1ULL /* Non-cacheable */
#define GIC_BASER_CACHE_RAWT 0x2ULL       /* Cacheable R-allocate, W-through */
#define GIC_BASER_CACHE_RAWB 0x3ULL       /* Cacheable R-allocate, W-back */
#define GIC_BASER_CACHE_WAWT 0x4ULL       /* Cacheable W-allocate, W-through */
#define GIC_BASER_CACHE_WAWB 0x5ULL       /* Cacheable W-allocate, W-back */
#define GIC_BASER_CACHE_RAWAWT 0x6ULL     /* Cacheable R-allocate, W-allocate, W-through */
#define GIC_BASER_CACHE_RAWAWB 0x7ULL     /* Cacheable R-allocate, W-allocate, W-back */
#define GIC_BASER_SHARE_NO 0x0ULL         /* Non-shareable */
#define GIC_BASER_SHARE_INNER 0x1ULL      /* Inner Shareable */
#define GIC_BASER_SHARE_OUTER 0x2ULL      /* Outer Shareable */

/* ITS TYPER register */
#define GITS_TYPER_PHY_SHIFT 0
#define GITS_TYPER_PHY_MASK 0x1ULL
#define GITS_TYPER_VIRT_SHIFT 1
#define GITS_TYPER_VIRT_MASK 0x1ULL
#define GITS_TYPER_ITT_ENTRY_SIZE_SHIFT 4
#define GITS_TYPER_ITT_ENTRY_SIZE_MASK 0xfULL
#define GITS_TYPER_IDBITS_SHIFT 8
#define GITS_TYPER_IDBITS_MASK 0x1fULL
#define GITS_TYPER_DEVBITS_SHIFT 13
#define GITS_TYPER_DEVBITS_MASK 0x1fULL
#define GITS_TYPER_SEIS_SHIFT 18
#define GITS_TYPER_SEIS_MASK 0x1ULL
#define GITS_TYPER_PTA_SHIFT 19
#define GITS_TYPER_PTA_MASK 0x1ULL
#define GITS_TYPER_HCC_SHIFT 24
#define GITS_TYPER_HCC_MASK 0xffULL
#define GITS_TYPER_CIDBITS_SHIFT 32
#define GITS_TYPER_CIDBITS_MASK 0xfULL
#define GITS_TYPER_CIL_SHIFT 36
#define GITS_TYPER_CIL_MASK 0x1ULL

/* ITS CBASER register */
#define GITS_CBASER_SIZE_SHIFT 0
#define GITS_CBASER_SIZE_MASK 0xffULL
#define GITS_CBASER_SHAREABILITY_SHIFT 10
#define GITS_CBASER_SHAREABILITY_MASK 0x3ULL
#define GITS_CBASER_ADDR_SHIFT 12
#define GITS_CBASER_ADDR_MASK 0xfffffffffULL
#define GITS_CBASER_OUTER_CACHE_SHIFT 53
#define GITS_CBASER_OUTER_CACHE_MASK 0x7ULL
#define GITS_CBASER_INNER_CACHE_SHIFT 59
#define GITS_CBASER_INNER_CACHE_MASK 0x7ULL
#define GITS_CBASER_VALID_SHIFT 63
#define GITS_CBASER_VALID_MASK 0x1ULL

/* ITS BASER<n> register */
#define GITS_BASER_SIZE_SHIFT 0
#define GITS_BASER_SIZE_MASK 0xffULL
#define GITS_BASER_PAGE_SIZE_SHIFT 8
#define GITS_BASER_PAGE_SIZE_MASK 0x3ULL
#define GITS_BASER_PAGE_SIZE_4K 0
#define GITS_BASER_PAGE_SIZE_16K 1
#define GITS_BASER_PAGE_SIZE_64K 2
#define GITS_BASER_SHAREABILITY_SHIFT 10
#define GITS_BASER_SHAREABILITY_MASK 0x3ULL
#define GITS_BASER_ADDR_SHIFT 12
#define GITS_BASER_ADDR_MASK 0xfffffffff
#define GITS_BASER_ENTRY_SIZE_SHIFT 48
#define GITS_BASER_ENTRY_SIZE_MASK 0x1fULL
#define GITS_BASER_OUTER_CACHE_SHIFT 53
#define GITS_BASER_OUTER_CACHE_MASK 0x7ULL
#define GITS_BASER_TYPE_SHIFT 56
#define GITS_BASER_TYPE_MASK 0x7ULL
#define GITS_BASER_INNER_CACHE_SHIFT 59
#define GITS_BASER_INNER_CACHE_MASK 0x7ULL
#define GITS_BASER_INDIRECT_SHIFT 62
#define GITS_BASER_INDIRECT_MASK 0x1ULL
#define GITS_BASER_VALID_SHIFT 63
#define GITS_BASER_VALID_MASK 0x1ULL

/* Temporarily do not make its affinity, only use Collection 0 */
#define ICID_NO 0

#define MASK(__basename) (__basename##_MASK << __basename##_SHIFT)
#define MASK_SET(__val, __basename) (((__val)&__basename##_MASK) << __basename##_SHIFT)
#define MASK_GET(__reg, __basename) (((__reg) >> __basename##_SHIFT) & __basename##_MASK)

#define ITS_CMD(_0, _1, _2, _3) ((uint64_t[4]){_0, _1, _2, _3})

struct its_cmd_block
{
    uint64_t raw_cmd[4];
};

struct gicv3_its
{
    struct ttos_pic pic;
    struct pic_gicv3 *gicv3;
    devaddr_region_t base;
    struct its_cmd_block *cmd_base;
    struct its_cmd_block *cmd_write;
    bool dev_table_is_indirect;
    uint64_t *indirect_dev_lvl1_table;
    size_t indirect_dev_lvl1_width;
    size_t indirect_dev_lvl2_width;
    size_t indirect_dev_page_size;
    size_t cmd_queue_size;
    uint64_t rdbase[32];
    uint64_t redist_base[32];
    uint64_t gicr_propbaser_base;
    uint64_t msi_address;
};

static struct ttos_irq_desc its_irq_desc_list[512];

/* Multi-ITS support: LPI intid -> ITS owner mapping */
#define ITS_MAX_LPIS 65536
static struct gicv3_its *g_lpi_owner[ITS_MAX_LPIS];
static inline void its_record_lpi_owner(struct gicv3_its *its, uint32_t lpi_intid)
{
    if (lpi_intid >= GIC_LPI_INTID_START)
    {
        uint32_t idx = lpi_intid - GIC_LPI_INTID_START;
        if (idx < ITS_MAX_LPIS)
            g_lpi_owner[idx] = its;
    }
}

/* Exported to gicv3 core to route ack to correct ITS */
struct ttos_pic *gicv3_its_pic_by_lpi_index(uint32_t lpi_index)
{
    if (lpi_index < ITS_MAX_LPIS && g_lpi_owner[lpi_index])
        return &g_lpi_owner[lpi_index]->pic;
    return NULL;
}

static __always_inline size_t fls_z(uint32_t x)
{
    uint32_t bits = sizeof(x) * 8;
    uint32_t cmp = 1 << (bits - 1);

    while (bits)
    {
        if (x & cmp)
        {
            return bits;
        }
        cmp >>= 1;
        bits--;
    }

    return 0;
}

static inline uint32_t align_pow2(uint32_t x)
{
    if (x == 0)
        return 1;

    x--; // 先减 1，防止已经是 2 的幂时被错误提升
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;

    return x;
}

static uint32_t gicv3_lpi_alloc_irq(void)
{
    static volatile uint32_t lpi_intid = GIC_LPI_INTID_START;
    return lpi_intid++;
}

static void its_rdist_get_rdbase(struct gicv3_its *its, uint32_t index)
{
    uint64_t rdbase;

    if (MASK_GET(GIC_REG_READ64(its->base.vaddr, GITS_TYPER), GITS_TYPER_PTA))
    {
        rdbase = its->redist_base[index];
    }
    else
    {
        rdbase = (GIC_REG_READ64(its->gicv3->redist[index].vaddr, GICR_TYPER) >> 8) & 0xFFFF;
        rdbase <<= GITS_CMD_RDBASE_SHIFT;
    }

    its->rdbase[index] = rdbase;

    KLOG_I("PTA: %lld  rdbase: 0x%llX",
           MASK_GET(GIC_REG_READ64(its->base.vaddr, GITS_TYPER), GITS_TYPER_PTA), rdbase);
}

static int32_t its_force_quiescent(struct gicv3_its *its)
{
    uint32_t reg, count = 500;
    uint64_t its_base = its->base.vaddr;

    reg = GIC_REG_READ32(its->base.vaddr, GITS_CTLR);
    if (reg & GITS_CTLR_Enabled)
    {
        reg &= ~GITS_CTLR_Enabled;
        GIC_REG_WRITE32(its_base, GITS_CTLR, reg);
    }

    do
    {
        count--;
        if (!count)
        {
            return -1;
        }

        ttos_time_udelay(1000);
        reg = GIC_REG_READ32(its->base.vaddr, GITS_CTLR);
    } while (!(reg & GITS_CTLR_Quiescent));

    return 0;
}

/* Probe the BASER(i) to get the largest supported page size */
static size_t its_probe_baser_page_size(struct gicv3_its *its, uint32_t i)
{
    size_t page_size = GITS_BASER_PAGE_SIZE_64K;

    while (page_size > GITS_BASER_PAGE_SIZE_4K)
    {
        uint64_t reg = GIC_REG_READ64(its->base.vaddr, GITS_BASER(i));

        reg &= ~MASK(GITS_BASER_PAGE_SIZE);
        reg |= MASK_SET(page_size, GITS_BASER_PAGE_SIZE);

        GIC_REG_WRITE64(its->base.vaddr, GITS_BASER(i), reg);

        reg = GIC_REG_READ64(its->base.vaddr, GITS_BASER(i));

        if (MASK_GET(reg, GITS_BASER_PAGE_SIZE) == page_size)
        {
            break;
        }

        switch (page_size)
        {
        case GITS_BASER_PAGE_SIZE_64K:
            page_size = GITS_BASER_PAGE_SIZE_16K;
            break;
        default:
            page_size = GITS_BASER_PAGE_SIZE_4K;
            break;
        }
    }

    switch (page_size)
    {
    case GITS_BASER_PAGE_SIZE_64K:
        return 1024 * 64;
    case GITS_BASER_PAGE_SIZE_16K:
        return 1024 * 16;
    default:
        return 1024 * 4;
    }
}

static int32_t its_alloc_tables(struct gicv3_its *its)
{
#define GITS_BASER_NR_REGS 8

    uint32_t device_ids, i;
    uint64_t reg, type;

    device_ids = MASK_GET(GIC_REG_READ64(its->base.vaddr, GITS_TYPER), GITS_TYPER_DEVBITS) + 1;

    for (i = 0; i < GITS_BASER_NR_REGS; i++)
    {
        size_t page_size, entry_size, page_cnt, lvl2_width = 0;
        bool indirect = false;
        void *alloc_addr;
        phys_addr_t alloc_addr_pa;

        reg = GIC_REG_READ64(its->base.vaddr, GITS_BASER(i));
        type = MASK_GET(reg, GITS_BASER_TYPE);
        entry_size = MASK_GET(reg, GITS_BASER_ENTRY_SIZE) + 1;

        switch (MASK_GET(reg, GITS_BASER_PAGE_SIZE))
        {
        case GITS_BASER_PAGE_SIZE_4K:
            page_size = 1024 * 4;
            break;
        case GITS_BASER_PAGE_SIZE_16K:
            page_size = 1024 * 16;
            break;
        case GITS_BASER_PAGE_SIZE_64K:
            page_size = 1024 * 64;
            break;
        default:
            page_size = 1024 * 64;
            break;
        }

        KLOG_I("BASER  type: %lld  page_size: %ld  entry_size: %ld "
               "device_ids: %d  reg: 0x%llx",
               type, page_size, entry_size, device_ids, reg);
        switch (type)
        {
        case GITS_BASER_TYPE_DEVICE:
            if (device_ids > 16)
            {
                /* Use the largest possible page size for indirect */
                page_size = its_probe_baser_page_size(its, i);

                /*
                 * lvl1 table size:
                 * subtract ID bits that sparse lvl2 table from 'ids'
                 * which is reported by ITS hardware times lvl1 table
                 * entry size.
                 */
                lvl2_width = fls_z(page_size / entry_size) - 1;
                device_ids -= lvl2_width + 1;

                /* The level 1 entry size is a 64bit pointer */
                entry_size = sizeof(uint64_t);

                indirect = true;
            }

            page_cnt = ALIGN(entry_size << device_ids, page_size) / page_size;
            break;

        case GITS_BASER_TYPE_COLLECTION:
            page_cnt = ALIGN(entry_size * ARM_GIC_CPU_NUM, page_size) / page_size;
            break;

        default:
            continue;
        }

        alloc_addr = memalign(page_size, page_size * page_cnt);
        if (!alloc_addr)
        {
            return -1;
        }

        alloc_addr_pa = mm_kernel_v2p((virt_addr_t)alloc_addr);
        alloc_addr = (void *)ioremap(alloc_addr_pa, page_size * page_cnt);
        memset(alloc_addr, 0, page_size * page_cnt);

        KLOG_I("Allocating table of %ldx%ldK pages (%ld bytes entry)  va:%p  "
               "pa:0x%llX",
               page_cnt, page_size / 1024, entry_size, alloc_addr, alloc_addr_pa);

        switch (page_size)
        {
        case 1024 * 4:
            reg = GITS_BASER_PAGE_SIZE_4K << 8;
            break;

        case 1024 * 16:
            reg = GITS_BASER_PAGE_SIZE_16K << 8;
            break;

        case 1024 * 64:
            reg = GITS_BASER_PAGE_SIZE_64K << 8;
            break;
        }

        reg |= MASK_SET(page_cnt - 1, GITS_BASER_SIZE);
        reg |= MASK_SET(GIC_BASER_SHARE_INNER, GITS_BASER_SHAREABILITY); /* Inner Shareable */
        reg |= MASK_SET((uint64_t)alloc_addr_pa >> GITS_BASER_ADDR_SHIFT, GITS_BASER_ADDR);
        reg |= MASK_SET(GIC_BASER_CACHE_INNERLIKE, GITS_BASER_OUTER_CACHE); /* OuterCache */
        reg |= MASK_SET(GIC_BASER_CACHE_RAWAWB, GITS_BASER_INNER_CACHE);    /* Cacheable */
        reg |= MASK_SET(indirect ? 1ULL : 0ULL, GITS_BASER_INDIRECT);       /* Indirect */
        reg |= MASK_SET(1ULL, GITS_BASER_VALID);                            /* Valid */

        GIC_REG_WRITE64(its->base.vaddr, GITS_BASER(i), reg);

        /* TOFIX: check page size & SHAREABILITY validity after write */
        if (type == GITS_BASER_TYPE_DEVICE && indirect)
        {
            its->dev_table_is_indirect = indirect;
            its->indirect_dev_lvl1_table = alloc_addr;
            its->indirect_dev_lvl1_width = device_ids;
            its->indirect_dev_lvl2_width = lvl2_width;
            its->indirect_dev_page_size = page_size;
            KLOG_D("table Indirection enabled");
        }
    }

    return 0;
}

static void its_setup_cmd_queue(struct gicv3_its *its)
{
    uint64_t reg = 0;
    phys_addr_t cmd_base_pa;

    its->cmd_queue_size = ITS_CMD_QUEUE_SIZE;
    its->cmd_base = memalign(0x10000, its->cmd_queue_size);
    its->cmd_write = its->cmd_base;

    cmd_base_pa = mm_kernel_v2p((virt_addr_t)its->cmd_base);
    its->cmd_base = (struct its_cmd_block *)ioremap(cmd_base_pa, its->cmd_queue_size);
    its->cmd_write = its->cmd_base;

    /* Zero out cmd table */
    memset(its->cmd_base, 0, its->cmd_queue_size);

    KLOG_D("cmd base va cache:   %p", its->cmd_base);
    KLOG_D("cmd base pa: [0x%llx : 0x%llx]", cmd_base_pa, cmd_base_pa + its->cmd_queue_size);

    reg |= MASK_SET((its->cmd_queue_size / 4096) - 1, GITS_CBASER_SIZE);
    reg |= MASK_SET(GIC_BASER_SHARE_NO, GITS_CBASER_SHAREABILITY); /* Inner Shareable */
    reg |= MASK_SET(cmd_base_pa >> GITS_CBASER_ADDR_SHIFT, GITS_CBASER_ADDR);
    reg |= MASK_SET(GIC_BASER_CACHE_RAWAWB,
                    GITS_CBASER_OUTER_CACHE); /* Cacheable R-allocate, W-allocate, W-back */
    reg |= MASK_SET(GIC_BASER_CACHE_RAWAWB,
                    GITS_CBASER_INNER_CACHE); /* Cacheable R-allocate, W-allocate, W-back */
    reg |= MASK_SET(1ULL, GITS_CBASER_VALID); /* Valid */

    GIC_REG_WRITE64(its->base.vaddr, GITS_CBASER, reg);

    KLOG_I("GITS_CBASER: 0x%llx  0x%llx", GIC_REG_READ64(its->base.vaddr, GITS_CBASER), reg);
    KLOG_I("Allocated %d entries for command table %p", ITS_CMD_QUEUE_NR_ENTRIES, its->cmd_base);

    GIC_REG_WRITE64(its->base.vaddr, GITS_CWRITER, 0);
}

static bool its_queue_full(struct gicv3_its *its)
{
    uint64_t widx, ridx;

    widx = its->cmd_write - its->cmd_base;
    ridx = GIC_REG_READ64(its->base.vaddr, GITS_CREADR) / sizeof(struct its_cmd_block);

    /* This is incredibly unlikely to happen, unless the ITS locks up. */
    return (((widx + 1) % ITS_CMD_QUEUE_NR_ENTRIES) == ridx);
}

static struct its_cmd_block *its_allocate_entry(struct gicv3_its *its)
{
    struct its_cmd_block *cmd;
    uint32_t count = 1000000; /* 1s! */

    while (its_queue_full(its))
    {
        count--;
        if (!count)
        {
            KLOG_E("ITS queue not draining");
            return NULL;
        }
        ttos_time_udelay(1);
    }

    cmd = its->cmd_write++;

    /* Handle queue wrapping */
    if (its->cmd_write == (its->cmd_base + ITS_CMD_QUEUE_NR_ENTRIES))
    {
        its->cmd_write = its->cmd_base;
    }

    /* Clear command  */
    cmd->raw_cmd[0] = 0;
    cmd->raw_cmd[1] = 0;
    cmd->raw_cmd[2] = 0;
    cmd->raw_cmd[3] = 0;

    return cmd;
}

static int32_t its_post_command(struct gicv3_its *its, struct its_cmd_block *cmd)
{
    uint64_t wr_idx, rd_idx, idx;
    uint32_t count = 1000000; /* 1s! */

    wr_idx = (its->cmd_write - its->cmd_base) * sizeof(*cmd);
    rd_idx = GIC_REG_READ64(its->base.vaddr, GITS_CREADR);

    GIC_REG_WRITE64(its->base.vaddr, GITS_CWRITER, wr_idx);

    while (1)
    {
        idx = GIC_REG_READ64(its->base.vaddr, GITS_CREADR);
        if (idx == wr_idx)
        {
            break;
        }

        count--;
        if (!count)
        {
            KLOG_E("cmd[0] 0x%" PRIx64, cmd->raw_cmd[0]);
            KLOG_E("cmd[1] 0x%" PRIx64, cmd->raw_cmd[1]);
            KLOG_E("cmd[2] 0x%" PRIx64, cmd->raw_cmd[2]);
            KLOG_E("cmd[3] 0x%" PRIx64, cmd->raw_cmd[3]);

            KLOG_E("ITS queue timeout (rd 0x%" PRIx64 " => 0x%" PRIx64 " => wr 0x%" PRIx64 ")",
                   rd_idx, idx, wr_idx);
            return -1;
        }
        ttos_time_udelay(1);
    }

    return 0;
}

static int32_t its_send_cmd(struct gicv3_its *its, uint64_t *raw_cmd)
{
    struct its_cmd_block *cmd = its_allocate_entry(its);

    if (!cmd)
    {
        return -1;
    }

    memcpy(cmd->raw_cmd, raw_cmd, sizeof(uint64_t) * 4);

    return its_post_command(its, cmd);
}
static int32_t its_send_sync_cmd(struct gicv3_its *its, uint64_t rd_addr)
{
    return its_send_cmd(its,
                        ITS_CMD(MASK_SET(GITS_CMD_ID_SYNC, GITS_CMD_ID), 0,
                                MASK_SET(rd_addr >> GITS_CMD_RDBASE_ALIGN, GITS_CMD_RDBASE), 0));
}

static int32_t its_send_mapc_cmd(struct gicv3_its *its, uint32_t icid, uint32_t cpuid, bool valid)
{
    return its_send_cmd(
        its, ITS_CMD(MASK_SET(GITS_CMD_ID_MAPC, GITS_CMD_ID), 0,
                     MASK_SET((uint64_t)icid, GITS_CMD_ICID) |
                         MASK_SET(its->rdbase[cpuid] >> GITS_CMD_RDBASE_ALIGN, GITS_CMD_RDBASE) |
                         MASK_SET(valid ? 1ULL : 0ULL, GITS_CMD_VALID),
                     0));
}

static int32_t its_send_mapd_cmd(struct gicv3_its *its, uint32_t device_id, uint32_t size,
                                 uint64_t itt_addr, bool valid)
{
    int32_t ret =
        its_send_cmd(its, ITS_CMD(MASK_SET(GITS_CMD_ID_MAPD, GITS_CMD_ID) |
                                      MASK_SET((uint64_t)device_id, GITS_CMD_DEVICEID),
                                  MASK_SET((uint64_t)size, GITS_CMD_SIZE),
                                  MASK_SET(itt_addr >> GITS_CMD_ITTADDR_ALIGN, GITS_CMD_ITTADDR) |
                                      MASK_SET(valid ? 1ULL : 0ULL, GITS_CMD_VALID),
                                  0));
    if (ret == 0)
    {
        (void)its_send_sync_cmd(its, its->rdbase[cpuid_get()]);
    }
    return ret;
}

/* forward decl for INV used by MAPTI */
static int32_t its_send_inv_cmd(struct gicv3_its *its, uint32_t device_id, uint32_t event_id);

static int32_t its_send_mapti_cmd(struct gicv3_its *its, uint32_t device_id, uint32_t event_id,
                                  uint32_t intid, uint32_t icid)
{
    int32_t ret = its_send_cmd(its, ITS_CMD(MASK_SET(GITS_CMD_ID_MAPTI, GITS_CMD_ID) |
                                                MASK_SET((uint64_t)device_id, GITS_CMD_DEVICEID),
                                            MASK_SET((uint64_t)event_id, GITS_CMD_EVENTID) |
                                                MASK_SET((uint64_t)intid, GITS_CMD_PINTID),
                                            MASK_SET((uint64_t)icid, GITS_CMD_ICID), 0));
    if (ret == 0)
    {
        (void)its_send_inv_cmd(its, device_id, event_id);
        (void)its_send_sync_cmd(its, its->rdbase[cpuid_get()]);
    }
    return ret;
}

static int32_t its_send_int_cmd(struct gicv3_its *its, uint32_t device_id, uint32_t event_id)
{
    return its_send_cmd(its, ITS_CMD(MASK_SET(GITS_CMD_ID_INT, GITS_CMD_ID) |
                                         MASK_SET((uint64_t)device_id, GITS_CMD_DEVICEID),
                                     MASK_SET((uint64_t)event_id, GITS_CMD_EVENTID), 0, 0));
}

static int32_t its_send_inv_cmd(struct gicv3_its *its, uint32_t device_id, uint32_t event_id)
{
    return its_send_cmd(its, ITS_CMD(MASK_SET(GITS_CMD_ID_INV, GITS_CMD_ID) |
                                         MASK_SET((uint64_t)device_id, GITS_CMD_DEVICEID),
                                     MASK_SET((uint64_t)event_id, GITS_CMD_EVENTID), 0, 0));
}

static int32_t gicv3_its_map_intid(struct gicv3_its *its, uint32_t device_id, uint32_t event_id,
                                   uint32_t intid)
{
    int32_t ret;

    /* TOFIX check device_id, event_id & intid bounds */
    if (intid < GIC_LPI_INTID_START)
    {
        return -1;
    }

    /* The CPU id directly maps as ICID for the current CPU redistributor */
    ret = its_send_mapti_cmd(its, device_id, event_id, intid, ICID_NO);
    if (ret)
    {
        KLOG_E("Failed to map eventid %d to intid %d for deviceid 0x%x", event_id, intid,
               device_id);
        return ret;
    }

    return 0;
}

static int32_t gicv3_its_init_device_id(struct gicv3_its *its, uint32_t device_id, uint32_t nites)
{
    size_t entry_size, alloc_size;
    uint32_t nr_ites;
    int32_t ret;
    uint64_t itt;

    /* TOFIX check device_id & nites bounds */

    entry_size =
        MASK_GET(GIC_REG_READ64(its->base.vaddr, GITS_TYPER), GITS_TYPER_ITT_ENTRY_SIZE) + 1;
    if (its->dev_table_is_indirect)
    {
        size_t offset = device_id >> its->indirect_dev_lvl2_width;

        /* Check if DeviceID can fit in the Level 1 table */
        if (offset > (1 << its->indirect_dev_lvl1_width))
        {
            return -1;
        }

        /* Check if a Level 2 table has already been allocated for the DeviceID
         */
        if (!its->indirect_dev_lvl1_table[offset])
        {
            void *alloc_addr;
            phys_addr_t alloc_addr_pa;

            KLOG_I("Allocating Level 2 Device %ldK table", its->indirect_dev_page_size / 1024);

            alloc_addr = memalign(its->indirect_dev_page_size, its->indirect_dev_page_size);
            if (!alloc_addr)
            {
                return -2;
            }

            alloc_addr_pa = mm_kernel_v2p((virt_addr_t)alloc_addr);
            alloc_addr = (void *)ioremap(alloc_addr_pa, its->indirect_dev_page_size);

            memset(alloc_addr, 0, its->indirect_dev_page_size);
            its->indirect_dev_lvl1_table[offset] =
                (size_t)alloc_addr | MASK_SET(1UL, GITS_BASER_VALID);
        }
    }

    /* ITT must be of power of 2 */
    nr_ites = max(2u, nites);
    nr_ites = align_pow2(nr_ites);
    alloc_size = ALIGN(nr_ites * entry_size, 256);

    KLOG_I("Allocating ITT for DeviceID 0x%x and %d vectors (%ld bytes entry)", device_id, nr_ites,
           entry_size);

    itt = (uintptr_t)memalign(256, alloc_size);
    if (!itt)
    {
        return -2;
    }

    /* size is log2(ites) - 1, equivalent to (fls(ites) - 1) - 1 */
    itt = mm_kernel_v2p((virt_addr_t)itt);
    ret = its_send_mapd_cmd(its, device_id, fls_z(nr_ites) - 2, itt, true);
    if (ret)
    {
        KLOG_E("Failed to map device id %x ITT table", device_id);
        return ret;
    }

    return 0;
}

static int32_t its_init(struct gicv3_its *its)
{
    int32_t ret;
    uint32_t reg;

    if (!is_bootcpu())
    {
        return -1;
    }
    KLOG_D("GITS_TYPER: 0x%" PRIx64, GIC_REG_READ64(its->base.vaddr, GITS_TYPER));

    its_rdist_get_rdbase(its, cpuid_get());

    ret = its_force_quiescent(its);
    if (ret)
    {
        KLOG_E("Failed to quiesce, giving up");
        return ret;
    }

    ret = its_alloc_tables(its);
    if (ret)
    {
        KLOG_E("Failed to allocate tables, giving up");
        return ret;
    }

    its_setup_cmd_queue(its);

    reg = GIC_REG_READ32(its->base.vaddr, GITS_CTLR);
    reg |= GITS_CTLR_Enabled;
    GIC_REG_WRITE32(its->base.vaddr, GITS_CTLR, reg);

    /* Map the boot CPU id to the CPU redistributor */
    ret = its_send_mapc_cmd(its, ICID_NO, cpuid_get(), true);
    if (ret)
    {
        KLOG_E("Failed to map boot CPU redistributor");
        return ret;
    }

    its->msi_address = GITS_TRANSLATER_ADDR(its->base.paddr);

    return 0;
}

static int32_t lpi_init(struct gicv3_its *its)
{
    struct pic_gicv3 *gic = its->gicv3;
    virt_addr_t base;
    virt_addr_t propbaser_va, pendbaser_va;
    phys_addr_t propbaser_pa, pendbaser_pa;
    uint32_t i, lpi_id_bits, prop_size, pend_size;
    uint64_t val, val1;

    lpi_id_bits = (GIC_REG_READ32(gic->dist.vaddr, GICD_TYPER) >> 19) & 0x1F;
    prop_size = 1 << (lpi_id_bits + 1);
    pend_size = ALIGN(prop_size / 8, 0x10000);

    if (!gic->lpi_tables_inited)
    {
        propbaser_va = (virt_addr_t)memalign(0x10000, prop_size);
        pendbaser_va = (virt_addr_t)memalign(0x10000, pend_size * ARM_GIC_CPU_NUM);

        memset((void *)propbaser_va, 0, prop_size);
        memset((void *)pendbaser_va, 0, pend_size * ARM_GIC_CPU_NUM);

        cache_dcache_clean(propbaser_va, prop_size);
        cache_dcache_clean(pendbaser_va, pend_size * ARM_GIC_CPU_NUM);

        propbaser_pa = mm_kernel_v2p(propbaser_va);
        pendbaser_pa = mm_kernel_v2p(pendbaser_va);

        /* Program Redistributor tables once */
        base = gic->redist[0].vaddr;
        for (i = 0; i < ARM_GIC_CPU_NUM; i++)
        {
            val = GIC_REG_READ32(base, GICR_CTLR);
            val &= ~GICR_CTLR_EnableLPI;
            GIC_REG_WRITE32(base, GICR_CTLR, val);

            val = (uint64_t)propbaser_pa | 0x780 | lpi_id_bits;
            val1 = (uint64_t)pendbaser_pa | 0x4000000000000780ull;
            GIC_REG_WRITE64(base, GICR_PROPBASER, val);
            GIC_REG_WRITE64(base, GICR_PENDBASER, val1);

            val = GIC_REG_READ32(base, GICR_CTLR);
            val |= GICR_CTLR_EnableLPI;
            GIC_REG_WRITE32(base, GICR_CTLR, val);

            GIC_REG_WRITE32(base, GICR_INVALLR, 0);
            GIC_REG_WRITE32(base, GICR_SYNCR, 0);

            pendbaser_pa += pend_size;
            base += 0x20000;
        }

        gic->lpi_propbases_va = (uint64_t)propbaser_va;
        gic->lpi_pendbases_va = (uint64_t)pendbaser_va;
        gic->lpi_prop_size = prop_size;
        gic->lpi_pend_size = pend_size;
        gic->lpi_tables_inited = 1;
    }

    /* Reuse shared tables */
    its->gicr_propbaser_base = gic->lpi_propbases_va;
    return 0;
}

static int32_t gicv3_its_init(struct ttos_pic *pic)
{
    struct gicv3_its *its = (struct gicv3_its *)pic;

    if (its_init(its))
    {
        return -1;
    }

    return lpi_init(its);
}

static int32_t gicv3_its_max_number_get(struct ttos_pic *pic, uint32_t *max_number)
{
    *max_number = 1024;
    return 0;
}

static int32_t gicv3_its_eoi(struct ttos_pic *pic, uint32_t irq, uint32_t src_cpu)
{
    uint32_t val = 0;

    val = (src_cpu & GICC_IAR_CPUID_MASK) << GICC_IAR_CPUID_SHIFT;
    val |= (irq + GIC_LPI_INTID_START) & GICC_IAR_INTID_MASK;
    arch_gicv3_eoir1_write(val);

    return (0);
}

static int32_t gicv3_its_mask(struct ttos_pic *pic, uint32_t irq)
{
    struct gicv3_its *its = (struct gicv3_its *)pic;
    uint32_t reg;
    uint32_t lpi_index = irq;
    virt_addr_t rd_base;

    reg = readb(((uintptr_t)its->gicr_propbaser_base) + lpi_index);
    reg &= ~0x1;
    writeb(reg, ((uintptr_t)its->gicr_propbaser_base) + lpi_index);

    cache_dcache_clean(((uintptr_t)its->gicr_propbaser_base) + lpi_index, 1);

    /* Ensure Redistributor refetches LPI property (invalidate all as a safe fallback) */
    rd_base = its->gicv3->redist[cpuid_get()].vaddr;
    GIC_REG_WRITE32(rd_base, GICR_INVALLR, 0);
    GIC_REG_WRITE32(rd_base, GICR_SYNCR, 0);
    /* 虚拟化没有模拟GICR的invall动作 非要我发送一个invall命令给its */

    {
        /* 依据 irq(LPI索引) 反查 device_id/event_id，发 INV+SYNC */
        uint32_t hw = its_irq_desc_list[lpi_index].hw_irq;
        uint32_t device_id = hw >> 16;
        uint32_t event_id = hw & 0xFF;
        (void)its_send_inv_cmd(its, device_id, event_id);
        (void)its_send_sync_cmd(its, its->rdbase[cpuid_get()]);
    }

    return 0;
}

static int32_t gicv3_its_umask(struct ttos_pic *pic, uint32_t irq)
{
    struct gicv3_its *its = (struct gicv3_its *)pic;
    uint32_t reg;
    uint32_t lpi_index = irq;
    virt_addr_t rd_base;
    reg = readb(((uintptr_t)its->gicr_propbaser_base) + lpi_index);
    reg |= 0x1;
    writeb(reg, ((uintptr_t)its->gicr_propbaser_base) + lpi_index);

    cache_dcache_clean(((uintptr_t)its->gicr_propbaser_base) + lpi_index, 1);

    /* Ensure Redistributor refetches LPI property (invalidate all as a safe fallback) */
    rd_base = its->gicv3->redist[cpuid_get()].vaddr;
    GIC_REG_WRITE32(rd_base, GICR_INVALLR, 0);
    GIC_REG_WRITE32(rd_base, GICR_SYNCR, 0);
    {
        uint32_t hw = its_irq_desc_list[lpi_index].hw_irq;
        uint32_t device_id = hw >> 16;
        uint32_t event_id = hw & 0xFF;
        (void)its_send_inv_cmd(its, device_id, event_id);
        (void)its_send_sync_cmd(its, its->rdbase[cpuid_get()]);
    }
    return 0;
}

static void *gicv3_its_irq_desc_get(struct ttos_pic *pic, int32_t hwirq)
{
    return its_irq_desc_list + hwirq;
}

static int32_t gicv3_its_msi_alloc(struct ttos_pic *pic, uint32_t device_id, uint32_t count)
{
    struct gicv3_its *its = (struct gicv3_its *)pic;
    gicv3_its_init_device_id(its, device_id, count);

    return 0;
}

static int32_t gicv3_its_msi_map(struct ttos_pic *pic, uint32_t device_id, uint32_t event_id)
{
    struct gicv3_its *its = (struct gicv3_its *)pic;
    uint32_t irq;

    irq = gicv3_lpi_alloc_irq();
    its_record_lpi_owner(its, irq);
    if (gicv3_its_map_intid(its, device_id, event_id, irq))
    {
        return -1;
    }

    irq -= GIC_LPI_INTID_START;
    its_irq_desc_list[irq].hw_irq = (device_id << 16) | event_id;

    return irq + pic->virt_irq_rang[0];
}

static int32_t gicv3_its_msi_send(struct ttos_pic *pic, uint32_t irq)
{
    irq -= pic->virt_irq_rang[0];
    return its_send_int_cmd((struct gicv3_its *)pic, its_irq_desc_list[irq].hw_irq >> 16,
                            its_irq_desc_list[irq].hw_irq & 0xFF);
}

static uint64_t gicv3_its_msi_address(struct ttos_pic *pic)
{
    struct gicv3_its *its = (struct gicv3_its *)pic;
    return its->msi_address;
}

static uint64_t gicv3_its_msi_data(struct ttos_pic *pic, uint32_t index, ttos_irq_desc_t desc)
{
    return index;
}

/* pic通用操作 */
static struct ttos_pic_ops gicv3_its_ops = {
    .pic_init = gicv3_its_init,
    .pic_mask = gicv3_its_mask,
    .pic_unmask = gicv3_its_umask,
    .pic_eoi = gicv3_its_eoi,
    .pic_max_number_get = gicv3_its_max_number_get,
    .pic_irq_desc_get = gicv3_its_irq_desc_get,
    .pic_msi_alloc = gicv3_its_msi_alloc,
    .pic_msi_map = gicv3_its_msi_map,
    .pic_msi_send = gicv3_its_msi_send,
    .pic_msi_address = gicv3_its_msi_address,
    .pic_msi_data = gicv3_its_msi_data,
};

static struct gicv3_its *dbg_its;

static int32_t gicv3_its_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct gicv3_its *gicv3_its;

    gicv3_its = calloc(1, sizeof(struct gicv3_its));
    gicv3_its->gicv3 = dev_priv_get((struct device *)dev->of_node->parent->device);
    gicv3_its->gicv3->its = &gicv3_its->pic;
    gicv3_its->pic.name = dev->driver->name;
    gicv3_its->pic.pic_ops = &gicv3_its_ops;

    of_device_get_resource_regs(dev, &gicv3_its->base, 1);

    dev_priv_set(&gicv3_its->pic, dev);
    dev_priv_set(dev, &gicv3_its->pic);

    KLOG_D("phandle: 0x%X", dev->of_node->phandle);
    KLOG_D("parent phandle: 0x%X", dev->of_node->parent->phandle);

    ttos_pic_register(&gicv3_its->pic);

    dbg_its = gicv3_its;

    return 0;
}

static struct of_device_id gicv3_its_table[] = {
    {.compatible = "arm,gic-v3-its"},
    {/* end of list */},
};

static struct platform_driver gicv3_its_driver = {
    .probe = gicv3_its_probe,
    .driver =
        {
            .name = "gicv3-its",
            .of_match_table = gicv3_its_table,
        },
};

static int32_t gicv3_its_driver_init(void)
{
    return platform_add_driver(&gicv3_its_driver);
}
INIT_EXPORT_PRE_PIC(gicv3_its_driver_init, "gicv3 ITS driver init");

#ifdef ITS_TEST
static int gicv3_its_send_int(struct gicv3_its *its, uint32_t device_id, uint32_t event_id)
{
    /* TOFIX check device_id & event_id bounds */
    return its_send_int_cmd(its, device_id, event_id);
}

static void lpi_test_isr(uint32_t irq, void *param)
{
    printk("lpi_test_isr  vector %d\n", irq);
}

void its_test(void)
{
    uint32_t irq[5], device_id, event_id, i;
    int ret = 0;

    device_id = 0x123;
    event_id = 0x5;

    ttos_pic_msi_alloc(&dbg_its->pic, device_id, 32);

    for (i = 0; i < 5; i++)
    {
        irq[i] = ttos_pic_msi_map(&dbg_its->pic, device_id, event_id + i);
        ttos_pic_irq_install(irq[i], lpi_test_isr, 0, 0, "its");
        if (!irq[i])
        {
            KLOG_E("Failed to map boot CPU gicv3_its_map_intid");
            return;
        }

        ttos_pic_irq_unmask(irq[i]);
    }

    usleep(100000);

    for (i = 0; i < 5; i++)
    {
        ttos_pic_msi_send(&dbg_its->pic, irq[i]);
    }
}

#include <shell.h>
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 its, its_test, its);
#endif
