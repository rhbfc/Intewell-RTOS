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

/*
 * @file： pl011Uart.h
 * @brief：
 *	    <li>PL011串口驱动相关宏定义。</li>
 */
#ifndef _PL011UAT_H
#define _PL011UAT_H

/************************头文件********************************/
#include <io.h>
#include <system/types.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************************宏定义********************************/

/* PL011寄存器 */
#define UARTDR    0x000
#define UARTRSR   0x004
#define UARTECR   0x004
#define UARTFR    0x018
#define UARTILPR  0x020
#define UARTIBRD  0x024
#define UARTFBRD  0x028
#define UARTLCRH  0x02C
#define UARTCR    0x030
#define UARTIFLS  0x034
#define UARTIMSC  0x038
#define UARTRIS   0x03C
#define UARTMIS   0x040
#define UARTICR   0x044
#define UARTDMACR 0x048

/* 状态寄存器错误掩码 */
#define UART_STATUS_ERROR_MASK 0x0F

/* 标志寄存器位定义 */
#define PL011_UARTFR_RI   (1 << 8)
#define PL011_UARTFR_TXFE (1 << 7)
#define PL011_UARTFR_RXFF (1 << 6)
#define PL011_UARTFR_TXFF (1 << 5)
#define PL011_UARTFR_RXFE (1 << 4)
#define PL011_UARTFR_BUSY (1 << 3)
#define PL011_UARTFR_DCD  (1 << 2)
#define PL011_UARTFR_DSR  (1 << 1)
#define PL011_UARTFR_CTS  (1 << 0)

/* 标志寄存器位定义 */
#define PL011_UARTCR_CTSEN  (1 << 15)
#define PL011_UARTCR_RTSEN  (1 << 14)
#define PL011_UARTCR_RTS    (1 << 11)
#define PL011_UARTCR_DTR    (1 << 10)
#define PL011_UARTCR_RXE    (1 << 9)
#define PL011_UARTCR_TXE    (1 << 8)
#define PL011_UARTCR_LBE    (1 << 7)
#define PL011_UARTCR_UARTEN (1 << 0)

/* LINE控制寄存器位定义 */
#define PL011_UARTLCRH_SPS    (1 << 7)
#define PL011_UARTLCRH_WLEN_8 (3 << 5)
#define PL011_UARTLCRH_WLEN_7 (2 << 5)
#define PL011_UARTLCRH_WLEN_6 (1 << 5)
#define PL011_UARTLCRH_WLEN_5 (0 << 5)
#define PL011_UARTLCRH_FEN    (1 << 4)
#define PL011_UARTLCRH_STP2   (1 << 3)
#define PL011_UARTLCRH_EPS    (1 << 2)
#define PL011_UARTLCRH_PEN    (1 << 1)
#define PL011_UARTLCRH_BRK    (1 << 0)

/* 中断屏蔽寄存器 */
#define PL011_IMSC_OEIM   (1 << 10)
#define PL011_IMSC_BEIM   (1 << 9)
#define PL011_IMSC_PEIM   (1 << 8)
#define PL011_IMSC_FEIM   (1 << 7)
#define PL011_IMSC_RTIM   (1 << 6)
#define PL011_IMSC_TXIM   (1 << 5)
#define PL011_IMSC_RXIM   (1 << 4)
#define PL011_IMSC_DSRMIM (1 << 3)
#define PL011_IMSC_DCDMIM (1 << 2)
#define PL011_IMSC_CTSMIM (1 << 1)
#define PL011_IMSC_RIMIM  (1 << 0)

/************************类型定义******************************/

/************************接口声明******************************/
u32  uart_early_init (phys_addr_t addr);
u32  uart_init (u32 irq, phys_addr_t addr, u32 clk, u32 baudrate);
void uart_write_char (char ch);

char uart_read_char (void);
int  uart_recv_buffer_is_empty (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _PL011UAT_H */
