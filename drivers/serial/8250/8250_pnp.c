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

#ifdef CONFIG_ACPI
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <driver/device.h>
#include <driver/driver.h>
#include <driver/platform.h>
#include <driver/serial/serial_8250.h>
#include <driver/serial/serial_core.h>
#include <driver/serial/serial_reg.h>
#include <io.h>
#include <system/kconfig.h>
#include <ttosMM.h>
#include <ttos_init.h>
#include <ttos_pic.h>

#define KLOG_TAG "PNP8250"
#include <klog.h>

#define UNKNOWN_DEV 0x3000
#define CIR_PORT 0x0800

static const struct acpi_device_id i8250_acpi_ids[] = {
    /* Archtek America Corp. */
    /* Archtek SmartLink Modem 3334BT Plug & Play */
    {"AAC000F", 0},
    /* Anchor Datacomm BV */
    /* SXPro 144 External Data Fax Modem Plug & Play */
    {"ADC0001", 0},
    /* SXPro 288 External Data Fax Modem Plug & Play */
    {"ADC0002", 0},
    /* PROLiNK 1456VH ISA PnP K56flex Fax Modem */
    {"AEI0250", 0},
    /* Actiontec ISA PNP 56K X2 Fax Modem */
    {"AEI1240", 0},
    /* Rockwell 56K ACF II Fax+Data+Voice Modem */
    {"AKY1021", 0 /*SPCI_FL_NO_SHIRQ*/},
    /*
     * ALi Fast Infrared Controller
     * Native driver (ali-ircc) is broken so at least
     * it can be used with irtty-sir.
     */
    {"ALI5123", 0},
    /* AZT3005 PnP SOUND DEVICE */
    {"AZT4001", 0},
    /* Best Data Products Inc. Smart One 336F PnP Modem */
    {"BDP3336", 0},
    /*  Boca Research */
    /* Boca Complete Ofc Communicator 14.4 Data-FAX */
    {"BRI0A49", 0},
    /* Boca Research 33,600 ACF Modem */
    {"BRI1400", 0},
    /* Boca 33.6 Kbps Internal FD34FSVD */
    {"BRI3400", 0},
    /* Computer Peripherals Inc */
    /* EuroViVa CommCenter-33.6 SP PnP */
    {"CPI4050", 0},
    /* Creative Labs */
    /* Creative Labs Phone Blaster 28.8 DSVD PnP Voice */
    {"CTL3001", 0},
    /* Creative Labs Modem Blaster 28.8 DSVD PnP Voice */
    {"CTL3011", 0},
    /* Davicom ISA 33.6K Modem */
    {"DAV0336", 0},
    /* Creative */
    /* Creative Modem Blaster Flash56 DI5601-1 */
    {"DMB1032", 0},
    /* Creative Modem Blaster V.90 DI5660 */
    {"DMB2001", 0},
    /* E-Tech */
    /* E-Tech CyberBULLET PC56RVP */
    {"ETT0002", 0},
    /* FUJITSU */
    /* Fujitsu 33600 PnP-I2 R Plug & Play */
    {"FUJ0202", 0},
    /* Fujitsu FMV-FX431 Plug & Play */
    {"FUJ0205", 0},
    /* Fujitsu 33600 PnP-I4 R Plug & Play */
    {"FUJ0206", 0},
    /* Fujitsu Fax Voice 33600 PNP-I5 R Plug & Play */
    {"FUJ0209", 0},
    /* Archtek America Corp. */
    /* Archtek SmartLink Modem 3334BT Plug & Play */
    {"GVC000F", 0},
    /* Archtek SmartLink Modem 3334BRV 33.6K Data Fax Voice */
    {"GVC0303", 0},
    /* Hayes */
    /* Hayes Optima 288 V.34-V.FC + FAX + Voice Plug & Play */
    {"HAY0001", 0},
    /* Hayes Optima 336 V.34 + FAX + Voice PnP */
    {"HAY000C", 0},
    /* Hayes Optima 336B V.34 + FAX + Voice PnP */
    {"HAY000D", 0},
    /* Hayes Accura 56K Ext Fax Modem PnP */
    {"HAY5670", 0},
    /* Hayes Accura 56K Ext Fax Modem PnP */
    {"HAY5674", 0},
    /* Hayes Accura 56K Fax Modem PnP */
    {"HAY5675", 0},
    /* Hayes 288, V.34 + FAX */
    {"HAYF000", 0},
    /* Hayes Optima 288 V.34 + FAX + Voice, Plug & Play */
    {"HAYF001", 0},
    /* IBM */
    /* IBM Thinkpad 701 Internal Modem Voice */
    {"IBM0033", 0},
    /* Intermec */
    /* Intermec CV60 touchscreen port */
    {"PNP4972", 0},
    /* Intertex */
    /* Intertex 28k8 33k6 Voice EXT PnP */
    {"IXDC801", 0},
    /* Intertex 33k6 56k Voice EXT PnP */
    {"IXDC901", 0},
    /* Intertex 28k8 33k6 Voice SP EXT PnP */
    {"IXDD801", 0},
    /* Intertex 33k6 56k Voice SP EXT PnP */
    {"IXDD901", 0},
    /* Intertex 28k8 33k6 Voice SP INT PnP */
    {"IXDF401", 0},
    /* Intertex 28k8 33k6 Voice SP EXT PnP */
    {"IXDF801", 0},
    /* Intertex 33k6 56k Voice SP EXT PnP */
    {"IXDF901", 0},
    /* Kortex International */
    /* KORTEX 28800 Externe PnP */
    {"KOR4522", 0},
    /* KXPro 33.6 Vocal ASVD PnP */
    {"KORF661", 0},
    /* Lasat */
    /* LASAT Internet 33600 PnP */
    {"LAS4040", 0},
    /* Lasat Safire 560 PnP */
    {"LAS4540", 0},
    /* Lasat Safire 336  PnP */
    {"LAS5440", 0},
    /* Microcom, Inc. */
    /* Microcom TravelPorte FAST V.34 Plug & Play */
    {"MNP0281", 0},
    /* Microcom DeskPorte V.34 FAST or FAST+ Plug & Play */
    {"MNP0336", 0},
    /* Microcom DeskPorte FAST EP 28.8 Plug & Play */
    {"MNP0339", 0},
    /* Microcom DeskPorte 28.8P Plug & Play */
    {"MNP0342", 0},
    /* Microcom DeskPorte FAST ES 28.8 Plug & Play */
    {"MNP0500", 0},
    /* Microcom DeskPorte FAST ES 28.8 Plug & Play */
    {"MNP0501", 0},
    /* Microcom DeskPorte 28.8S Internal Plug & Play */
    {"MNP0502", 0},
    /* Motorola */
    /* Motorola BitSURFR Plug & Play */
    {"MOT1105", 0},
    /* Motorola TA210 Plug & Play */
    {"MOT1111", 0},
    /* Motorola HMTA 200 (ISDN) Plug & Play */
    {"MOT1114", 0},
    /* Motorola BitSURFR Plug & Play */
    {"MOT1115", 0},
    /* Motorola Lifestyle 28.8 Internal */
    {"MOT1190", 0},
    /* Motorola V.3400 Plug & Play */
    {"MOT1501", 0},
    /* Motorola Lifestyle 28.8 V.34 Plug & Play */
    {"MOT1502", 0},
    /* Motorola Power 28.8 V.34 Plug & Play */
    {"MOT1505", 0},
    /* Motorola ModemSURFR External 28.8 Plug & Play */
    {"MOT1509", 0},
    /* Motorola Premier 33.6 Desktop Plug & Play */
    {"MOT150A", 0},
    /* Motorola VoiceSURFR 56K External PnP */
    {"MOT150F", 0},
    /* Motorola ModemSURFR 56K External PnP */
    {"MOT1510", 0},
    /* Motorola ModemSURFR 56K Internal PnP */
    {"MOT1550", 0},
    /* Motorola ModemSURFR Internal 28.8 Plug & Play */
    {"MOT1560", 0},
    /* Motorola Premier 33.6 Internal Plug & Play */
    {"MOT1580", 0},
    /* Motorola OnlineSURFR 28.8 Internal Plug & Play */
    {"MOT15B0", 0},
    /* Motorola VoiceSURFR 56K Internal PnP */
    {"MOT15F0", 0},
    /* Com 1 */
    /*  Deskline K56 Phone System PnP */
    {"MVX00A1", 0},
    /* PC Rider K56 Phone System PnP */
    {"MVX00F2", 0},
    /* NEC 98NOTE SPEAKER PHONE FAX MODEM(33600bps) */
    {"nEC8241", 0},
    /* Pace 56 Voice Internal Plug & Play Modem */
    {"PMC2430", 0},
    /* Generic */
    /* Generic standard PC COM port	 */
    {"PNP0500", 0},
    /* Generic 16550A-compatible COM port */
    {"PNP0501", 0},
    /* Compaq 14400 Modem */
    {"PNPC000", 0},
    /* Compaq 2400/9600 Modem */
    {"PNPC001", 0},
    /* Dial-Up Networking Serial Cable between 2 PCs */
    {"PNPC031", 0},
    /* Dial-Up Networking Parallel Cable between 2 PCs */
    {"PNPC032", 0},
    /* Standard 9600 bps Modem */
    {"PNPC100", 0},
    /* Standard 14400 bps Modem */
    {"PNPC101", 0},
    /*  Standard 28800 bps Modem*/
    {"PNPC102", 0},
    /*  Standard Modem*/
    {"PNPC103", 0},
    /*  Standard 9600 bps Modem*/
    {"PNPC104", 0},
    /*  Standard 14400 bps Modem*/
    {"PNPC105", 0},
    /*  Standard 28800 bps Modem*/
    {"PNPC106", 0},
    /*  Standard Modem */
    {"PNPC107", 0},
    /* Standard 9600 bps Modem */
    {"PNPC108", 0},
    /* Standard 14400 bps Modem */
    {"PNPC109", 0},
    /* Standard 28800 bps Modem */
    {"PNPC10A", 0},
    /* Standard Modem */
    {"PNPC10B", 0},
    /* Standard 9600 bps Modem */
    {"PNPC10C", 0},
    /* Standard 14400 bps Modem */
    {"PNPC10D", 0},
    /* Standard 28800 bps Modem */
    {"PNPC10E", 0},
    /* Standard Modem */
    {"PNPC10F", 0},
    /* Standard PCMCIA Card Modem */
    {"PNP2000", 0},
    /* Rockwell */
    /* Modular Technology */
    /* Rockwell 33.6 DPF Internal PnP */
    /* Modular Technology 33.6 Internal PnP */
    {"ROK0030", 0},
    /* Kortex International */
    /* KORTEX 14400 Externe PnP */
    {"ROK0100", 0},
    /* Rockwell 28.8 */
    {"ROK4120", 0},
    /* Viking Components, Inc */
    /* Viking 28.8 INTERNAL Fax+Data+Voice PnP */
    {"ROK4920", 0},
    /* Rockwell */
    /* British Telecom */
    /* Modular Technology */
    /* Rockwell 33.6 DPF External PnP */
    /* BT Prologue 33.6 External PnP */
    /* Modular Technology 33.6 External PnP */
    {"RSS00A0", 0},
    /* Viking 56K FAX INT */
    {"RSS0262", 0},
    /* K56 par,VV,Voice,Speakphone,AudioSpan,PnP */
    {"RSS0250", 0},
    /* SupraExpress 28.8 Data/Fax PnP modem */
    {"SUP1310", 0},
    /* SupraExpress 336i PnP Voice Modem */
    {"SUP1381", 0},
    /* SupraExpress 33.6 Data/Fax PnP modem */
    {"SUP1421", 0},
    /* SupraExpress 33.6 Data/Fax PnP modem */
    {"SUP1590", 0},
    /* SupraExpress 336i Sp ASVD */
    {"SUP1620", 0},
    /* SupraExpress 33.6 Data/Fax PnP modem */
    {"SUP1760", 0},
    /* SupraExpress 56i Sp Intl */
    {"SUP2171", 0},
    /* Phoebe Micro */
    /* Phoebe Micro 33.6 Data Fax 1433VQH Plug & Play */
    {"TEX0011", 0},
    /* Archtek America Corp. */
    /* Archtek SmartLink Modem 3334BT Plug & Play */
    {"UAC000F", 0},
    /* 3Com Corp. */
    /* Gateway Telepath IIvi 33.6 */
    {"USR0000", 0},
    /* U.S. Robotics Sporster 33.6K Fax INT PnP */
    {"USR0002", 0},
    /*  Sportster Vi 14.4 PnP FAX Voicemail */
    {"USR0004", 0},
    /* U.S. Robotics 33.6K Voice INT PnP */
    {"USR0006", 0},
    /* U.S. Robotics 33.6K Voice EXT PnP */
    {"USR0007", 0},
    /* U.S. Robotics Courier V.Everything INT PnP */
    {"USR0009", 0},
    /* U.S. Robotics 33.6K Voice INT PnP */
    {"USR2002", 0},
    /* U.S. Robotics 56K Voice INT PnP */
    {"USR2070", 0},
    /* U.S. Robotics 56K Voice EXT PnP */
    {"USR2080", 0},
    /* U.S. Robotics 56K FAX INT */
    {"USR3031", 0},
    /* U.S. Robotics 56K FAX INT */
    {"USR3050", 0},
    /* U.S. Robotics 56K Voice INT PnP */
    {"USR3070", 0},
    /* U.S. Robotics 56K Voice EXT PnP */
    {"USR3080", 0},
    /* U.S. Robotics 56K Voice INT PnP */
    {"USR3090", 0},
    /* U.S. Robotics 56K Message  */
    {"USR9100", 0},
    /* U.S. Robotics 56K FAX EXT PnP*/
    {"USR9160", 0},
    /* U.S. Robotics 56K FAX INT PnP*/
    {"USR9170", 0},
    /* U.S. Robotics 56K Voice EXT PnP*/
    {"USR9180", 0},
    /* U.S. Robotics 56K Voice INT PnP*/
    {"USR9190", 0},
    /* Wacom tablets */
    {"WACFXXX", 0},
    /* Compaq touchscreen */
    {"FPI2002", 0},
    /* Fujitsu Stylistic touchscreens */
    {"FUJ02B2", 0},
    {"FUJ02B3", 0},
    /* Fujitsu Stylistic LT touchscreens */
    {"FUJ02B4", 0},
    /* Passive Fujitsu Stylistic touchscreens */
    {"FUJ02B6", 0},
    {"FUJ02B7", 0},
    {"FUJ02B8", 0},
    {"FUJ02B9", 0},
    {"FUJ02BC", 0},
    /* Fujitsu Wacom Tablet PC device */
    {"FUJ02E5", 0},
    /* Fujitsu P-series tablet PC device */
    {"FUJ02E6", 0},
    /* Fujitsu Wacom 2FGT Tablet PC device */
    {"FUJ02E7", 0},
    /* Fujitsu Wacom 1FGT Tablet PC device */
    {"FUJ02E9", 0},
    /*
     * LG C1 EXPRESS DUAL (C1-PB11A3) touch screen (actually a FUJ02E6
     * in disguise).
     */
    {"LTS0001", 0},
    /* Rockwell's (PORALiNK) 33600 INT PNP */
    {"WCI0003", 0},
    /* Unknown PnP modems */
    {"PNPCXXX", (void *)UNKNOWN_DEV},
    /* More unknown PnP modems */
    {"PNPDXXX", (void *)UNKNOWN_DEV},
    /*
     * Winbond CIR port, should not be probed. We should keep track of
     * it to prevent the legacy serial driver from probing it.
     */
    {"WEC1022", (void *)CIR_PORT},
    /*
     * SMSC IrCC SIR/FIR port, should not be probed by serial driver as
     * well so its own driver can bind to it.
     */
    {"SMCF010", (void *)CIR_PORT},
    {},
};

