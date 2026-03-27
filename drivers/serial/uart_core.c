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

/**
 * @file    drivers/serial/uart_core.c
 * @brief   统一的 UART 核心框架 - 消除驱动重复代码
 * @author  TTOS Driver Team
 * @date    2025-10-10
 *
 * 设计哲学（Linus "好品味"原则）：
 * 1. 数据结构正确 -> 代码自然简单
 * 2. 消除特殊情况 -> 统一处理路径
 * 3. 核心层处理通用逻辑，驱动层只处理硬件差异
 *
 * 功能：
 * - 统一的 UART 端口注册/注销
 * - 自动管理缓冲区、锁、信号量
 * - 提供标准的 char_ops_s 实现
 * - 驱动只需实现最小化的 uart_ops
 */

#include <assert.h>
#include <driver/class/chardev.h>
#include <driver/device.h>
#include <driver/serial/serial_core.h>
#include <errno.h>
#include <fs/fs.h>
#include <io.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <ttosMM.h>
#include <ttos_pic.h>
#include <util.h>

#undef KLOG_TAG
#define KLOG_TAG "uart_core"
#include <klog.h>

#define TCGETS2 _IOR('T', 0x2A, struct termios)
#define TCSETS2 _IOW('T', 0x2B, struct termios)
#define TCSETSW2 _IOW('T', 0x2C, struct termios)
#define TCSETSF2 _IOW('T', 0x2D, struct termios)

/****************************************************************************
 * 通用 char_ops_s 实现 - 所有 UART 驱动共享
 ****************************************************************************/

static int uart_setup(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops || !port->ops->set_termios)
        return -EINVAL;

    /* 调用驱动的硬件配置函数 */
    return port->ops->set_termios(port);
}

static void uart_shutdown(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (port && port->ops && port->ops->shutdown)
        port->ops->shutdown(port);
}

