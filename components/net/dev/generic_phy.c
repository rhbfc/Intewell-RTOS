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

#include <net/phy_dev.h>
#include <driver/of.h>

#define GENERIC_PHY_DRIVER "Generic PHY"

#define MII_MODEL_REALTEK_RTL8211E      0x0011
#define MII_MODEL_REALTEK_RTL8211FD_VD  0x0007
#define MII_REV_RTL8211E                0x5
#define MII_REV_RTL8211F                0x6
#define MII_REV_RTL8211FD_VD            0x8

/* YT8521相关定义 */
#define YT8521_PHY_ID1_REG              0x0
#define YT8521_PHY_ID2_REG              0x11A
#define YT8521_SPEC_STAT_REG            0x11
#define YT8521_SPEC_STAT_FD             0x2000
#define YT8521_SPEC_STAT_SPEED_MASK     0xC000
#define YT8521_SPEC_STAT_1000M          0x8000
#define YT8521_SPEC_STAT_100M           0x4000
#define YT8521_SPEC_STAT_10M            0x0000


#define MII_MMD_MACR                    0x000D
#define MII_MMD_MAADR                   0x000E
#define MII_MMD_ACCESS_CONTROL_ADDR     0x0000
#define MII_MMD_ACCESS_CONTROL_DATA     0x4000
#define MII_MMD_DEV_ADDR_7              0x7
#define MII_EEE_ADV_OFFSET              0x003C

/* Realtek 8211x EEE Advertisement Register */
#define MII_EEE_ADV_1000M_EN    0x4
#define MII_EEE_ADV_100M_EN     0x2

int generic_phy_probe (PHY_DEV *phydev);
int generic_phy_init (PHY_DEV *phydev);
int generic_phy_mode_get (PHY_DEV *phydev, unsigned int *mode, unsigned int *status);
int generic_phy_mode_set (PHY_DEV *phydev , unsigned int mode);

static PHY_DRV_FUNCS generic_phy_func =
{
    .probe = generic_phy_probe,
    .init = generic_phy_init,
    .uninit = NULL,
    .mode_get = generic_phy_mode_get,
    .mode_set = generic_phy_mode_set
};

PHY_DRV_INFO generic_phy_drv =
{
    {NULL, NULL},
    GENERIC_PHY_DRIVER,
    "Generic PHY Driver",
    &generic_phy_func,
    0
};

static void rtl_mmd_reg_write (PHY_DEV *phydev, unsigned short devAddr, unsigned short offset, unsigned short miiVal)
{
    unsigned short tmp;

    mdio_write (phydev, MII_MMD_MACR, MII_MMD_ACCESS_CONTROL_ADDR | devAddr);
    mdio_write (phydev, MII_MMD_MAADR, offset);

    mdio_write (phydev, MII_MMD_MACR, MII_MMD_ACCESS_CONTROL_DATA | devAddr);

    mdio_read (phydev, MII_MMD_MAADR, &tmp);

    mdio_write (phydev, MII_MMD_MAADR, miiVal);
}

static unsigned short rtl_mmd_reg_read (PHY_DEV *phydev, unsigned short devAddr, unsigned short offset)
{
    unsigned short miiVal;

    mdio_write (phydev, MII_MMD_MACR, MII_MMD_ACCESS_CONTROL_ADDR | devAddr);

    mdio_write (phydev, MII_MMD_MAADR, offset);

    mdio_write (phydev, MII_MMD_MACR, MII_MMD_ACCESS_CONTROL_DATA | devAddr);

    mdio_read (phydev, MII_MMD_MAADR, &miiVal);

    return miiVal;
}

static void rtl_lpi_disable (PHY_DEV *phydev)
{
    unsigned short miiVal;

    miiVal = rtl_mmd_reg_read (phydev, MII_MMD_DEV_ADDR_7, MII_EEE_ADV_OFFSET);

    miiVal &= (unsigned short)(~(MII_EEE_ADV_1000M_EN | MII_EEE_ADV_100M_EN));
    rtl_mmd_reg_write (phydev, MII_MMD_DEV_ADDR_7, MII_EEE_ADV_OFFSET, miiVal);
}