static void pnp8250_hw_init(struct uart_port *port)
{
    serialout(port, 8, 1);
    serialout(port, 8, 0);

    serialout(port, UART_IER_OFFSET, 0);

    serialout(port, 4, 0);
    serialout(port, 4, serialin(port, 4) | 0x02);

    serialout(port, 3, serialin(port, 3) | 0x80);
    serialout(port, 3, serialin(port, 3) & ~0x80);
    serialout(port, 3, (serialin(port, 3) & ~0x80) | 0x3);
    serialout(port, 3, (serialin(port, 3) & ~0x4));
    serialout(port, 3, (serialin(port, 3) & ~0x8));

    serialout(port, UART_IER_OFFSET, 0x2);
}

static int pnp8250_probe(struct platform_device *pdev)
{
    ACPI_RESOURCE *resource;
    uintptr_t flags = (uintptr_t)pdev->dev.driver_data;
    phys_addr_t base;
    int irq;
    struct uart_8250_port *pnp8250;
    int io_type;
    int ret;
    char irq_name[5];
    int com;

    /* 暂时不支持检测未知类型设备 */
    if (flags & UNKNOWN_DEV)
    {
        return 0;
    }

    /* 尝试获取IO资源 */
    resource = acpi_device_get_resource(&pdev->adev, ACPI_RESOURCE_TYPE_IO);

    /* 如果是 CIR串口 则必须是IO模式访问 */
    if (flags & CIR_PORT)
    {
        if (!resource)
        {
            KLOG_E("Failed to get resource");
            return -ENODEV;
        }
    }

    /* 如果不是IO 就尝试获取mem资源 */
    if (!resource)
    {
        resource = acpi_device_get_resource(&pdev->adev, ACPI_RESOURCE_TYPE_MEMORY32);
        if (!resource)
        {
            KLOG_E("Failed to get resource");
            return -ENODEV;
        }
        base = resource->Data.Address32.Address.Minimum;
        io_type = UART_IO_MEM32;
    }
    else
    {
        base = resource->Data.Io.Minimum;
        io_type = UART_IO_PORT;
    }

    if (!base)
    {
        return -ENODEV;
    }

    resource = acpi_device_get_resource(&pdev->adev, ACPI_RESOURCE_TYPE_IRQ);
    if (!resource)
    {
        KLOG_E("Failed to get resource irq");
        return -ENODEV;
    }

    irq = resource->Data.Irq.Interrupt;

    if (!irq)
    {
        return -ENODEV;
    }
    KLOG_I("pnp8250 probe: %s", pdev->adev.path);
    KLOG_I("base: 0x%08lX", base);
    KLOG_I("irq: %d", irq);

    /* 使用托管内存分配（绑定到 dev） */
    pnp8250 = devm_kzalloc(&pdev->dev, sizeof(*pnp8250), GFP_KERNEL);
    if (!pnp8250)
    {
        KLOG_E("Failed to allocate memory");
        return -ENOMEM;
    }

    /* 初始化 uart_port */
    pnp8250->port.dev = &pdev->dev;
    pnp8250->port.ops = &uart_8250_ops; /* 使用 8250 通用 ops */
    pnp8250->port.io_type = io_type;
    pnp8250->port.config.baudrate = CONFIG_UART_BAUD;
    pnp8250->port.config.data_bits = 8;
    pnp8250->port.config.stop_bit = 0;
    pnp8250->port.config.parity = 0;
    pnp8250->port.config.uartclk = 1843200; /* Standard PC UART clock */
    pnp8250->port.is_console = true;
    pnp8250->use_fifo = false; /* i8250 不使用 FIFO */

    pdev->dev.priv = &pnp8250->port;
    pdev->dev.is_fixed_minor = true;

    if (base == 0x3F8)
        com = 1;
    else if (base == 0x2F8)
        com = 2;
    else if (base == 0x3E8)
        com = 3;
    else if (base == 0x2E8)
        com = 4;
    else
        com = -1; // 非标准串口

    if (com != -1)
    {
        pdev->dev.minor = com - 1;
    }
    else
    {
        pdev->dev.is_fixed_minor = false;
    }

    if (io_type == UART_IO_PORT)
    {
        pnp8250->port.io_base = base;
    }
    else
    {
        pnp8250->port.membase = (void *)base;
    }
    pnp8250->port.hw_irq = irq;

    /* 设置 IO 访问函数 */
    uart_set_io_from_type(&pnp8250->port);

    /* i8250 特定的硬件初始化 */
    pnp8250_hw_init(&pnp8250->port);

    uart8250_init_config(&pnp8250->port);

    /* 分配中断号 */
    pnp8250->port.irq = ttos_pic_irq_alloc(NULL, pnp8250->port.hw_irq);

    KLOG_I("UART hw_irq:%d virq:%d", pnp8250->port.hw_irq, pnp8250->port.irq);

    /* 使用托管中断注册 */
    snprintf(irq_name, sizeof(irq_name), "COM%d", com == -1 ? '?' : com);
    ret = devm_request_irq(&pdev->dev, pnp8250->port.irq, uart_8250_interrupt,
                           resource->Data.Irq.Shareable ? IRQ_SHARED : 0, irq_name, &pdev->dev);
    if (ret)
    {
        KLOG_E("Failed to request IRQ %d, ret=%d", pnp8250->port.irq, ret);
        return ret;
    }

    /* 初始化时先 mask 中断 */
    ttos_pic_irq_mask(pnp8250->port.irq);

    /* 使用统一的 UART 注册函数 */
    ret = uart_register_port(&pdev->dev);
    if (ret < 0)
    {
        KLOG_E("Failed to register UART port");
        return ret;
    }

    KLOG_I("pnp8250 UART initialized successfully");

    return 0;
}

static void pnp8250_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    /* 所有资源由 devres 自动释放 */
    uart_unregister_port(dev);
}

struct platform_driver pnp8250_driver = {
    .probe = pnp8250_probe,
    .remove = pnp8250_remove,
    .driver =
        {
            .name = "pnp8250",
            .acpi_match_table = i8250_acpi_ids,
        },
};

static int pnp8250_init(void)
{
    return platform_add_driver(&pnp8250_driver);
}

INIT_EXPORT_DRIVER(pnp8250_init, "pnp8250 driver init");

#endif