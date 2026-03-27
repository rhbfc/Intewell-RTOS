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

#include <driver/pci/pci.h>
#include <driver/pci/pci_host.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_NAME_LENGTH 128
#define TYPE0 0x4000
#define TYPE1 0x8000
#define TYPE01 0xC000

struct pci_config_space_item
{
    uint16_t cap_id;
    uint16_t offset;
    uint32_t len;
    const char *desc;
};

static const struct pci_config_space_item pci_config_type[] = {
    {
        .cap_id = TYPE0,
        .desc = "Configuration Space Header Type0",
    },
    {
        .cap_id = TYPE1,
        .desc = "Configuration Space Header Type1",
    },
    {
        .cap_id = PCI_CAP_ID_PM,
        .desc = "PCI Power Management Capability",
    },
    {
        .cap_id = PCI_CAP_ID_MSI,
        .desc = "MSI Capability",
    },
    {.cap_id = PCI_CAP_ID_MSIX, .desc = "MSI-X Capability"},
    {
        .cap_id = PCI_CAP_ID_EXP,
        .desc = "PCI Express Capability",
    },
    {.cap_id = PCI_CAP_ID_VNDR, .desc = "Vendor-Specific Capability"},
};

#define SPACE_ITEM(a, b, c, d)                                                                     \
    {                                                                                              \
        .cap_id = a, .offset = b, .len = c, .desc = #d                                             \
    }

static struct pci_config_space_item space_table[] = {
    SPACE_ITEM(TYPE01, 2, 0, Device ID),
    SPACE_ITEM(TYPE01, 2, 0, Vendor ID),
    SPACE_ITEM(TYPE01, 2, 0, Status),
    SPACE_ITEM(TYPE01, 2, 0, Command),
    SPACE_ITEM(TYPE01, 3, 0, Class Code),
    SPACE_ITEM(TYPE01, 1, 0, Revision ID),
    SPACE_ITEM(TYPE01, 1, 0, BIST),
    SPACE_ITEM(TYPE01, 1, 0, Header Type),
    SPACE_ITEM(TYPE01, 1, 0, Latency Timer),
    SPACE_ITEM(TYPE01, 1, 0, Cache Line Size),
    SPACE_ITEM(TYPE01, 4, 0, Base Address Register 0),
    SPACE_ITEM(TYPE01, 4, 0, Base Address Register 1),
    /* TYPE0 0x18 - 0x30 */
    SPACE_ITEM(TYPE0, 4, 0, Base Address Register 2),
    SPACE_ITEM(TYPE0, 4, 0, Base Address Register 3),
    SPACE_ITEM(TYPE0, 4, 0, Base Address Register 4),
    SPACE_ITEM(TYPE0, 4, 0, Base Address Register 5),
    SPACE_ITEM(TYPE0, 4, 0, Cardbus CIS Pointer),
    SPACE_ITEM(TYPE0, 2, 0, Subsystem ID),
    SPACE_ITEM(TYPE0, 2, 0, Subsystem Vendor ID),
    SPACE_ITEM(TYPE0, 4, 0, Expansion ROM Base Address),
    /* TYPE1 0x18 - 0x30 */
    SPACE_ITEM(TYPE1, 1, 0, Sec_Lat_Timer),
    SPACE_ITEM(TYPE1, 1, 0, Sub_Bus_No),
    SPACE_ITEM(TYPE1, 1, 0, Sec_Bus_No),
    SPACE_ITEM(TYPE1, 1, 0, Prim_Bus_No),
    SPACE_ITEM(TYPE1, 2, 0, Secondary Status Register),
    SPACE_ITEM(TYPE1, 1, 0, I / O Limit),
    SPACE_ITEM(TYPE1, 1, 0, I / O Base),
    SPACE_ITEM(TYPE1, 2, 0, Memory Limit),
    SPACE_ITEM(TYPE1, 2, 0, Memory Base),
    SPACE_ITEM(TYPE1, 2, 0, Prefetchable Memory Limit),
    SPACE_ITEM(TYPE1, 2, 0, Prefetchable Memory Base),
    SPACE_ITEM(TYPE1, 4, 0, Prefetchable Base Upper 32 Bits),
    SPACE_ITEM(TYPE1, 4, 0, Prefetchable Limit Upper 32 Bits),
    SPACE_ITEM(TYPE1, 2, 0, I / O Limit Upper 16 Bits),
    SPACE_ITEM(TYPE1, 2, 0, I / O Base Upper 16 Bits),
    SPACE_ITEM(TYPE01, 3, 0, Reserved),
    SPACE_ITEM(TYPE01, 1, 0, Cap_Ptr),
    /* TYPE0 0x38 - 0x3E */
    SPACE_ITEM(TYPE0, 4, 0, Reserved),
    SPACE_ITEM(TYPE0, 1, 0, Max_Lat),
    SPACE_ITEM(TYPE0, 1, 0, Min_Gnt),
    /* TYPE1 0x38 - 0x3E */
    SPACE_ITEM(TYPE1, 4, 0, Expansion ROM Base Address),
    SPACE_ITEM(TYPE1, 2, 0, Bridge Control),
    SPACE_ITEM(TYPE01, 1, 0, Int_Pin),
    SPACE_ITEM(TYPE01, 1, 0, Int_Line),

