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

#include <driver/of.h>
#include <driver/serial/serial_reg.h>
#include <io.h>
#include <stdint.h>
#include <string.h>
#include <system/kconfig.h>
#include <ttosMM.h>

struct earlycon_uart
{
    unsigned long base;
    unsigned int wait_flag;
    unsigned int wait_offset;
    unsigned int send_offset;
    unsigned int flag_r;
};

static struct earlycon_uart *earlycon = NULL;
static struct of_device_id match_earlycon_uart[] = {
    {.compatible = "arm,pl011",
     .data =
         &(struct earlycon_uart){
             .wait_flag = 0x80,
             .wait_offset = 0x18,
             .send_offset = 0,
             .flag_r = 1,
         }},

    {.compatible = "snps,dw-apb-uart",
     .data =
         &(struct earlycon_uart){
             .wait_flag = 0x60,
             .wait_offset = 0x14,
             .send_offset = 0,
             .flag_r = 1,
         }},
    {/* end of list */},
};

void printk_init(void *printChar);

static void earlycon_write_char(char ch)
{
    while (!(readl(earlycon->base + earlycon->wait_offset) & earlycon->wait_flag))
        ;

    if (earlycon->flag_r && (ch == '\n'))
        earlycon_write_char('\r');

    writel(ch, earlycon->base + earlycon->send_offset);
}

static void *earlycon_uart_compatible(const char *compatible)
{
    const struct of_device_id *dev;

    for (dev = match_earlycon_uart; dev->compatible[0]; dev++)
    {
        if (!strncmp(dev->compatible, compatible, strlen(compatible)))
        {
            earlycon = (struct earlycon_uart *)dev->data;
            return earlycon;
        }
    }

    return NULL;
}

static void earlycon_uart_match(const void *fdt, int startoffset)
{
    const unsigned int *reg;
    const char *compatible;
    unsigned long long address;
    int len, len_sum;
    int addr_cells;

    compatible = fdt_getprop(fdt, startoffset, "compatible", &len_sum);

    for (len = 0; len < len_sum;)
    {
        if (earlycon_uart_compatible(compatible))
        {
            break;
        }

        len += strlen(compatible) + 1;
        compatible += strlen(compatible) + 1;
    }

    reg = fdt_getprop((const void *)fdt, startoffset, "reg", &len);

    if (earlycon)
    {
        addr_cells = fdt_address_cells(fdt, fdt_path_offset(fdt, "/"));

        address = fdt32_to_cpu(reg[0]);
        if (addr_cells == 2)
            address = (address << 32) | fdt32_to_cpu(reg[1]);

        earlycon->base = fix_map_set(FIX_MAP_EARLYCON, address, MT_KERNEL_IO);

        if (earlycon->base)
            printk_init(earlycon_write_char);
    }
}

__weak void board_earlycon_uart_config(char **compatible, unsigned long *base) {}

static void of_earlycon_match_and_init(void)
{
    char *compatible;
    unsigned long base = 0;

    board_earlycon_uart_config(&compatible, &base);
    if (base)
    {
        if (earlycon_uart_compatible(compatible))
        {
            earlycon->base = (unsigned long)fix_map_set(FIX_MAP_EARLYCON, base, MT_KERNEL_IO);
            printk_init(earlycon_write_char);
        }
    }
}
const char *g_eupath = NULL;
const char *earlycon_uart_of_path(void)
{
#ifdef __x86_64__
    g_eupath = "/dev/ttyS0";
    return g_eupath;
#else
    return g_eupath;
#endif
}

void of_earlycon_uart_init(const void *fdt)
{
    const char *stdout_path;
    const char *node_path;
    const char *alias_name;
    int offset;
    int len;

    offset = fdt_path_offset(fdt, "/chosen");

    if (offset > 0)
    {
        stdout_path = fdt_getprop(fdt, offset, "stdout-path", &len);
        if (!stdout_path)
        {
            of_earlycon_match_and_init();
            return;
        }

        alias_name = strtok((char *)stdout_path, ":");

        /* stdout-path = "aliases" */
        if (alias_name[0] != '/')
        {
            offset = fdt_path_offset(fdt, "/aliases");

            if (offset < 0)
                return;

            node_path = fdt_getprop(fdt, offset, alias_name, &len);
        }
        else
        {
            /* stdout-path = /slave@3e000000/serial@0:115200n8 */
            /* stdout-path = &uart0 */
            node_path = alias_name;
        }

        offset = fdt_path_offset(fdt, node_path);
    }

    if (offset > 0)
    {
        g_eupath = node_path;
        earlycon_uart_match(fdt, offset);
    }
}

