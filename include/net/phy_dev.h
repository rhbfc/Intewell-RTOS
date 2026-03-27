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

#ifndef __PHY_DEV_H__
#define __PHY_DEV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <spinlock.h>
#include <wqueue.h>
#include <fs/fs.h>
#include <net/net.h>
#include <netinet/in.h>
#include <list.h>
#include <net/ethernet_dev.h>

#define IFM_NMASK   0x000000e0    /* Network type */
#define IFM_TMASK   0x0000001f    /* Media sub-type */
#define IFM_IMASK   0xf0000000    /* Instance */
#define IFM_ISHIFT  28            /* Instance shift */
#define IFM_OMASK   0x0000ff00    /* Type specific options */
#define IFM_OSHIFT  8
#define IFM_MMASK   0x00070000    /* Mode */
#define IFM_MSHIFT  16            /* Mode shift */
#define IFM_GMASK   0x0ff00000    /* Global options */

#define MII_AUTONEG_LINK_CHANGE    1
#define MII_AUTONEG_LINK_NO_CHANGE 2


#define IFM_AVALID  0x00000001    /* Active bit valid */
#define IFM_ACTIVE  0x00000002    /* Interface attached to working net */

#define IFM_TYPE(x)         ((x) & IFM_NMASK)
#define IFM_SUBTYPE(x)      ((x) & IFM_TMASK)
#define IFM_TYPE_OPTIONS(x) ((x) & IFM_OMASK)
#define IFM_INST(x)         (((x) & IFM_IMASK) >> IFM_ISHIFT)
#define IFM_OPTIONS(x)      ((x) & (IFM_OMASK|IFM_GMASK))
#define IFM_MODE(x)         ((x) & IFM_MMASK)

#define MII_MAX_PHY_NUM      0x20    /* max number of attached PHYs */
#define MII_MAX_REG_NUM      0x20    /* max number of registers */

#define MII_CTRL_REG         0x0    /* Control Register */
#define MII_STAT_REG         0x1    /* Status Register */
#define MII_PHY_ID1_REG      0x2    /* PHY identifier 1 Register */
#define MII_PHY_ID2_REG      0x3    /* PHY identifier 2 Register */
#define MII_AN_ADS_REG       0x4    /* Auto-Negotiation   Advertisement Register */
#define MII_AN_PRTN_REG      0x5    /* Auto-Negotiation   partner ability Register */
#define MII_AN_EXP_REG       0x6    /* Auto-Negotiation   Expansion Register */
#define MII_AN_NEXT_REG      0x7    /* Auto-Negotiation   next-page transmit Register */
#define MII_AN_PRTN_NEXT_REG 0x8    /* Link partner received next page */
#define MII_MASSLA_CTRL_REG  0x9    /* MATER-SLAVE control register */
#define MII_MASSLA_STAT_REG  0xa    /* MATER-SLAVE status register */
#define MII_EXT_STAT_REG     0xf    /* Extented status register */

/* MII control register bit  */
#define MII_CR_1000       0x0040    /* 1 = 1000mb when   MII_CR_100 is also 1 */
#define MII_CR_COLL_TEST  0x0080    /* collision test */
#define MII_CR_FDX        0x0100    /* FDX =1, half duplex =0 */
#define MII_CR_RESTART    0x0200    /* restart auto negotiation */
#define MII_CR_ISOLATE    0x0400    /* isolate PHY from MII */
#define MII_CR_POWER_DOWN 0x0800    /* power down */
#define MII_CR_AUTO_EN    0x1000    /* auto-negotiation enable */
#define MII_CR_100        0x2000    /* 0 = 10mb, 1 = 100mb */
#define MII_CR_LOOPBACK   0x4000    /* 0 = normal, 1 = loopback */
#define MII_CR_RESET      0x8000    /* 0 = normal, 1 = PHY reset */
#define MII_CR_NORM_EN    0x0000    /* just enable the PHY */
#define MII_CR_DEF_0_MASK 0xca7f    /* they must return zero */
#define MII_CR_RES_MASK   0x003f    /* reserved bits,return zero */

