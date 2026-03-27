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

#ifndef __CHAR_DEV_H__
#define __CHAR_DEV_H__

#include <driver/devclass.h>

#include <circbuf.h>
#include <fs/kpoll.h>
#include <stdint.h>
#include <termios.h>
#include <ttos.h>

#ifndef CONFIG_SERIAL_NPOLLWAITERS
#define CONFIG_SERIAL_NPOLLWAITERS 4
#endif

#define CONFIG_TTY_SIGINT 1
#define CONFIG_TTY_SIGINT_CHAR 0x3

#define char_setup(dev) ((struct char_class_type *)((dev)->class))->ops->setup(dev)
#define char_shutdown(dev) ((struct char_class_type *)((dev)->class))->ops->shutdown(dev)
#define char_attach(dev) ((struct char_class_type *)((dev)->class))->ops->attach(dev)
#define char_detach(dev) ((struct char_class_type *)((dev)->class))->ops->detach(dev)
#define char_enabletxint(dev) ((struct char_class_type *)((dev)->class))->ops->txint(dev, true)
#define char_disabletxint(dev) ((struct char_class_type *)((dev)->class))->ops->txint(dev, false)
#define char_enablerxint(dev) ((struct char_class_type *)((dev)->class))->ops->rxint(dev, true)
#define char_disablerxint(dev) ((struct char_class_type *)((dev)->class))->ops->rxint(dev, false)
#define char_rxavailable(dev) ((struct char_class_type *)((dev)->class))->ops->rxavailable(dev)
#define char_txready(dev) ((struct char_class_type *)((dev)->class))->ops->txready(dev)
#define char_txempty(dev) ((struct char_class_type *)((dev)->class))->ops->txempty(dev)
#define char_send(dev, ch) ((struct char_class_type *)((dev)->class))->ops->send(dev, ch)
#define char_receive(dev, s) ((struct char_class_type *)((dev)->class))->ops->receive(dev, s)

#ifdef CONFIG_SERIAL_TXDMA
#define char_dmasend(dev)                                                                          \
    (((struct char_class_type *)((dev)->class))->ops->dmasend                                      \
         ? ((struct char_class_type *)((dev)->class))->ops->dmasend(dev)                           \
         : -ENOSYS)

#define char_dmatxavail(dev)                                                                       \
    (((struct char_class_type *)((dev)->class))->ops->dmatxavail                                   \
         ? ((struct char_class_type *)((dev)->class))->ops->dmatxavail(dev)                        \
         : -ENOSYS)
#endif

#ifdef CONFIG_SERIAL_RXDMA
#define char_dmareceive(dev)                                                                       \
    (((struct char_class_type *)((dev)->class))->ops->dmareceive                                   \
         ? ((struct char_class_type *)((dev)->class))->ops->dmareceive(dev)                        \
         : -ENOSYS)

#define char_dmarxfree(dev)                                                                        \
    (((struct char_class_type *)((dev)->class))->ops->dmarxfree                                    \
         ? ((struct char_class_type *)((dev)->class))->ops->dmarxfree(dev)                         \
         : -ENOSYS)
#endif

#define char_rxflowcontrol(dev, n, u)                                                              \
    (((struct char_class_type *)((dev)->class))->ops->rxflowcontrol &&                             \
     ((struct char_class_type *)((dev)->class))->ops->rxflowcontrol(dev, n, u))
struct file;
struct char_ops_s
{
    /* Configure the UART baud, bits, parity, fifos, etc. This method is called
     * the first time that the serial port is opened. For the serial console,
     * this will occur very early in initialization; for other serial ports
     * this will occur when the port is first opened. This setup does not
     * include attaching or enabling interrupts. That portion of the UART setup
     * is performed when the attach() method is called.
     */

    int (*setup)(struct device *dev);

    /* Disable the UART.  This method is called when the serial port is closed.
     * This method reverses the operation the setup method.
     * NOTE that the serial console is never shutdown.
     */

    void (*shutdown)(struct device *dev);

    /* Configure the UART to operation in interrupt driven mode. This method is
     * called when the serial port is opened. Normally, this is just after the
     * the setup() method is called, however, the serial console may operate in
     * a non-interrupt driven mode during the boot phase.
     *
     * RX and TX interrupts are not enabled when by the attach method (unless
     * the hardware supports multiple levels of interrupt enabling). The RX
     * and TX interrupts are not enabled until the txint() and rxint() methods
     * are called.
     */

    int (*attach)(struct device *dev);

    /* Detach UART interrupts. This method is called when the serial port is
     * closed normally just before the shutdown method is called.
     * The exception is the serial console which is never shutdown.
     */

    void (*detach)(struct device *dev);

    /* All ioctl calls will be routed through this method */

    int (*ioctl)(struct file *filep, int cmd, unsigned long arg);

    /* Called (usually) from the interrupt level to receive one character from
     * the UART.  Error bits associated with the receipt are provided in the
     * the return 'status'.
     */

    int (*receive)(struct device *dev, unsigned int *status);

    /* Call to enable or disable RX interrupts */

    void (*rxint)(struct device *dev, bool enable);

    /* Return true if the receive data is available */