static void gen_init (PHY_DEV *phydev)
{
    unsigned short status;
    unsigned short control;
    unsigned short data;
    int i;
    int mode, ver;

    mdio_read (phydev, MII_STAT_REG, &status);

    data = MII_CR_POWER_DOWN;
    mdio_write (phydev, MII_CTRL_REG, data);

    data = 0;
    mdio_write (phydev, MII_CTRL_REG, data);

    data = MII_CR_RESET;
    mdio_write (phydev, MII_CTRL_REG, data);

    for (i = 0; i < 1000; i++)
    {
        mdio_read (phydev, MII_CTRL_REG, &control);
        if (!(control & MII_CR_RESET))
        {
            break;
        }
    }

    if (status & MII_SR_EXT_STS)
    {
        data = MII_MASSLA_CTRL_1000T_FD | MII_MASSLA_CTRL_1000T_HD;
        mdio_write (phydev, MII_MASSLA_CTRL_REG, data);
    }

    control = MII_CR_AUTO_EN | MII_CR_RESTART;

    mdio_write (phydev, MII_CTRL_REG, control);

    mode = MII_MODEL (phydev->hw_info.phy_id2);
    ver = MII_REV (phydev->hw_info.phy_id2);

    /* 如果是Reaktek 8211系列PHY芯片，则关闭其LPI模式 */
    if (((mode == MII_MODEL_REALTEK_RTL8211FD_VD) && (ver == MII_REV_RTL8211FD_VD)) ||
        ((mode == MII_MODEL_REALTEK_RTL8211E) && (ver == MII_REV_RTL8211E)) ||
        ((mode == MII_MODEL_REALTEK_RTL8211E) && (ver == MII_REV_RTL8211F)))
    {
        rtl_lpi_disable (phydev);
    }

    return;
}

int generic_phy_probe (PHY_DEV *phydev)
{
    /* 通用PHY驱动无需检查匹配PHY设备的ID */

    return OK;
}

int generic_phy_init (PHY_DEV *phydev)
{
    MEDIA_LIST *media_list;
    unsigned short status;
    unsigned int cfg_speed;
    unsigned int cfg_duplex = 1;

    media_list = (MEDIA_LIST *) calloc (1, sizeof(MEDIA_LIST));

    if (media_list == NULL)
    {
        return (-1);
    }

    phydev->media_list = media_list;

    mdio_read (phydev, MII_STAT_REG, &status);

    if (status & MII_SR_EXT_STS)
    {
        phy_media_list_add (phydev, IFM_ETHER | IFM_1000_T);
        phy_media_list_add (phydev, IFM_ETHER | IFM_1000_T | IFM_FDX);
    }

    if (status & MII_SR_TX_HALF_DPX)
    {
        phy_media_list_add (phydev, IFM_ETHER | IFM_100_TX);
    }

    if (status & MII_SR_TX_FULL_DPX)
    {
        phy_media_list_add (phydev, IFM_ETHER | IFM_100_TX | IFM_FDX);
    }

    if (status & MII_SR_10T_HALF_DPX)
    {
        phy_media_list_add (phydev, IFM_ETHER | IFM_10_T);
    }

    if (status & MII_SR_10T_FULL_DPX)
    {
        phy_media_list_add (phydev, IFM_ETHER | IFM_10_T | IFM_FDX);
    }

    if (status & MII_SR_AUTO_SEL)
    {
        phy_media_list_add (phydev, IFM_ETHER | IFM_AUTO);
    }

    if (phydev->device_node != NULL && of_find_property(phydev->device_node, "no-autoneg", NULL) && 
        of_property_read_u32 (phydev->device_node, "speed", &cfg_speed) == OK)
    {
        of_property_read_u32 (phydev->device_node, "duplex-full", &cfg_duplex);

        switch(cfg_speed)
        {
            case 1000:
                phy_default_media_set (phydev, IFM_ETHER | IFM_1000_T | IFM_FDX);
                break;
            case 100:
                if (cfg_duplex == 0)
                {
                    phy_default_media_set (phydev, IFM_ETHER | IFM_100_TX);
                }
                else
                {
                    phy_default_media_set (phydev, IFM_ETHER | IFM_100_TX | IFM_FDX);
                }
                break;
            case 10:
                if (cfg_duplex == 0)
                {
                    phy_default_media_set (phydev, IFM_ETHER | IFM_10_T);
                }
                else
                {
                    phy_default_media_set (phydev, IFM_ETHER | IFM_10_T | IFM_FDX);
                }
                break;
            default:
                break;
        }
    }
    else
    {
        phy_default_media_set (phydev, IFM_ETHER | IFM_AUTO);
    }
    
    if (phydev->mdio_func.skip_gen_init == 0)
    {
        gen_init (phydev);
    }
    else if((phydev->mdio_func.skip_gen_init == 1) && (phydev->mdio_func.custom_init != NULL))
    {
        phydev->mdio_func.custom_init (phydev);
    }

    return OK;
}