static int uart_attach(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;
    int ret;

    if (!port || !port->ops)
        return -EINVAL;

    /* 使用托管中断注册（已在驱动 probe 中完成） */
    ttos_pic_irq_unmask(port->irq);

    /* 启动 UART（如果需要） */
    if (port->ops->startup)
    {
        ret = port->ops->startup(port);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void uart_detach(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port)
        return;

    /* 停止 UART */
    if (port->ops && port->ops->stop_tx)
        port->ops->stop_tx(port);
    if (port->ops && port->ops->stop_rx)
        port->ops->stop_rx(port);

    ttos_pic_irq_mask(port->irq);
}

/* 低速波特率表: B0(0) ~ B38400(15), cflag 值直接作为索引 */
static const int low_speeds[] = {
    0,     50,   75,   110,  134,  150,  200, 300, /* B0 ~ B300 */
    600,   1200, 1800, 2400, 4800, 9600,           /* B600 ~ B9600 */
    19200, 38400                                   /* B19200 ~ B38400 */
};

/* 高速波特率表: B57600(0010001) ~ B4000000(0010017) */
static const int high_speeds[] = {
    57600,   115200,  230400,  460800,  500000, /* B57600 ~ B500000 */
    576000,  921600,  1000000, 1152000,         /* B576000 ~ B1152000 */
    1500000, 2000000, 2500000, 3000000,         /* B1500000 ~ B3000000 */
    3500000, 4000000                            /* B3500000 ~ B4000000 */
};

static int cflags_to_speed(tcflag_t cflags)
{
    tcflag_t baud = cflags & CBAUD;

    if (baud <= B38400)
        return low_speeds[baud];
    if (baud >= B57600 && baud <= B4000000)
        return high_speeds[baud - B57600];
    return 0;
}

static tcflag_t speed_to_cflags(int speed)
{
    /* Linux 风格: 误差容限 = 标准波特率 / 50 (约 2%) */
    for (size_t i = 0; i < ARRAY_SIZE(low_speeds); i++)
    {
        int s = low_speeds[i];
        int tolerance = s / 50;
        if (speed >= s - tolerance && speed <= s + tolerance)
            return (tcflag_t)i;
    }
    for (size_t i = 0; i < ARRAY_SIZE(high_speeds); i++)
    {
        int s = high_speeds[i];
        int tolerance = s / 50;
        if (speed >= s - tolerance && speed <= s + tolerance)
            return B57600 + (tcflag_t)i;
    }
    return 0;
}

static tcflag_t port_cflags(struct uart_port *port)
{
    tcflag_t cflags = 0;
    switch (port->config.data_bits)
    {
    case 5:
        cflags |= CS5;
        break;
    case 6:
        cflags |= CS6;
        break;
    case 7:
        cflags |= CS7;
        break;
    case 8:
        cflags |= CS8;
        break;
    }

    if (port->config.parity)
    {
        cflags |= PARENB;
        if (port->config.parity == 1)
        {
            cflags |= PARODD;
        }
    }
    if (port->config.stop_bit)
        cflags |= CSTOPB;
    if (port->config.flow_ctrl)
        cflags |= CRTSCTS;
    cflags |= speed_to_cflags(port->config.baudrate);
    /* 始终使能接受和忽略载波检测 */
    cflags |= CLOCAL | CREAD;
    return cflags;
}

static int uart_get_termios(struct uart_port *port, struct termios *termios)
{
    /* 返回保存的完整 termios，但 c_cflag 从硬件配置重建 */
    if (port->config.c_cflag)
    {
        termios->c_cflag = port->config.c_cflag;
    }
    else
    {
        termios->c_cflag = port_cflags(port);
    }

    termios->__c_ispeed = port->config.__c_ispeed;
    termios->__c_ospeed = port->config.__c_ospeed;

    return 0;
}

static int uart_set_termios(struct uart_port *port, struct termios *termios,
                            bool using_cflags_set_bandrate)
{
    /* 保存 c_cflag */
    port->config.c_cflag = termios->c_cflag;
    port->config.__c_ispeed = termios->__c_ispeed;
    port->config.__c_ospeed = termios->__c_ospeed;

    /* 从 termios 提取硬件配置 */
    port->config.baudrate =
        using_cflags_set_bandrate ? cflags_to_speed(termios->c_cflag) : termios->__c_ispeed;
    port->config.data_bits = (termios->c_cflag & CSIZE) == CS5   ? 5
                             : (termios->c_cflag & CSIZE) == CS6 ? 6
                             : (termios->c_cflag & CSIZE) == CS7 ? 7
                             : (termios->c_cflag & CSIZE) == CS8 ? 8
                                                                 : 8;
    port->config.parity = (termios->c_cflag & PARENB) ? (termios->c_cflag & PARODD) ? 1 : 2 : 0;
    port->config.stop_bit = (termios->c_cflag & CSTOPB) ? 1 : 0;
    port->config.flow_ctrl = (termios->c_cflag & CRTSCTS) ? 1 : 0;
    return port->ops->set_termios(port);
}

static int uart_ioctl(struct file *filep, int cmd, unsigned long arg)
{
    struct device *dev = filep->f_inode->i_private;
    struct uart_port *port = (struct uart_port *)dev->priv;
    struct termios *termiosp = (struct termios *)(uintptr_t)arg;

    switch (cmd)
    {
    case TCGETS:
    case TCGETS2:
        uart_get_termios(port, termiosp);
        return 0;
    case TCSETS:
    /* 等待输出完成 */
    case TCSETSW:
    /* 刷新缓存区完成 */
    case TCSETSF:
        return uart_set_termios(port, termiosp, true);
    case TCSETS2:
    /* 等待输出完成 */
    case TCSETSW2:
    /* 刷新缓存区完成 */
    case TCSETSF2:
        return uart_set_termios(port, termiosp, false);
    }
    return -ENOTTY;
}

static int uart_receive(struct device *dev, unsigned int *status)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops || !port->ops->rx_char)
        return -1;

    return port->ops->rx_char(port, status);
}

