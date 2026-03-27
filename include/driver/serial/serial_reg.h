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

#ifndef __SERIAL_REG_H__
#define __SERIAL_REG_H__

#define CONFIG_DW8250_REGINCR   1

/* 禁止所有中断 */
#define IER_DISABLE_ALL          0x00

/* Register offsets *********************************************************/

#define UART_RBR_INCR          0  /* (DLAB =0) Receiver Buffer Register */
#define UART_THR_INCR          0  /* (DLAB =0) Transmit Holding Register */
#define UART_DLL_INCR          0  /* (DLAB =1) Divisor Latch LSB */
#define UART_DLM_INCR          1  /* (DLAB =1) Divisor Latch MSB */
#define UART_IER_INCR          1  /* (DLAB =0) Interrupt Enable Register */
#define UART_IIR_INCR          2  /* Interrupt ID Register */
#define UART_FCR_INCR          2  /* FIFO Control Register */
#define UART_LCR_INCR          3  /* Line Control Register */
#define UART_MCR_INCR          4  /* Modem Control Register */
#define UART_LSR_INCR          5  /* Line Status Register */
#define UART_MSR_INCR          6  /* Modem Status Register */
#define UART_SCR_INCR          7  /* Scratch Pad Register */
#define UART_USR_INCR          31 /* UART Status Register */

#define UART_RBR_OFFSET        (CONFIG_DW8250_REGINCR * UART_RBR_INCR)
#define UART_THR_OFFSET        (CONFIG_DW8250_REGINCR * UART_THR_INCR)
#define UART_DLL_OFFSET        (CONFIG_DW8250_REGINCR * UART_DLL_INCR)
#define UART_DLM_OFFSET        (CONFIG_DW8250_REGINCR * UART_DLM_INCR)
#define UART_IER_OFFSET        (CONFIG_DW8250_REGINCR * UART_IER_INCR)
#define UART_IIR_OFFSET        (CONFIG_DW8250_REGINCR * UART_IIR_INCR)
#define UART_FCR_OFFSET        (CONFIG_DW8250_REGINCR * UART_FCR_INCR)
#define UART_LCR_OFFSET        (CONFIG_DW8250_REGINCR * UART_LCR_INCR)
#define UART_MCR_OFFSET        (CONFIG_DW8250_REGINCR * UART_MCR_INCR)
#define UART_LSR_OFFSET        (CONFIG_DW8250_REGINCR * UART_LSR_INCR)
#define UART_MSR_OFFSET        (CONFIG_DW8250_REGINCR * UART_MSR_INCR)
#define UART_SCR_OFFSET        (CONFIG_DW8250_REGINCR * UART_SCR_INCR)
#define UART_USR_OFFSET        (CONFIG_DW8250_REGINCR * UART_USR_INCR)

/* Register bit definitions *************************************************/

/* RBR (DLAB =0) Receiver Buffer Register */

#define UART_RBR_MASK                (0xff)    /* Bits 0-7: Oldest received byte in RX FIFO */
                                               /* Bits 8-31: Reserved */

/* THR (DLAB =0) Transmit Holding Register */

#define UART_THR_MASK                (0xff)    /* Bits 0-7: Adds byte to TX FIFO */
                                               /* Bits 8-31: Reserved */

/* DLL (DLAB =1) Divisor Latch LSB */

#define UART_DLL_MASK                (0xff)    /* Bits 0-7: DLL */
                                               /* Bits 8-31: Reserved */

/* DLM (DLAB =1) Divisor Latch MSB */

#define UART_DLM_MASK                (0xff)    /* Bits 0-7: DLM */
                                               /* Bits 8-31: Reserved */

/* IER (DLAB =0) Interrupt Enable Register */

#define UART_IER_ERBFI               (1 << 0)  /* Bit 0: Enable received data available interrupt */
#define UART_IER_ETBEI               (1 << 1)  /* Bit 1: Enable THR empty interrupt */
#define UART_IER_ELSI                (1 << 2)  /* Bit 2: Enable receiver line status interrupt */
#define UART_IER_EDSSI               (1 << 3)  /* Bit 3: Enable MODEM status interrupt */
                                               /* Bits 4-7: Reserved */
#define UART_IER_ALLIE               (0x0f)

/* IIR Interrupt ID Register */

#define UART_IIR_INTSTATUS           (1 << 0)  /* Bit 0:  Interrupt status (active low) */
#define UART_IIR_INTID_SHIFT         (1)       /* Bits 1-3: Interrupt identification */
#define UART_IIR_INTID_MASK          (7 << UART_IIR_INTID_SHIFT)
#  define UART_IIR_INTID_MSI         (0 << UART_IIR_INTID_SHIFT) /* Modem Status */
#  define UART_IIR_INTID_THRE        (1 << UART_IIR_INTID_SHIFT) /* THR Empty Interrupt */
#  define UART_IIR_INTID_RDA         (2 << UART_IIR_INTID_SHIFT) /* Receive Data Available (RDA) */
#  define UART_IIR_INTID_RLS         (3 << UART_IIR_INTID_SHIFT) /* Receiver Line Status (RLS) */
#  define UART_IIR_INTID_CTI         (6 << UART_IIR_INTID_SHIFT) /* Character Time-out Indicator (CTI) */

                                               /* Bits 4-5: Reserved */