    bool (*rxavailable)(struct device *dev);

    /* Return true if UART activated RX flow control to block more incoming
     * data.
     */

    bool (*rxflowcontrol)(struct device *dev, unsigned int nbuffered, bool upper);

#ifdef CONFIG_SERIAL_TXDMA
    /* Start transfer bytes from the TX circular buffer using DMA */

    void (*dmasend)(struct device *dev);
#endif

#ifdef CONFIG_SERIAL_RXDMA
    /* Start transfer bytes from the TX circular buffer using DMA */

    void (*dmareceive)(struct device *dev);

    /* Notify DMA that there is free space in the RX buffer */

    void (*dmarxfree)(struct device *dev);
#endif

#ifdef CONFIG_SERIAL_TXDMA
    /* Notify DMA that there is data to be transferred in the TX buffer */

    void (*dmatxavail)(struct device *dev);
#endif

    /* This method will send one byte on the UART */

    void (*send)(struct device *dev, int ch);

    /* Call to enable or disable TX interrupts */

    void (*txint)(struct device *dev, bool enable);

    /* Return true if the tranmsit hardware is ready to send another byte.
     * This is used to determine if send() method can be called.
     */

    bool (*txready)(struct device *dev);

    /* Return true if all characters have been sent.  If for example, the UART
     * hardware implements FIFOs, then this would mean the transmit FIFO is
     * empty.  This method is called when the driver needs to make sure that
     * all characters are "drained" from the TX hardware.
     */

    bool (*txempty)(struct device *dev);
};

struct char_buffer_s
{
    MUTEX_ID lock; /* Used to control exclusive access to the buffer */
    circbuf_t buf;
};

#if defined(CONFIG_SERIAL_RXDMA) || defined(CONFIG_SERIAL_TXDMA)
struct uart_dmaxfer_s
{
    char *buffer;   /* First DMA buffer */
    char *nbuffer;  /* Next DMA buffer */
    size_t length;  /* Length of first DMA buffer */
    size_t nlength; /* Length of next DMA buffer */
    size_t nbytes;  /* Bytes actually transferred by DMA from both buffers */
};
#endif /* CONFIG_SERIAL_RXDMA || CONFIG_SERIAL_TXDMA */

struct char_class_type
{
    struct devclass_type class;

    /* State data */
    uint8_t escape; /* Number of the character to be escaped */
#ifdef CONFIG_SERIAL_REMOVABLE
    volatile bool disconnected; /* true: Removable device is not connected */
#endif
    bool isconsole; /* true: This is the serial console */

#if defined(CONFIG_TTY_SIGINT) || defined(CONFIG_TTY_SIGTSTP) ||                                   \
    defined(CONFIG_TTY_FORCE_PANIC) || defined(CONFIG_TTY_LAUNCH)
    pid_t pid; /* Thread PID to receive signals (-1 if none) */
    pid_t gpid;
#endif

    /* Terminal control flags */

    tcflag_t tc_iflag; /* Input modes */
    tcflag_t tc_oflag; /* Output modes */
    tcflag_t tc_lflag; /* Local modes */
    cc_t c_line;
    cc_t c_cc[NCCS];

    struct winsize winsize;

    /* Semaphores & mutex */

    SEMA_ID xmitsem;    /* Wakeup user waiting for space in xmit.buffer */
    SEMA_ID recvsem;    /* Wakeup user waiting for data in recv.buffer */
    MUTEX_ID closelock; /* Locks out new open while close is in progress */
    MUTEX_ID polllock;  /* Manages exclusive access to fds[] */

    /* I/O buffers */

    struct char_buffer_s xmit; /* Describes transmit buffer */
    struct char_buffer_s recv; /* Describes receive buffer */

    ttos_spinlock_t xmit_spinlock;
    ttos_spinlock_t recv_spinlock;

    /* DMA transfers */

#ifdef CONFIG_SERIAL_TXDMA
    struct uart_dmaxfer_s dmatx; /* Describes transmit DMA transfer */
#endif
#ifdef CONFIG_SERIAL_RXDMA
    struct uart_dmaxfer_s dmarx; /* Describes receive DMA transfer */
#endif

    /* Driver interface */

    const struct char_ops_s *ops; /* Arch-specific operations */
    void *priv;                   /* Used by the arch-specific logic */

    /* The following is a list if poll structures of threads waiting for
     * driver events. The 'struct kpollfd' reference for each open is also
     * retained in the f_priv field of the 'struct file'.
     */

    uint8_t minrecv; /* Minimum received bytes */
    uint8_t minread; /* c_cc[VMIN] */
    uint8_t timeout; /* c_cc[VTIME] */

    struct kpollfd *fds[CONFIG_SERIAL_NPOLLWAITERS];
    struct aio_info aio;
};

void char_datasent(struct device *dev);
void char_datareceived(struct device *dev);
void char_reset_sem(struct device *dev);
void char_xmitchars(struct device *dev);
void char_recvchars(struct device *dev);

int char_register(const char *path, struct device *dev);

int serial_register(struct device *dev);
const char *earlycon_uart_of_path(void);

#endif /* __CHAR_DEV_H__ */