    SPACE_ITEM(PCI_CAP_ID_PM, 2, 0, PMC),
    SPACE_ITEM(PCI_CAP_ID_PM, 1, 0, Next_Item_Ptr),
    SPACE_ITEM(PCI_CAP_ID_PM, 1, 0, Cap_ID),
    SPACE_ITEM(PCI_CAP_ID_PM, 1, 0, Data),
    SPACE_ITEM(PCI_CAP_ID_PM, 3, 0, PMCSR),

    SPACE_ITEM(PCI_CAP_ID_MSI, 2, 0, Message Control),
    SPACE_ITEM(PCI_CAP_ID_MSI, 1, 0, Next_Item_Ptr),
    SPACE_ITEM(PCI_CAP_ID_MSI, 1, 0, Cap_ID),
    SPACE_ITEM(PCI_CAP_ID_MSI, 4, 0, Message Lower Address),
    SPACE_ITEM(PCI_CAP_ID_MSI, 4, 0, Message Upper Address),
    SPACE_ITEM(PCI_CAP_ID_MSI, 2, 0, Reserved),
    SPACE_ITEM(PCI_CAP_ID_MSI, 2, 0, Message Data),

    SPACE_ITEM(PCI_CAP_ID_MSIX, 2, 0, Message Control),
    SPACE_ITEM(PCI_CAP_ID_MSIX, 1, 0, Next_Item_Ptr),
    SPACE_ITEM(PCI_CAP_ID_MSIX, 1, 0, Cap_ID),
    SPACE_ITEM(PCI_CAP_ID_MSIX, 4, 0, Table Offset / Table BIR),
    SPACE_ITEM(PCI_CAP_ID_MSIX, 4, 0, PBA Offset / PBA BIR),

    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, PCI Express Capabilities Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 1, 0, Next_Item_Ptr),
    SPACE_ITEM(PCI_CAP_ID_EXP, 1, 0, Cap_ID),
    SPACE_ITEM(PCI_CAP_ID_EXP, 4, 0, Device Capabilities Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Device Status Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Device Control Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 4, 0, Link Capabilities Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Link Status Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Link Control Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 4, 0, Slot Capabilities Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Slot Status Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Slot Control Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Root Capabilities Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Root Control Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 4, 0, Root Status Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 4, 0, Device Capabilities 2 Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Device Status 2 Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Device Control 2 Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 4, 0, Link Capabilities 2 Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Link Status 2 Register),
    SPACE_ITEM(PCI_CAP_ID_EXP, 2, 0, Link Control 2 Register),

    SPACE_ITEM(PCI_CAP_ID_VNDR, 1, 0, VS Register),
    SPACE_ITEM(PCI_CAP_ID_VNDR, 1, 0, Length),
    SPACE_ITEM(PCI_CAP_ID_VNDR, 1, 0, Next_Item_Ptr),
    SPACE_ITEM(PCI_CAP_ID_VNDR, 1, 0, Cap_ID),
};