#define UART_IIR_FIFOEN_SHIFT        (6)       /* Bits 6-7: RCVR FIFO interrupt */
#define UART_IIR_FIFOEN_MASK         (3 << UART_IIR_FIFOEN_SHIFT)

/* FCR FIFO Control Register */

#define UART_FCR_FIFOEN              (1 << 0)  /* Bit 0:  Enable FIFOs */
#define UART_FCR_RXRST               (1 << 1)  /* Bit 1:  RX FIFO Reset */
#define UART_FCR_TXRST               (1 << 2)  /* Bit 2:  TX FIFO Reset */
#define UART_FCR_DMAMODE             (1 << 3)  /* Bit 3:  DMA Mode Select */
                                               /* Bits 4-5: Reserved */
#define UART_FCR_RXTRIGGER_SHIFT     (6)       /* Bits 6-7: RX Trigger Level */
#define UART_FCR_RXTRIGGER_MASK      (3 << UART_FCR_RXTRIGGER_SHIFT)
#  define UART_FCR_RXTRIGGER_1       (0 << UART_FCR_RXTRIGGER_SHIFT) /*  Trigger level 0 (1 character) */
#  define UART_FCR_RXTRIGGER_4       (1 << UART_FCR_RXTRIGGER_SHIFT) /* Trigger level 1 (4 characters) */
#  define UART_FCR_RXTRIGGER_8       (2 << UART_FCR_RXTRIGGER_SHIFT) /* Trigger level 2 (8 characters) */
#  define UART_FCR_RXTRIGGER_14      (3 << UART_FCR_RXTRIGGER_SHIFT) /* Trigger level 3 (14 characters) */

/* LCR Line Control Register */

#define UART_LCR_WLS_SHIFT           (0)       /* Bit 0-1: Word Length Select */
#define UART_LCR_WLS_MASK            (3 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_5BIT          (0 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_6BIT          (1 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_7BIT          (2 << UART_LCR_WLS_SHIFT)
#  define UART_LCR_WLS_8BIT          (3 << UART_LCR_WLS_SHIFT)
#define UART_LCR_STB                 (1 << 2)  /* Bit 2:  Number of Stop Bits */
#define UART_LCR_PEN                 (1 << 3)  /* Bit 3:  Parity Enable */
#define UART_LCR_EPS                 (1 << 4)  /* Bit 4:  Even Parity Select */
#define UART_LCR_STICKY              (1 << 5)  /* Bit 5:  Stick Parity */
#define UART_LCR_BRK                 (1 << 6)  /* Bit 6: Break Control */
#define UART_LCR_DLAB                (1 << 7)  /* Bit 7: Divisor Latch Access Bit (DLAB) */

/* MCR Modem Control Register */

#define UART_MCR_DTR                 (1 << 0)  /* Bit 0:  DTR Control Source for DTR output */
#define UART_MCR_RTS                 (1 << 1)  /* Bit 1:  Control Source for  RTS output */
#define UART_MCR_OUT1                (1 << 2)  /* Bit 2:  Auxiliary user-defined output 1 */
#define UART_MCR_OUT2                (1 << 3)  /* Bit 3:  Auxiliary user-defined output 2 */
#define UART_MCR_LPBK                (1 << 4)  /* Bit 4:  Loopback Mode Select */
#define UART_MCR_AFCE                (1 << 5)  /* Bit 5:  Auto Flow Control Enable */
                                               /* Bit 6-7: Reserved */

/* LSR Line Status Register */

#define UART_LSR_DR                  (1 << 0)  /* Bit 0:  Data Ready */
#define UART_LSR_OE                  (1 << 1)  /* Bit 1:  Overrun Error */
#define UART_LSR_PE                  (1 << 2)  /* Bit 2:  Parity Error */
#define UART_LSR_FE                  (1 << 3)  /* Bit 3:  Framing Error */
#define UART_LSR_BI                  (1 << 4)  /* Bit 4:  Break Interrupt */
#define UART_LSR_THRE                (1 << 5)  /* Bit 5:  Transmitter Holding Register Empty */
#define UART_LSR_TEMT                (1 << 6)  /* Bit 6:  Transmitter Empty */
#define UART_LSR_RXFE                (1 << 7)  /* Bit 7:  Error in RX FIFO (RXFE) */

/* SCR Scratch Pad Register */

#define UART_SCR_MASK                (0xff)    /* Bits 0-7: SCR data */

/* USR UART Status Register */

#define UART_USR_BUSY                (1 << 0)  /* Bit 0: UART Busy */
#define UART_USR_TFNF                (1 << 1)  /* Bit 1: Transmit FIFO is not full */
#define UART_USR_TFE                 (1 << 2)  /* Bit 2: Transmit FIFO is empty */
#define UART_USR_RFNE                (1 << 3)  /* Bit 3:  Receive FIFO is not empty */
#define UART_USR_RFF                 (1 << 4)  /* Bit 4:   Receive FIFO Full */

#endif      /* __SERIAL_REG_H__ */