/* MII Status register bit definitions */
#define MII_SR_LINK_STATUS    0x0004    /* link Status -- 1 = link */
#define MII_SR_AUTO_SEL       0x0008    /* auto speed select capable */
#define MII_SR_REMOTE_FAULT   0x0010    /* Remote fault detect */
#define MII_SR_AUTO_NEG       0x0020    /* auto negotiation complete */
#define MII_SR_EXT_STS        0x0100    /* extended sts in reg 15 */
#define MII_SR_T2_HALF_DPX    0x0200    /* 100baseT2 HD capable */
#define MII_SR_T2_FULL_DPX    0x0400    /* 100baseT2 FD capable */
#define MII_SR_10T_HALF_DPX   0x0800    /* 10BaseT HD capable */
#define MII_SR_10T_FULL_DPX   0x1000    /* 10BaseT FD capable */
#define MII_SR_TX_HALF_DPX    0x2000    /* TX HD capable */
#define MII_SR_TX_FULL_DPX    0x4000    /* TX FD capable */
#define MII_SR_T4             0x8000    /* T4 capable */
#define MII_SR_ABIL_MASK      0xff80    /* abilities mask */
#define MII_SR_EXT_CAP        0x0001    /* extended capabilities */
#define MII_SR_SPEED_SEL_MASK 0xf800    /* Mask to extract just speed capabilities from status register. */

/*  MII ID2 register bit mask */
#define MII_ID2_REVISON_MASK  0x000f
#define MII_ID2_MODE_MASK     0x03f0

/* MII AN advertisement Register bit definition */
#define MII_ANAR_10TX_HD      0x0020
#define MII_ANAR_10TX_FD      0x0040
#define MII_ANAR_100TX_HD     0x0080
#define MII_ANAR_100TX_FD     0x0100
#define MII_ANAR_100T_4       0x0200
#define MII_ANAR_PAUSE        0x0400
#define MII_ANAR_ASM_PAUSE    0x0800
#define MII_ANAR_REMORT_FAULT 0x2000
#define MII_ANAR_NEXT_PAGE    0x8000
#define MII_ANAR_PAUSE_MASK   0x0c00

/* MII Link Code word bit definitions */
#define MII_BP_FAULT 0x2000    /* remote fault */
#define MII_BP_ACK   0x4000    /* acknowledge */
#define MII_BP_NP    0x8000    /* nexp page is supported */

/* MII Next Page bit definitions */
#define MII_NP_TOGGLE 0x0800    /* toggle bit */
#define MII_NP_ACK2   0x1000    /* acknowledge two */
#define MII_NP_MSG    0x2000    /* message page */
#define MII_NP_ACK1   0x4000    /* acknowledge one */
#define MII_NP_NP     0x8000    /* nexp page will follow */

/* MII Expansion Register bit definitions */
#define MII_EXP_FAULT   0x0010    /* parallel detection fault */
#define MII_EXP_PRTN_NP 0x0008    /* link partner next-page able */
#define MII_EXP_LOC_NP  0x0004    /* local PHY next-page able */
#define MII_EXP_PR      0x0002    /* full page received */
#define MII_EXP_PRT_AN  0x0001    /* link partner auto negotiation able */

/* MII Master-Slave Control register bit definition */
#define MII_MASSLA_CTRL_1000T_HD   0x100
#define MII_MASSLA_CTRL_1000T_FD   0x200
#define MII_MASSLA_CTRL_PORT_TYPE  0x400
#define MII_MASSLA_CTRL_CONFIG_VAL 0x800
#define MII_MASSLA_CTRL_CONFIG_EN  0x1000

/* MII Master-Slave Status register bit definition */
#define MII_MASSLA_STAT_LP1000T_HD  0x400
#define MII_MASSLA_STAT_LP1000T_FD  0x800
#define MII_MASSLA_STAT_REMOTE_RCV  0x1000
#define MII_MASSLA_STAT_LOCAL_RCV   0x2000
#define MII_MASSLA_STAT_CONF_RES    0x4000
#define MII_MASSLA_STAT_CONF_FAULT  0x8000

/* MII Extented Status register bit definition */
#define MII_EXT_STAT_1000T_HD       0x1000
#define MII_EXT_STAT_1000T_FD       0x2000
#define MII_EXT_STAT_1000X_HD       0x4000
#define MII_EXT_STAT_1000X_FD       0x8000