int generic_phy_mode_get (PHY_DEV *phydev, unsigned int *mode, unsigned int *status)
{
    unsigned short mii_status;
    unsigned short mii_control;
    unsigned short mii_anadver;
    unsigned short mii_lpbpa;
    unsigned short gmii_massl_control = 0;
    unsigned short gmii_massl_status = 0;
    unsigned short anlpar;
    unsigned short yt8521_sts;

    *mode = IFM_ETHER;
    *status = IFM_AVALID;

    (void) mdio_read (phydev, MII_STAT_REG, &mii_status);

    (void) mdio_read (phydev, MII_STAT_REG, &mii_status);

    if (!(mii_status & MII_SR_LINK_STATUS) || (mii_status == 0xFFFF))
    {
        *mode |= IFM_NONE;

        return (OK);
    }

    *status |= IFM_ACTIVE;

    /* 如果PHY为YT8521系列，则从17号寄存器获取其状态信息 */
    if (phydev->hw_info.phy_id1 == YT8521_PHY_ID1_REG && phydev->hw_info.phy_id2 == YT8521_PHY_ID2_REG)
    {
        (void) mdio_read (phydev, YT8521_SPEC_STAT_REG, &yt8521_sts);
        
        switch (yt8521_sts & YT8521_SPEC_STAT_SPEED_MASK)
        {
            case YT8521_SPEC_STAT_1000M:
                *mode |= (IFM_1000_T | IFM_FDX);
                break;
            case YT8521_SPEC_STAT_100M:
                if (yt8521_sts & YT8521_SPEC_STAT_FD)
                {
                    *mode |= (IFM_100_TX | IFM_FDX);
                }
                else
                {
                    *mode |= (IFM_100_TX | IFM_HDX);
                }
                break;
            case YT8521_SPEC_STAT_10M:
                if (yt8521_sts & YT8521_SPEC_STAT_FD)
                {
                    *mode |= (IFM_10_T | IFM_FDX);
                }
                else
                {
                    *mode |= (IFM_10_T | IFM_HDX);
                }
                break;
            default:
                *mode |= IFM_NONE;
                break;
        }
    }
    else
    {
        (void) mdio_read (phydev, MII_CTRL_REG, &mii_control);
        (void) mdio_read (phydev, MII_AN_ADS_REG, &mii_anadver);
        (void) mdio_read (phydev, MII_AN_PRTN_REG, &mii_lpbpa);

        if (mii_status & MII_SR_EXT_STS)
        {
            (void) mdio_read (phydev, MII_MASSLA_CTRL_REG, &gmii_massl_control);
            (void) mdio_read (phydev, MII_MASSLA_STAT_REG, &gmii_massl_status);
        }

        if (mii_control & MII_CR_AUTO_EN)
        {
            anlpar = mii_anadver & mii_lpbpa;
            if (gmii_massl_control & MII_MASSLA_CTRL_1000T_FD && gmii_massl_status & MII_MASSLA_STAT_LP1000T_FD)
            {
                *mode |= IFM_1000_T | IFM_FDX;
            }
            else if (gmii_massl_control & MII_MASSLA_CTRL_1000T_HD && gmii_massl_status & MII_MASSLA_STAT_LP1000T_HD)
            {
                *mode |= IFM_1000_T | IFM_HDX;
            }
            else if (anlpar & MII_ANAR_100TX_FD)
            {
                *mode |= IFM_100_TX | IFM_FDX;
            }
            else if (anlpar & MII_ANAR_100TX_HD)
            {
                *mode |= IFM_100_TX | IFM_HDX;
            }
            else if (anlpar & MII_ANAR_10TX_FD)
            {
                *mode |= IFM_10_T | IFM_FDX;
            }
            else if (anlpar & MII_ANAR_10TX_HD)
            {
                *mode |= IFM_10_T | IFM_HDX;
            }
            else
            {
                *mode |= IFM_NONE;
            }
        }
        else
        {
            if (mii_control & MII_CR_FDX)
            {
                *mode |= IFM_FDX;
            }
            else
            {
                *mode |= IFM_HDX;
            }

            if ((mii_control & MII_CR_1000) == MII_CR_1000)
            {
                *mode |= IFM_1000_T;
            }
            else if (mii_control & MII_CR_100)
            {
                *mode |= IFM_100_TX;
            }
            else
            {
                *mode |= IFM_10_T;
            }
        }
    }
    return (OK);
}