static void uart_rxint(struct device *dev, bool enable)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops)
        return;

    if (enable)
    {
        if (port->ops->start_rx)
            port->ops->start_rx(port);
    }
    else
    {
        if (port->ops->stop_rx)
            port->ops->stop_rx(port);
    }
}

static bool uart_rxavailable(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops || !port->ops->rx_ready)
        return false;

    return port->ops->rx_ready(port);
}

static bool uart_rxflowcontrol(struct device *dev, unsigned int nbuffered, bool upper)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->config.flow_ctrl)
        return false;

    /* 简单的流控：缓冲区满时禁止接收 */
    uart_rxint(dev, !upper);
    return true;
}

static void uart_send(struct device *dev, int ch)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (port && port->ops && port->ops->tx_char)
        port->ops->tx_char(port, ch);
}

static void uart_txint(struct device *dev, bool enable)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops)
        return;

    if (enable)
    {
        if (port->ops->start_tx)
            port->ops->start_tx(port);

        /* Fake a TX interrupt by calling char_xmitchars() */
        char_xmitchars(dev);
    }
    else
    {
        if (port->ops->stop_tx)
            port->ops->stop_tx(port);
    }
}

static bool uart_txready(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops || !port->ops->tx_ready)
        return false;

    return port->ops->tx_ready(port);
}

static bool uart_txempty(struct device *dev)
{
    struct uart_port *port = (struct uart_port *)dev->priv;

    if (!port || !port->ops || !port->ops->tx_empty)
        return true; /* 默认认为是空的 */

    return port->ops->tx_empty(port);
}

/**
 * 统一的 UART 操作函数表
 * 所有 UART 驱动共享这一套实现
 */
static const struct char_ops_s uart_generic_ops = {
    .setup = uart_setup,
    .shutdown = uart_shutdown,
    .attach = uart_attach,
    .detach = uart_detach,
    .ioctl = uart_ioctl,
    .receive = uart_receive,
    .rxint = uart_rxint,
    .rxavailable = uart_rxavailable,
    .rxflowcontrol = uart_rxflowcontrol,
    .send = uart_send,
    .txint = uart_txint,
    .txready = uart_txready,
    .txempty = uart_txempty,
};

/****************************************************************************
 * 通用中断处理 - 驱动可以直接使用
 ****************************************************************************/

/**
 * @brief 通用 UART 中断处理函数
 *
 * 大多数 UART 的中断逻辑都一样：
 * 1. 检查是否有数据可接收 -> 调用 char_recvchars()
 * 2. 检查是否可发送数据 -> 调用 char_xmitchars()
 *
 * 驱动可以直接使用这个函数，或者自己实现复杂的中断处理
 */
void uart_interrupt_handler(u32 irq, void *param)
{
    struct device *dev = (struct device *)param;
    struct uart_port *port;

    if (!dev || !dev->priv)
        return;

    port = (struct uart_port *)dev->priv;

    /* 接收中断 */
    if (port->ops->rx_ready && port->ops->rx_ready(port))
    {
        char_recvchars(dev);
    }

    /* 发送中断 */
    if (port->ops->tx_ready && port->ops->tx_ready(port))
    {
        char_xmitchars(dev);
    }
}

/****************************************************************************
 * 核心注册函数 - 简化驱动代码
 ****************************************************************************/

/**
 * @brief 注册 UART 端口（核心函数）
 *
 * 这个函数完成所有繁琐的初始化工作：
 * 1. 分配并初始化 char_class_type
 * 2. 设置缓冲区、锁、信号量
 * 3. 注册为字符设备
 * 4. 驱动只需调用这一个函数
 *
 * @param dev 设备指针（已设置 dev->priv = uart_port）
 * @return 0 成功，负值失败
 */