#define MII45_MMD_RSVD      0   /* Reserved */
#define MII45_MMD_PMAPMD    1   /* Physical Media Attachment/Dependent */
#define MII45_MMD_WIS       2   /* WAN Interface Sublayer */
#define MII45_MMD_PCS       3   /* Physical Coding Sublayer */
#define MII45_MMD_PHYXS     4   /* PHY Extenter Sublayer */
#define MII45_MMD_DTEXS     5   /* DTE Extenter Sublayer */
#define MII45_MMD_TC        6   /* Transmission Convergence */
#define MII45_MMD_AUTONEG   7   /* Autonegotiation */
#define MII45_MMD_SPMA1     8   /* Separated PMA (1) */
#define MII45_MMD_SPMA2     9   /* Separated PMA (2) */
#define MII45_MMD_SPMA3     10  /* Separated PMA (3) */
#define MII45_MMD_SPMA4     11  /* Separated PMA (4) */
#define MII45_MMD_CL22EXT   29  /* Clause 22 extention */
#define MII45_MMD_VSPEC1    30  /* Vendor-specific 1 */
#define MII45_MMD_VSPEC2    32  /* Vendor-specific 2 */

#define MII45_PCS_CTL0      0x0000  /* PCS control 1 */
#define MII45_PCS_STS0      0x0001  /* PCS status 1 */
#define MII45_PCS_PKGID0    0x0002  /* PCS Package ID1 */
#define MII45_PCS_PKGID1    0x0003  /* PCS Package ID2 */
#define MII45_PCS_SPEED     0x0004  /* PCS speed ability */
#define MII45_PCS_CTL1      0x0007  /* PMA/PMD control 2 */
#define MII45_PCS_STS1      0x0008  /* PMA/PMD status 2 */


#define IDR2_MODEL 0x03f0    /* vendor model */
#define IDR2_REV   0x000f    /* vendor revision */

#define MII_MODEL(id2) (((id2) & IDR2_MODEL) >> 4)
#define MII_REV(id2)   ((id2) & IDR2_REV)

#define PHY_DEV_ORPHAN_STATUS 0xBABEBABE
#define PHY_DEV_NORMAL_STATUS 0xFEEDCAFE

#define PHY_ADDR_IS_PREASGN(val)      (val & BIT(0))
#define PHY_ADDR_MASK                 (0x1F)
#define PHY_ACQUIRE_PREASGN_ADDR(val) ((val >> 16) & PHY_ADDR_MASK)
#define PHY_ASSIGN_ADDR(addr)         ((addr & PHY_ADDR_MASK) << 16) || (BIT(0))

/*
 * Shared media sub-types
 */
#define IFM_AUTO   0    /* Autoselect best media */
#define IFM_MANUAL 1    /* Jumper/dipswitch selects media */
#define IFM_NONE   2    /* Deselect all media */

#define IFM_ETHER           0x00000020
#define IFM_10_T            3             /* 10BaseT - RJ45 */
#define IFM_10_2            4             /* 10Base2 - Thinnet */
#define IFM_10_5            5             /* 10Base5 - AUI */
#define IFM_100_TX          6             /* 100BaseTX - RJ45 */
#define IFM_100_FX          7             /* 100BaseFX - Fiber */
#define IFM_100_T4          8             /* 100BaseT4 - 4 pair cat 3 */
#define IFM_100_VG          9             /* 100VG-AnyLAN */
#define IFM_100_T2          10            /* 100BaseT2 */
#define IFM_1000_SX         11            /* 1000BaseSX - multi-mode fiber */
#define IFM_10_STP          12            /* 10BaseT over shielded TP */
#define IFM_10_FL           13            /* 10BaseFL - Fiber */
#define IFM_1000_LX         14            /* 1000baseLX - single-mode fiber */
#define IFM_1000_CX         15            /* 1000baseCX - 150ohm STP */
#define IFM_1000_T          16            /* 1000baseT - 4 pair cat 5 */
#define IFM_HPNA_1          17            /* HomePNA 1.0 (1Mb/s) */
#define IFM_10G_LR          18            /* 10GBase-LR 1310nm Single-mode */
#define IFM_10G_SR          19            /* 10GBase-SR 850nm Multi-mode */
#define IFM_10G_CX4         20            /* 10GBase CX4 copper */
#define IFM_2500_SX         21            /* 2500BaseSX - multi-mode fiber */
#define IFM_10G_TWINAX      22            /* 10GBase Twinax copper */
#define IFM_10G_TWINAX_LONG 23            /* 10GBase Twinax Long copper */
#define IFM_10G_LRM         24            /* 10GBase-LRM 850nm Multi-mode */
#define IFM_UNKNOWN         25            /* New media types that have not been defined yet */
#define IFM_10G_T           26            /* 10GBase-T */

#define IFM_FDX   0x00100000    /* Force full duplex */
#define IFM_HDX   0x00200000    /* Force half duplex */
#define IFM_FLAG0 0x01000000    /* Driver defined flag */
#define IFM_FLAG1 0x02000000    /* Driver defined flag */
#define IFM_FLAG2 0x04000000    /* Driver defined flag */
#define IFM_LOOP  0x08000000    /* Put hardware in loopback */