int generic_phy_mode_set (PHY_DEV *phydev , unsigned int mode)
{
    unsigned short data;
    unsigned short mii_anadver = 0;
    unsigned short gmii_massl_control = 0;
    unsigned short mii_control = 0;
    unsigned short mii_status;

    (void) mdio_read (phydev, MII_STAT_REG, &mii_status);

    switch (IFM_SUBTYPE(mode))
    {
        case IFM_AUTO:
            mii_anadver = MII_ANAR_10TX_HD | MII_ANAR_10TX_FD | MII_ANAR_100TX_HD | MII_ANAR_100TX_FD;
            if (mii_status & MII_SR_EXT_STS)
            {
                gmii_massl_control = MII_MASSLA_CTRL_1000T_FD | MII_MASSLA_CTRL_1000T_HD;
            }
            mii_control = MII_CR_AUTO_EN | MII_CR_RESTART;
            break;

        case IFM_1000_T:
            if (!(mii_status & MII_SR_EXT_STS))
            {
                return(-1);
            }
            gmii_massl_control = MII_MASSLA_CTRL_1000T_FD | MII_MASSLA_CTRL_1000T_HD;
            mii_anadver = 0;
            mii_control = MII_CR_AUTO_EN | MII_CR_RESTART;
            break;

        case IFM_100_TX:
            mii_control = MII_CR_100 | MII_CR_AUTO_EN | MII_CR_RESTART;
            if ((mode & IFM_GMASK) == IFM_FDX)
            {
                mii_anadver = MII_ANAR_100TX_FD;
                mii_control |= MII_CR_FDX;
            }
            else
            {
                mii_anadver = MII_ANAR_100TX_HD;
            }
            gmii_massl_control = 0;
            break;

        case IFM_10_T:
            mii_control = MII_CR_AUTO_EN | MII_CR_RESTART;
            if ((mode & IFM_GMASK) == IFM_FDX)
            {
                mii_anadver = MII_ANAR_10TX_FD;
                mii_control |= MII_CR_FDX;
            }
            else
            {
                mii_anadver = MII_ANAR_10TX_HD;
            }
            gmii_massl_control = 0;
            break;

        default:
            return (-1);
    }

    if (phydev->mdio_func.skip_gen_init == 0)
    {
        gen_init (phydev);
    }
    else if((phydev->mdio_func.skip_gen_init == 1) && (phydev->mdio_func.custom_init != NULL))
    {
        phydev->mdio_func.custom_init (phydev);
    }

    mdio_read (phydev, MII_AN_ADS_REG, &data);
    data &= (unsigned short)~(MII_ANAR_10TX_HD | MII_ANAR_10TX_FD | MII_ANAR_100TX_HD | MII_ANAR_100TX_FD);
    data |= mii_anadver;
    mdio_write (phydev, MII_AN_ADS_REG, data);

    if (mii_status & MII_SR_EXT_STS)
    {
        mdio_read (phydev, MII_MASSLA_CTRL_REG, &data);
        data &= (unsigned short)~(MII_MASSLA_CTRL_1000T_HD | MII_MASSLA_CTRL_1000T_FD);
        data |= gmii_massl_control;
        mdio_write (phydev, MII_MASSLA_CTRL_REG, data);
    }

    mdio_read (phydev, MII_CTRL_REG, &data);
    data &= (unsigned short)~(MII_CR_FDX | MII_CR_100 | MII_CR_AUTO_EN | MII_CR_RESTART);
    data |= mii_control;
    mdio_write (phydev, MII_CTRL_REG, data);

    return(OK);
}