static uint32_t pci_dump_get_max_len(uint32_t type)
{
    uint32_t i, max_len = 0, len;
    uint32_t item_number = sizeof(space_table) / sizeof(struct pci_config_space_item);

    for (i = 0; i < item_number; i++)
    {
        if ((space_table[i].cap_id == type) || ((type & TYPE01) && (space_table[i].cap_id & type)))
        {
            space_table[i].len = strlen(space_table[i].desc);
            len = space_table[i].len + 3;
            len = ALIGN(len, space_table[i].offset) / space_table[i].offset;
            max_len = max_len > len ? max_len : len;
        }
    }

    return max_len * 4 + 1;
}

static int pci_dump_title(uint32_t type, uint32_t reg_offset, uint32_t max_len)
{
    uint32_t item_number = sizeof(pci_config_type) / sizeof(struct pci_config_space_item);
    uint32_t i, offset = 0;
    char str[MAX_NAME_LENGTH];
    char str1[MAX_NAME_LENGTH];
    char *pstr = str;

    memset(pstr, 0, MAX_NAME_LENGTH);
    memset(pstr, ' ', max_len);
    pstr[0] = pstr[max_len - 1] = '+';

    for (i = 0; i < item_number; i++)
    {
        if (pci_config_type[i].cap_id == type)
        {
            sprintf(str1, "%s (%03Xh)%n", pci_config_type[i].desc, reg_offset, &offset);
            pstr += (max_len - offset) / 2;
            memcpy(pstr, str1, offset);
            printk("\n\n\n%s%s%s\n", "\033[47;30m", str, "\033[0m");
            return 0;
        }
    }

    return -1;
}

static void pci_dump_format(uint32_t index, uint32_t max_len)
{
    uint32_t type;
    char str[MAX_NAME_LENGTH];
    uint32_t offset = 0, item_len;

    memset(str, 0, MAX_NAME_LENGTH);
    memset(str, '-', max_len);
    item_len = max_len / 4;

    type = space_table[index].cap_id;

    while (offset < 4)
    {
        if ((space_table[index].cap_id == type) ||
            ((type & TYPE01) && (space_table[index].cap_id & type)))
        {
            str[offset * item_len] = '+';
            offset += space_table[index].offset;
        }
        index++;
    }
    str[max_len - 1] = '+';

    printk("%s\n", str);

    return;
}

static void pci_dump_desc(uint32_t index, uint32_t reg_offset, uint32_t max_len)
{
    uint32_t type;
    char str[MAX_NAME_LENGTH], *pstr;
    uint32_t offset = 0, len, item_len;

    pstr = str;
    memset(pstr, 0, MAX_NAME_LENGTH);
    memset(pstr, ' ', max_len);
    len = max_len / 4;

    type = space_table[index].cap_id;

    while (offset < 4)
    {
        if ((space_table[index].cap_id == type) ||
            ((type & TYPE01) && (space_table[index].cap_id & type)))
        {
            pstr = str + len * offset;
            *pstr = '|';
            item_len = len * space_table[index].offset;
            pstr += (item_len - 1 - space_table[index].len) / 2 + 1;
            memcpy(pstr, space_table[index].desc, space_table[index].len);
            offset += space_table[index].offset;
        }
        index++;
    }
    str[max_len - 1] = '|';
    sprintf(str + max_len, "%02X", reg_offset);
    printk("%s\n", str);
}