int uart_register_port(struct device *dev)
{
    struct uart_port *port;
    struct char_class_type *class;
    void *rx_buf, *tx_buf;

    if (!dev || !dev->priv)
    {
        KLOG_E("Invalid device or uart_port");
        return -EINVAL;
    }

    port = (struct uart_port *)dev->priv;

    if (!port->ops)
    {
        KLOG_E("uart_ops not set");
        return -EINVAL;
    }

    /* 使用托管内存分配 char_class */
    class = devm_kzalloc(dev, sizeof(struct char_class_type), GFP_KERNEL);
    if (!class)
        return -ENOMEM;

    dev->class = (struct devclass_type *)class;

    /* 分配接收和发送缓冲区 */
    rx_buf = devm_kmalloc(dev, CONFIG_UART_RX_BUF_SIZE, GFP_KERNEL);
    if (!rx_buf)
        return -ENOMEM;

    tx_buf = devm_kmalloc(dev, CONFIG_UART_TX_BUF_SIZE, GFP_KERNEL);
    if (!tx_buf)
        return -ENOMEM;

    circbuf_init(&class->recv.buf, rx_buf, CONFIG_UART_RX_BUF_SIZE);
    circbuf_init(&class->xmit.buf, tx_buf, CONFIG_UART_TX_BUF_SIZE);

    /* 初始化锁和信号量 */
    spin_lock_init(&class->xmit_spinlock);
    spin_lock_init(&class->recv_spinlock);

    /* 设置默认窗口大小 */
    class->winsize.ws_col = 80;
    class->winsize.ws_row = 24;

    class->class.device = dev;
    spin_lock_init(&class->class.lock);

    /* 使用通用的 UART 操作函数 */
    class->ops = &uart_generic_ops;

    /* 标记是否为控制台 */
    class->isconsole = port->is_console;

    /* 注册为串口设备 (/dev/ttySx) */
    return serial_register(dev);
}

/**
 * @brief 注销 UART 端口
 *
 * 实际上不需要做什么，devres 会自动清理所有资源
 * 但保留这个函数以保持接口对称性
 */
void uart_unregister_port(struct device *dev)
{
    /* 所有资源由 devres 自动释放 */
    KLOG_I("UART port unregistered");
}
#ifdef __x86_64__
#include <asm/io.h>
#endif

static uint32_t io_port_readb(struct uart_port *up, uint32_t offset)
{
    uint8_t value;
#ifdef __x86_64__
    inb(up->io_base + offset, value);
#else
    value = (uint8_t)readb(up->membase + offset);
#endif
    return value;
}

static void io_port_writeb(struct uart_port *up, uint32_t offset, uint32_t value)
{
#ifdef __x86_64__
    outb(up->io_base + offset, value);
#else
    writeb(value, up->membase + offset);
#endif
}

static uint32_t io_port_readl(struct uart_port *up, uint32_t offset)
{
    return readl((uint32_t *)up->membase + offset);
}

static void io_port_writel(struct uart_port *up, uint32_t offset, uint32_t value)
{
    writel(value, (uint32_t *)up->membase + offset);
}

static uint32_t io_port_readw(struct uart_port *up, uint32_t offset)
{
    return (uint32_t)readw((uint16_t *)up->membase + offset);
}

static void io_port_writew(struct uart_port *up, uint32_t offset, uint32_t value)
{
    writew(value, (uint16_t *)up->membase + offset);
}

/* 根据IO类型设置串口使用的IO函数 */
int uart_set_io_from_type(struct uart_port *p)
{
    switch (p->io_type)
    {
    /* i8250, 向端口输出 */
    case UART_IO_PORT:
    case UART_IO_MEM8:
        p->serial_in = io_port_readb;
        p->serial_out = io_port_writeb;
        break;

    case UART_IO_MEM16:
        p->serial_in = io_port_readw;
        p->serial_out = io_port_writew;
        break;
    case UART_IO_MEM32:
        p->serial_in = io_port_readl;
        p->serial_out = io_port_writel;
    default:
        break;
    }
    return 0;
}