/**
 *  x86架构的earlycon串口初始化
 */

#ifdef CONFIG_UART_I8250

#include <asm/io.h>

/* uart使用的端口号 */
#define UART_COM1 0x03F8
#define UART_COM2 0x02F8

/* 默认时钟频率设置为1.8432MHz */
#define UART_DEFAULT_INPUT_FREQUENCY 1843200
#define UART_DEFAULT_BAUD_RATE 115200
#define UART_DEFAULT_CHAR_SIZE 8
#define UART_DEFAULT_PARITY_TYPE 0
#define UART_DEFAULT_STOP_BIT_COUNT 1

#define UART_PORT_ENABLE 1
#define UART_PORT_DISABLE 0

typedef struct
{
    uint32_t base_address;
    uint8_t enabled;
    uint8_t char_size;
    uint8_t parity_type;
    uint8_t stop_bit_count;
    uint32_t baud_rate;
    uint32_t input_frequency;
} uart_config;

static uart_config cur_uart;

/**
 * 写入字符到串口
 * @param ch 字符
 */
static void uart_i8250_write(char ch)
{
    uint32_t value;

    while (1)
    {
        inb(cur_uart.base_address + UART_LSR_INCR, value);
        if ((value & UART_LSR_THRE) && cur_uart.enabled)
        {
            break;
        }
    }

    if (ch == '\n')
    {
        uart_i8250_write('\r');
    }

    outb(cur_uart.base_address + UART_THR_INCR, ch);
}

static uint32_t io_readb(uart_config *up, uint32_t offset)
{
    uint8_t value;
#ifdef __x86_64__
    inb(up->base_address + offset, value);
#else
    value = (uint8_t)readb(up->base_address + offset);
#endif
    return value;
}

static void io_writeb(uart_config *up, uint32_t offset, uint32_t value)
{
#ifdef __x86_64__
    outb(up->base_address + offset, value);
#else
    writeb(value, up->base_address + offset);
#endif
}

/**
 * 初始化串口
 * @param device 串口设备结构体
 */
static void init_uart_8250(uart_config *config)
{
    uint8_t value;

    cur_uart.base_address = config->base_address;
    cur_uart.enabled = config->enabled;
    cur_uart.char_size = config->char_size;
    cur_uart.parity_type = config->parity_type;
    cur_uart.stop_bit_count = config->stop_bit_count;
    cur_uart.baud_rate = config->baud_rate;
    cur_uart.input_frequency = config->input_frequency;

    io_writeb(&cur_uart, 8, 1);
    io_writeb(&cur_uart, 8, 0);

    io_writeb(&cur_uart, UART_IER_OFFSET, 0);

    io_writeb(&cur_uart, 4, 0);
    io_writeb(&cur_uart, 4, io_readb(&cur_uart, 4) | 0x02);

    io_writeb(&cur_uart, 3, io_readb(&cur_uart, 3) | 0x80);
    io_writeb(&cur_uart, 3, io_readb(&cur_uart, 3) & ~0x80);
    io_writeb(&cur_uart, 3, (io_readb(&cur_uart, 3) & ~0x80) | 0x3);
    io_writeb(&cur_uart, 3, (io_readb(&cur_uart, 3) & ~0x4));
    io_writeb(&cur_uart, 3, (io_readb(&cur_uart, 3) & ~0x8));
}

static void x86_8250_earlycon_uart_init()
{
    uart_config config;

    /* 默认使用端口1: 0x3f8 */
    config.base_address = UART_COM1;
    config.enabled = UART_PORT_ENABLE;
    config.char_size = UART_DEFAULT_CHAR_SIZE;
    config.parity_type = UART_DEFAULT_PARITY_TYPE;
    config.stop_bit_count = UART_DEFAULT_STOP_BIT_COUNT;
    config.baud_rate = UART_DEFAULT_BAUD_RATE;
    config.input_frequency = UART_DEFAULT_INPUT_FREQUENCY;

    /* 初始化串口 */
    init_uart_8250(&config);

    printk_init(uart_i8250_write);
}

#else /* CONFIG_UART_I8250 */

static void x86_8250_earlycon_uart_init() {}

#endif

void earlycon_uart_init(const void *fdt)
{
    if (IS_ENABLED(CONFIG_UART_I8250))
    {
        x86_8250_earlycon_uart_init();
    }
    else
    {
        of_earlycon_uart_init(fdt);
    }
}