static uint32_t pci_dump_value(struct pci_host *host, uint32_t bdf, uint32_t cap_ptr,
                               uint32_t index, uint32_t reg_offset, uint32_t max_len)
{
    uint32_t type;
    char str[MAX_NAME_LENGTH], *pstr;
    uint32_t offset = 0, item = 0, len, item_len;
    uint32_t data;

    pstr = str;
    memset(pstr, 0, MAX_NAME_LENGTH);
    memset(pstr, ' ', max_len);
    len = max_len / 4;

    item = index;

    type = space_table[index].cap_id;

    while (offset < 4)
    {
        if ((space_table[index].cap_id == type) ||
            ((type & TYPE01) && (space_table[index].cap_id & type)))
        {
            pstr = str + len * offset;
            *pstr = '|';
            item_len = len * space_table[index].offset;
            pstr += (item_len - 1 - space_table[index].offset * 2) / 2 + 1;

            switch (space_table[index].offset)
            {
            case 1:
                data = pci_cfg_readb(host, bdf, cap_ptr + reg_offset + (3 - offset));
                sprintf(pstr, "%02X", data);
                break;

            case 2:
                data = pci_cfg_readw(host, bdf, cap_ptr + reg_offset + (2 - offset));
                sprintf(pstr, "%04X", data);
                break;

            case 3:
                data = pci_cfg_readl(host, bdf, cap_ptr + reg_offset + (offset & ~0x3));
                data = (data >> ((1 - offset) * 8)) & 0xFFFFFF;
                sprintf(pstr, "%06X", data);
                break;

            case 4:
                data = pci_cfg_readl(host, bdf, cap_ptr + reg_offset + offset);
                sprintf(pstr, "%08X", data);
                break;
            }
            pstr[space_table[index].offset * 2] = ' ';
            offset += space_table[index].offset;
        }
        index++;
    }
    str[max_len - 1] = '|';
    printk("%s\n", str);

    return index - item;
}

static uint32_t pci_dump_space(struct pci_host *host, uint32_t bdf, uint32_t cap_ptr)
{
    uint32_t item_number = sizeof(space_table) / sizeof(struct pci_config_space_item);
    uint32_t i, j = 0, max_len, offset, type, new_cap_ptr;

    if (cap_ptr == 0)
    {
        type = 1 << (14 + pci_cfg_readb(host, bdf, PCI_HEADER_TYPE));
        new_cap_ptr = pci_cfg_readb(host, bdf, PCI_CAPABILITY_LIST);
    }
    else
    {
        type = pci_cfg_readb(host, bdf, cap_ptr);
        new_cap_ptr = pci_cfg_readb(host, bdf, cap_ptr + PCI_CAP_LIST_NEXT);
    }

    max_len = pci_dump_get_max_len(type);
    if ((max_len > MAX_NAME_LENGTH) || (max_len < 10))
    {
        return 0;
    }

    if (pci_dump_title(type, cap_ptr, max_len))
    {
        return 0;
    }

    offset = 0;
    for (i = 0; i < item_number;)
    {
        if ((space_table[i].cap_id == type) || ((type & TYPE01) && (space_table[i].cap_id & type)))
        {
            pci_dump_format(i, max_len);
            pci_dump_desc(i, offset, max_len);
            j = i;
            i += pci_dump_value(host, bdf, cap_ptr, j, offset, max_len);
            offset += 4;
        }
        else
        {
            i++;
        }
    }
    pci_dump_format(j, max_len);

    return new_cap_ptr;
}

static void pci_config_dump(struct pci_host *host, uint32_t bdf)
{
    uint32_t cap_ptr = 0;

    do
    {
        cap_ptr = pci_dump_space(host, bdf, cap_ptr);
    } while (cap_ptr);
}

static struct pci_host *pci_dbg_host;
void pci_dbg_host_set(struct pci_host *host)
{
    pci_dbg_host = host;
}

#include <shell.h>
static void pci_tool(int argc, char *argv[])
{
    uint32_t bdf;
    struct pci_host *host;

    if (argc < 4)
        return;

    bdf = PCI_BDF(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    pci_config_dump(pci_dbg_host, bdf);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) |
                     SHELL_CMD_DISABLE_RETURN,
                 pci, pci_tool, pci tools);