typedef struct phy_dev PHY_DEV;

typedef struct phy_dev_funcs
{
    int (*probe) (PHY_DEV *phy_dev);
    int (*init) (PHY_DEV *phydev);
    int (*uninit) (PHY_DEV *phydev);
    int (*mode_set) (PHY_DEV *phy_dev, unsigned int mode);
    int (*mode_get) (PHY_DEV *phy_dev, unsigned int *mode, unsigned int *status);
} PHY_DRV_FUNCS;

typedef struct phy_driver_info
{
    struct list_node node;
    char *name;              /* 驱动名称 */
    char *desc;              /* 驱动描述 */
    PHY_DRV_FUNCS *phy_func; /* 功能表 */
    unsigned int ref_cnt;
} PHY_DRV_INFO;

typedef struct phy_hardware_info
{
    unsigned short phy_id1;
    unsigned short phy_id2;
    unsigned char phy_addr;
    MDIO_TYPE mdio_type;       /* mdio类型 */
    unsigned int phandle;      /* 设备树节点中phandle的值，用于与MAC匹配 */
    unsigned int pre_asgn;     /* PHY地址由MAC预设 */
} PHY_HW_INFO;

typedef struct phy_id_entry
{
    unsigned short id1;
    unsigned short id2;
} PHY_ID_ENTRY;

typedef struct media_list
{
    unsigned int default_media;
    unsigned int media_num;
    unsigned int media[1];
} MEDIA_LIST;

struct mii_ioctl_data
{
    unsigned short phy_id;    /* PHY芯片地址 */
    unsigned short reg_num;  /* 寄存器地址 */
    unsigned short val_in;          /* 写入值 */
    unsigned short val_out;         /* 读出值 */
};

typedef struct phy_dev
{
    struct list_node node;
    struct device_node *device_node;

    int unit;
    int init_flag;                /* 初始化后值为0xFEEDCAFE */;
    ETH_DEV *ethdev;              /* 对应的MAC设备 */

    MDIO_PARENT_TYPE parent_type; /* 父设备类型 */
    void *parent_dev;             /* 父设备 */
    MDIO_FUNCS mdio_func;         /* 功能表 */

    unsigned short status;        /* 由PHY监控任务更新的PHY status寄存器的值 */
    unsigned short auto_status;   /* 仅clause45使用 */
    PHY_HW_INFO hw_info;          /* PHY设备信息 */
    PHY_DRV_INFO *phy_drv;

    MEDIA_LIST *media_list;
} PHY_DEV;

int mdio_read (PHY_DEV *phydev, unsigned char phy_reg, unsigned short *data);
int mdio_write (PHY_DEV *phydev, unsigned char phy_reg, unsigned short data);
int mdio45_read (PHY_DEV *phydev, unsigned char dev_addr, unsigned char phy_reg, unsigned short *data);
int mdio45_write (PHY_DEV *phydev, unsigned char dev_addr, unsigned char phy_reg, unsigned short data);
int phy_mode_set (PHY_DEV *phydev, unsigned int mode);
int phy_mode_get (PHY_DEV *phydev, unsigned int *mode, unsigned int *status);
int phy_media_update_notify (PHY_DEV *phydev);
int phy_default_media_set (PHY_DEV *phydev, unsigned int media);
int phy_default_media_get(PHY_DEV *phydev, unsigned int *media);
int phy_media_list_add (PHY_DEV *phydev, unsigned int media);
int phy_media_list_del (PHY_DEV *phydev, unsigned int media);
int phy_id_match (PHY_DEV *phydev, const PHY_ID_ENTRY *id_table, PHY_ID_ENTRY **id_entry);
int phy_drv_register (PHY_DRV_INFO *drv);
int phy_device_init (void *parent_dev, MDIO_PARENT_TYPE parent_dev_type, MDIO_TYPE mdio_type, MDIO_READ_FUNC mdio_read_func, MDIO_WRITE_FUNC mdio_write_func,
                            MDIO45_READ_FUNC mdio45_read_func, MDIO45_WRITE_FUNC mdio45_write_func);
int generic_phy_init (PHY_DEV *phydev);
int generic_phy_mode_set (PHY_DEV *phydev , unsigned int mode);

#ifdef __cplusplus
}
#endif

#endif /* __PHY_DEV_H__ */
