/* drivers/serial/semc_irda/semc_msm_irda.c
 *
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Manabu Yoshida <Manabu.X.Yoshida@sonyericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-nfc.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/clk.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/poll.h>

#include <linux/serial_core.h>

#include "semc_msm_irda.h"

#include <mach/dma.h>

#define DRV_NAME "semc-msm-irda"
#define DRV_VERSION "1.0"

/* UART DM Register size */
#define	UARTDM_SIZE		0x1000
#define UARTDM_BURST_SIZE	16

/* UART DM Registers */
#define UARTDM_MR2    0x0004
#define UARTDM_CSR    0x0008 /* write only */
#define UARTDM_SR     0x0008 /* read only  */
#define UARTDM_CR     0x0010 /* write only */
#define UARTDM_IMR    0x0014 /* write only */
#define UARTDM_ISR    0x0014 /* read only  */
#define UARTDM_IPR    0x0018
#define UARTDM_TFWR   0x001c
#define UARTDM_RFWR   0x0020
#define UARTDM_DMRX   0x0034
#define UARTDM_IRDA   0x0038
#define UARTDM_RX_TOTAL_SNAP 0x0038 /* read only */
#define UARTDM_DMEN   0x003c
#define UARTDM_NCF_TX 0x0040
#define UARTDM_BADR   0x0044
#define UARTDM_TF     0x0070 /* write only */
#define UARTDM_RF     0x0070 /* read only  */

/* Bit Definition */
/* UARTDM_SR (R) */
#define BIT_TXEMT		0x0008
/* UARTDM_CR (W) */
#define BIT_UART_TX_DISABLE	0x0008
#define BIT_UART_TX_EN		0x0004
#define BIT_UART_RX_DISABLE	0x0002
#define BIT_UART_RX_EN		0x0001
/* UARTDM_IMR (W) */
#define BIT_TX_READY		0x0080
#define BIT_RXLEV		0x0010
#define BIT_RXSTALE		0x0008
/* UARTDM_IPR (R/W) */
#define BIT_STALE_TIMEOUT_MSB	0xFFFFFF80
#define BIT_STALE_TIMEOUT_LSB	0x001F
/* UARTDM_IRDA (W) */
#define BIT_MEDIUM_RATE_EN	0x0010
#define BIT_INVERT_IRDA_RX	0x0002
#define BIT_IRDA_EN		0x0001
/* UARTDM_RX_TOTAL_SNAP (R) */
#define BIT_RX_TOTAL_BYTES	0x00FFFFFF
/* UARTDM_DMEN (R/W) */
#define BIT_RX_DM_EN		0x0002
#define BIT_TX_DM_EN		0x0001

/* Command Definition */
/* Channel Commands (Bit11)---(Bit7)(Bit6)(Bit5)(Bit4) */
#define CCMD_NULL_COMMAND		0x00
#define CCMD_RESET_RECEIVER		0x10
#define CCMD_RESET_TRANSMITTER		0x20
#define CCMD_RESET_ERROR_STATUS		0x30
#define CCMD_BREAK_CHANGE_INTERRUPT	0x40
#define CCMD_START_BREAK		0x50
#define CCMD_STOP_BREAK			0x60
#define CCMD_RESET_CTS_N		0x70
#define CCMD_RESET_STALE_INTERRUPT	0x80
#define CCMD_PACKET_MODE		0x90
#define CCMD_MODE_RESET			0xC0
#define CCMD_SET_RFR_N			0xD0
#define CCMD_RESET_RFR_N		0xE0
#define CCMD_RESET_TX_ERROR		0x800
#define CCMD_CLEAR_TX_DONE		0x810
#define CCMD_RESET_BREAK_START_INTERRUPT	0x820
#define CCMD_BREAK_END_INTERRUPT	0x830
#define CCMD_RESET_PER_FRAME_ERR_INTERRUPT	0x840

/* General Commands -(Bit10)(Bit9)(Bit8)---- */
#define GCMD_NULL_COMMAND		0x000
#define GCMD_CR_PROTECTION_ENABLE	0x100
#define GCMD_CR_PROTECTION_DISABLE	0x200
#define GCMD_RESET_TX_READY_INTERRUPT	0x300
#define GCMD_ENABLE_STALE_EVENT		0x500
#define GCMD_DISABLE_STALE_EVENT	0x600
#define GCMD_SW_FORCE_STALE		0x400

/* PM MISC reg*/
#define	SSBI_REG_ADDR_MISC		0x1CC

/* IrDA UART2DM settings */
#define IRUART_UARTDM_CLK(bps)  (((bps) > 460800) ? (bps * 16) : 7372800)
#define IRUART_DEF_BAUDRATE_CSR	0x55		/* 9600 bps */
#define IRUART_DEF_RXSTALE	32		/* character times */
#define IRUART_DATA_STOP_PARITY	0x34		/* 8N1 */

#define IRUART_FSYNC_TIMEOUT (3*HZ)

/* IrDA UART settings */
#define IRUART_IRDA_EN          (BIT_INVERT_IRDA_RX | BIT_IRDA_EN)
#define IRUART_IRDA_DISABLE     0x0

enum irda_state {
	IRDA_UART_CLOSE,
	IRDA_UART_OPEN,
};
enum irda_overflow {
	IRDA_NORMAL,
	IRDA_OVERFLOW,
};
enum irda_rx_state {
	IRDA_RX_IDLE,
	IRDA_RX_START,
	IRDA_RX_CB,
};

struct pm8058_nfc_device {
	struct mutex		nfc_mutex;
	struct pm8058_chip	*pm_chip;
};

struct irda_driver_data {
	struct irda_platform_data	*pfdata;

	struct pm8058_nfc_device	*nfcdev;

	enum irda_state			state;
	enum irda_overflow		rx_overflow;
	enum irda_rx_state		rx_state;
	int				bps;

	int				refcount;
	struct semaphore		sem;
	spinlock_t			lock;

	u32				imr_reg;
	wait_queue_head_t		wait_tx;
	wait_queue_head_t		wait_rx;
	int				condition_tx;
	int				condition_rx;

	struct tasklet_struct		tasklet_rxfer_s;
	struct tasklet_struct		tasklet_rxfer_e;
	struct tasklet_struct		tasklet_txfer;

	u8				*vaddr_uartdm;
	struct resource			*iores_uartdm;
	struct clk			*clk_uartdm;

	struct irtxbuf_all		txbuf;
	struct irrxbuf_all		rxbuf;

	struct msm_dmov_cmd		txdmov_cmd;
	dmov_box			*txbox;
	u32				*txbox_ptr;
	dma_addr_t			mapped_txbox;
	dma_addr_t			mapped_txbox_ptr;

	struct msm_dmov_cmd		rxdmov_cmd;
	dmov_box			*rxbox;
	u32				*rxbox_ptr;
	dma_addr_t			mapped_rxbox;
	dma_addr_t			mapped_rxbox_ptr;
};

/* Local datas */
static struct irda_driver_data *irc;

/* Local functions */
static u32  msm_uartdm_read(u32 offset);
static void msm_uartdm_write(u32 offset, u32 value);
static void irda_txbuf_clear(struct irtxbuf_all *);
static void irda_rxbuf_clear(struct irrxbuf_all *);
static void irda_txdmov_callback(
struct msm_dmov_cmd *, unsigned int, struct msm_dmov_errdata *);
static void irda_rxdmov_callback(
struct msm_dmov_cmd *, unsigned int, struct msm_dmov_errdata *);
static void irda_uart_txfer_exec(unsigned long);
static void irda_uart_rxfer_start(unsigned long);
static void irda_uart_rxfer_end(unsigned long);
static irqreturn_t irda_uart_irq_handler(int, void *);
static unsigned int irda_baudrate_set(unsigned int bps);


static unsigned int msm_uartdm_read(u32 offset)
{
	if (offset >= UARTDM_SIZE) {
		printk(KERN_ERR DRV_NAME ": msm_uartdm_read invalid offset\n");
		return 0x0;
	}
	return ioread32(irc->vaddr_uartdm + offset);
}

static void msm_uartdm_write(u32 offset, u32 value)
{
	if (offset >= UARTDM_SIZE) {
		printk(KERN_ERR DRV_NAME ": msm_uartdm_write invalid offset\n");
		return;
	}
	iowrite32(value, irc->vaddr_uartdm + offset);
}

static void irda_txbuf_clear(struct irtxbuf_all *p_txbuf)
{
	p_txbuf->ptr = 0;
	p_txbuf->num = 0;
}

static void irda_rxbuf_clear(struct irrxbuf_all *p_rxbuf)
{
	p_rxbuf->wp = 0;
	p_rxbuf->rp = 0;
}

static void irda_txdmov_callback(
	struct msm_dmov_cmd *cmd_ptr,
	unsigned int result,
	struct msm_dmov_errdata *err)
{
	unsigned long flags;

	WARN_ON(result != 0x80000002);  /* DMA did not finish properly */

	spin_lock_irqsave(&irc->lock, flags);
	irc->imr_reg |= BIT_TX_READY;
	msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
	spin_unlock_irqrestore(&irc->lock, flags);
}

static void irda_rxdmov_callback(
	struct msm_dmov_cmd *cmd_ptr, unsigned int result,
	struct msm_dmov_errdata *err)
{
	if (irc->rx_state == IRDA_RX_START) {
		irc->rx_state = IRDA_RX_CB;
		tasklet_schedule(&irc->tasklet_rxfer_e);
	} else {
		printk(KERN_INFO DRV_NAME ": rxdmov callback is invalid.\n");
	}
}

static void irda_uart_rxfer_start(unsigned long data)
{
	unsigned long flags;

	spin_lock_irqsave(&irc->lock, flags);

	if (irc->rx_state != IRDA_RX_IDLE) {
		printk(KERN_INFO DRV_NAME ": rx state is not idle (%d).\n",
			irc->rx_state);
		goto out;
	}
	irc->rx_state = IRDA_RX_START;

	/* Update DMOV destination */
	irc->rxbox->dst_row_addr = irc->rxbuf.dmabase;

	/* Enqueue RX DMOV request */
	msm_dmov_enqueue_cmd(irc->pfdata->chan_uartdm_rx, &irc->rxdmov_cmd);

	/* Write DMRX register*/
	msm_uartdm_write(UARTDM_DMRX, IRUART_SW_RXBUF_SIZE);

	/* Enable RX stale*/
	irc->imr_reg |= BIT_RXSTALE;
	msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
	msm_uartdm_write(UARTDM_CR, GCMD_ENABLE_STALE_EVENT);

out:
	spin_unlock_irqrestore(&irc->lock, flags);
}

static void irda_uart_rxfer_end(unsigned long data)
{
	int num, wp, rp, wlen1, wlen2, remd;
	unsigned long flags;

	spin_lock_irqsave(&irc->lock, flags);

	irc->rx_state = IRDA_RX_IDLE;

	/* Disable and Reset RXSTALE */
	irc->imr_reg &= ~BIT_RXSTALE;
	msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
	msm_uartdm_write(UARTDM_CR, GCMD_DISABLE_STALE_EVENT);
	msm_uartdm_write(UARTDM_CR, CCMD_RESET_STALE_INTERRUPT);

	if (irc->rx_overflow == IRDA_OVERFLOW) {
		printk(KERN_ERR DRV_NAME ": RX SW buf overflowed.\n");
		num = 0;
		goto out;
	}

	/* Update SW RX ring buffer */
	num = msm_uartdm_read(UARTDM_RX_TOTAL_SNAP) & BIT_RX_TOTAL_BYTES;
	wlen1 = 0;
	wlen2 = 0;
	wp = irc->rxbuf.wp;
	rp = irc->rxbuf.rp;

	/* Get remainder of ring buffer */
	remd = 0;
	if (wp < rp)
		remd = irc->rxbuf.rp - irc->rxbuf.wp;
	else
		remd = (IRUART_SW_RXRINGBUF_SIZE - irc->rxbuf.wp)
			+ irc->rxbuf.rp;
	if (remd <= num) {
		irc->rx_overflow = IRDA_OVERFLOW;
		printk(KERN_ERR DRV_NAME ": RX SW buf overflowed.\n");
		printk(KERN_ERR DRV_NAME ": num(%d), remd(%d).\n", num, remd);
		num = remd;
	}

	if ((wp + num) > IRUART_SW_RXRINGBUF_SIZE) {
		wlen1 = IRUART_SW_RXRINGBUF_SIZE - wp;
		wlen2 = num - wlen1;
	} else {
		wlen1 = num;
		wlen2 = 0;
	}
	if (wlen1) {
		memcpy((irc->rxbuf.ringbuf + wp),
			irc->rxbuf.buf, wlen1);
		wp += wlen1;
	}
	if (wlen2) {
		wp = 0;
		memcpy((irc->rxbuf.ringbuf + wp),
			(irc->rxbuf.buf + wlen1), wlen2);
		wp += wlen2;
	}
	irc->rxbuf.wp = wp;

	if (irc->rxbuf.wp >= IRUART_SW_RXRINGBUF_SIZE)
		irc->rxbuf.wp -= IRUART_SW_RXRINGBUF_SIZE;

	if (irc->rx_overflow != IRDA_OVERFLOW &&
			(num == IRUART_SW_RXBUF_SIZE ||
			(irc->bps == 115200 &&
			(num == 128 || num == 256 || num == 384)))) {
		printk(KERN_ERR DRV_NAME ": retry rxfer_start.\n");
		spin_unlock_irqrestore(&irc->lock, flags);
		irda_uart_rxfer_start(0);
		spin_lock_irqsave(&irc->lock, flags);
	} else {
out:
		/* Enable RXLEV */
		irc->imr_reg |= BIT_RXLEV;
		msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
	}
	irc->condition_rx = 1;
	wake_up_interruptible(&irc->wait_rx);
	spin_unlock_irqrestore(&irc->lock, flags);
}

static void irda_uart_txfer_exec(unsigned long data)
{
	int rest;
	unsigned long flags;

	spin_lock_irqsave(&irc->lock, flags);

	rest = irc->txbuf.num - irc->txbuf.ptr;
	if (rest > 0) {
		if (rest > IRUART_SW_TXBUF_SIZE)
			rest = IRUART_SW_TXBUF_SIZE;
		irc->txbox->src_row_addr = irc->txbuf.dmabase + irc->txbuf.ptr;
		irc->txbox->num_rows =
			((rest + 15) >> 4 << 16) | (rest + 15) >> 4;
		msm_dmov_enqueue_cmd(irc->pfdata->chan_uartdm_tx,
					&irc->txdmov_cmd);
		msm_uartdm_write(UARTDM_NCF_TX, rest);
		irc->txbuf.ptr += rest;
	}

	spin_unlock_irqrestore(&irc->lock, flags);
}

static irqreturn_t irda_uart_irq_handler(int irq, void *dev)
{
	u32 isr_reg;
	unsigned long flags;

	spin_lock_irqsave(&irc->lock, flags);

	isr_reg = msm_uartdm_read(UARTDM_ISR);

	/* TX Ready (TX completion) */
	if (isr_reg & BIT_TX_READY) {
		irc->imr_reg &= ~BIT_TX_READY;
		msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
		msm_uartdm_write(UARTDM_CR, GCMD_RESET_TX_READY_INTERRUPT);
		if (irc->txbuf.num - irc->txbuf.ptr) {
			tasklet_schedule(&irc->tasklet_txfer);
		} else {
			u32 sr_reg = msm_uartdm_read(UARTDM_SR);
			while (!(sr_reg & BIT_TXEMT)) {
				udelay(1); /* delay 1us */
				sr_reg = msm_uartdm_read(UARTDM_SR);
			}

			msm_uartdm_write(UARTDM_CR, BIT_UART_RX_EN);
			irc->condition_tx = 1;
			wake_up_interruptible(&irc->wait_tx);
		}
	}
	/* RXLEV (1byte recognition)*/
	if (isr_reg & BIT_RXLEV) {
		irc->imr_reg &= ~BIT_RXLEV;
		msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
		tasklet_schedule(&irc->tasklet_rxfer_s);
	}
	/* RX Stale (RXDM timeout)*/
	if (isr_reg & BIT_RXSTALE) {
		irc->imr_reg &= ~BIT_RXSTALE;
		msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
		msm_dmov_flush(irc->pfdata->chan_uartdm_rx);
	}

	spin_unlock_irqrestore(&irc->lock, flags);

	return IRQ_HANDLED;
}

static unsigned int irda_baudrate_set(unsigned int bps)
{
	unsigned long rxstale;
	unsigned long iprdata;
	u32           sr_reg;

	/* check the transmitter under-runs. */
	sr_reg = msm_uartdm_read(UARTDM_SR);
	if (!(sr_reg&BIT_TXEMT))
		msleep(1);
	if (!(sr_reg&BIT_TXEMT))
		return -EBUSY;

	switch (bps) {
	case B2400:
		bps = 2400;
		msm_uartdm_write(UARTDM_CSR, 0x33);
		rxstale = 1;
		break;
	case B9600:
		bps = 9600;
		msm_uartdm_write(UARTDM_CSR, 0x55);
		rxstale = 2;
		break;
	case B19200:
		bps = 19200;
		msm_uartdm_write(UARTDM_CSR, 0x77);
		rxstale = 4;
		break;
	case B38400:
		bps = 38400;
		msm_uartdm_write(UARTDM_CSR, 0x99);
		rxstale = 8;
		break;
	case B57600:
		bps = 57600;
		msm_uartdm_write(UARTDM_CSR, 0xaa);
		rxstale = 16;
		break;
	case B115200:
		bps = 115200;
		msm_uartdm_write(UARTDM_CSR, 0xcc);
		rxstale = 31;
		break;
	default:
		/* default to 9600 */
		printk(KERN_WARNING DRV_NAME
		": changed %d bps into 9600 bps of the default value.\n",
			bps);
		bps = 9600;
		msm_uartdm_write(UARTDM_CSR, 0x55);
		rxstale = 2;
		break;
	}

	irc->bps = bps;
	if (clk_set_rate(irc->clk_uartdm, IRUART_UARTDM_CLK(bps))) {
		printk(KERN_WARNING DRV_NAME
			": error setting clock rate on UART\n");
		return -EIO;
	}

	iprdata = rxstale & BIT_STALE_TIMEOUT_LSB;
	iprdata |= BIT_STALE_TIMEOUT_MSB & (rxstale << 2);

	msm_uartdm_write(UARTDM_IPR, iprdata);

	return 0;
}

static int irda_uart_open(struct inode *inode, struct file *file)
{
	int ret;
	u8 misc = 0x0;

	down(&irc->sem);
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);

	/* Check refcount */
	if (irc->refcount > 0) {
		irc->refcount++;
		printk(KERN_INFO DRV_NAME
			": refirence counter count up(%d).\n", irc->refcount);
		up(&irc->sem);
		return 0;
	}
	/* PM8058-NFC Request */
	irc->nfcdev = pm8058_nfc_request();
	if (irc->nfcdev == NULL) {
		printk(KERN_ERR DRV_NAME ": pm8058 nfc not found.\n");
		ret = -ENODEV;
		goto error_pm_nfc_request;
	}

	/* PM8058 UART MUX setting */
	ret = pm8058_read(irc->nfcdev->pm_chip, SSBI_REG_ADDR_MISC, &misc, 1);
	if (ret)
		printk(KERN_ERR DRV_NAME
			": cannot read pm8058 UART MUX reg.\n");
	else
		printk(KERN_INFO DRV_NAME ": misc register = %x\n", misc);
	misc &= PM8058_UART_MUX_MASK;
	switch (misc) {
	case PM8058_UART_MUX_NO:
		pm8058_misc_control(irc->nfcdev->pm_chip,
				PM8058_UART_MUX_MASK, PM8058_UART_MUX_3);
		printk(KERN_INFO DRV_NAME
			": OK. UART MUX = neutral --> IrDA.\n");
		break;
	case PM8058_UART_MUX_1:
		printk(KERN_ERR DRV_NAME ": Now, uart_mux = 1 (FeliCa)\n");
		ret = -EBUSY;
		goto err_set_uart_mux;
	case PM8058_UART_MUX_2:
		printk(KERN_ERR DRV_NAME ": Now, uart_mux = 2 (unknown)\n");
		ret = -EBUSY;
		goto err_set_uart_mux;
	default:
		printk(KERN_ERR DRV_NAME ": UART MUX unavaible.\n");
		ret = -EIO;
		goto err_set_uart_mux;
	}

	/* init IMR value */
	irc->imr_reg = 0x0;

	/* init wait queue */
	init_waitqueue_head(&irc->wait_tx);
	init_waitqueue_head(&irc->wait_rx);

	/* init tasklet */
	tasklet_init(&irc->tasklet_rxfer_s, irda_uart_rxfer_start, 0);
	tasklet_init(&irc->tasklet_rxfer_e, irda_uart_rxfer_end,   0);
	tasklet_init(&irc->tasklet_txfer,   irda_uart_txfer_exec,  0);

	/* Register UARTDM IRQ */
	ret = request_irq(irc->pfdata->irq_uartdm, irda_uart_irq_handler,
				IRQF_TRIGGER_HIGH,  "irda_uart", irc);
	if (ret) {
		printk(KERN_ERR DRV_NAME ": request IRQ failed. (UARTDM)\n");
		goto err_request_irq_uart;
	}

	/* UARTDM ioremap */
	/* memory protection */
	irc->iores_uartdm = request_mem_region((u32) irc->pfdata->paddr_uartdm,
						UARTDM_SIZE, "irda_uart");
	if (!irc->iores_uartdm) {
		printk(KERN_ERR DRV_NAME
			": UARTDM request_mem_region failed.\n");
		ret = -EBUSY;
		goto err_request_mem_region;
	}
	irc->vaddr_uartdm = ioremap_nocache((u32) irc->pfdata->paddr_uartdm,
						UARTDM_SIZE);
	if (!irc->vaddr_uartdm) {
		printk(KERN_ERR DRV_NAME ": UARTDM ioremap failed.\n");
		ret = -ENOMEM;
		goto err_ioremap_uartdm;
	}

	/* UARTDM clock set and start */
	/* default 9600 bps */
	irc->bps = 9600;
	clk_set_rate(irc->clk_uartdm, IRUART_UARTDM_CLK(9600));
	clk_enable(irc->clk_uartdm);
	msm_uartdm_write(UARTDM_CSR, IRUART_DEF_BAUDRATE_CSR);

	/* UARTDM register setting */
	/* Data-stop-parity setting (MR2) */
	msm_uartdm_write(UARTDM_MR2, IRUART_DATA_STOP_PARITY);

	/* RX&TX wartermark setting */
	msm_uartdm_write(UARTDM_TFWR, 0x0);
	msm_uartdm_write(UARTDM_RFWR, 0x0);

	/* Stale time-out setting */
	msm_uartdm_write(UARTDM_IPR,
			(IRUART_DEF_RXSTALE & BIT_STALE_TIMEOUT_LSB)
			| ((IRUART_DEF_RXSTALE << 2) & BIT_STALE_TIMEOUT_MSB));

	/* Enable TXDM and RXDM mode */
	msm_uartdm_write(UARTDM_DMEN, BIT_RX_DM_EN|BIT_TX_DM_EN);

	/* Enable the IRDA transceiver */
	msm_uartdm_write(UARTDM_IRDA, IRUART_IRDA_EN);

	/* TX DMOV mapping */
	irc->txbox = dma_alloc_coherent(NULL, sizeof(dmov_box),
					&irc->mapped_txbox, GFP_KERNEL);
	if (!irc->txbox) {
		printk(KERN_ERR DRV_NAME ": no enough mem for TX DMOV(1).\n");
		ret = -ENOMEM;
		goto err_alloc_txbox;
	}
	irc->txbox_ptr = dma_alloc_coherent(NULL, sizeof(u32 *),
					&irc->mapped_txbox_ptr, GFP_KERNEL);
	if (!irc->txbox_ptr) {
		printk(KERN_ERR DRV_NAME ": no enough mem for TX DMOV(2).\n");
		ret = -ENOMEM;
		goto err_alloc_txbox_ptr;
	}
	*irc->txbox_ptr = CMD_PTR_LP | DMOV_CMD_ADDR(irc->mapped_txbox);
	irc->txdmov_cmd.cmdptr = DMOV_CMD_ADDR(irc->mapped_txbox_ptr);
	irc->txdmov_cmd.complete_func = irda_txdmov_callback;
	irc->txdmov_cmd.cmdptr = DMOV_CMD_ADDR(irc->mapped_txbox_ptr);
	irc->txbox->cmd =
		CMD_LC
		| CMD_DST_CRCI(irc->pfdata->crci_uartdm_tx) | CMD_MODE_BOX;
	/* TX DMOV BOX command */
	irc->txbox->src_row_addr = 0x0;
	irc->txbox->dst_row_addr = (u32)irc->pfdata->paddr_uartdm + UARTDM_TF;
	irc->txbox->src_dst_len = (UARTDM_BURST_SIZE<<16)|UARTDM_BURST_SIZE;
	irc->txbox->num_rows = 0x0;
	irc->txbox->row_offset = (UARTDM_BURST_SIZE<<16);

	/* RX DMOV mapping */
	irc->rxbox = dma_alloc_coherent(NULL, sizeof(dmov_box),
					&irc->mapped_rxbox, GFP_KERNEL);
	if (!irc->rxbox) {
		printk(KERN_ERR DRV_NAME ": no enough mem for RX DMOV(1).\n");
		ret = -ENOMEM;
		goto err_alloc_rxbox;
	}
	irc->rxbox_ptr = dma_alloc_coherent(NULL, sizeof(u32 *),
					&irc->mapped_rxbox_ptr, GFP_KERNEL);
	if (!irc->rxbox_ptr) {
		printk(KERN_ERR DRV_NAME ": no enough mem for RX DMOV(2).\n");
		ret = -ENOMEM;
		goto err_alloc_rxbox_ptr;
	}
	*irc->rxbox_ptr = CMD_PTR_LP | DMOV_CMD_ADDR(irc->mapped_rxbox);
	irc->rxdmov_cmd.cmdptr = DMOV_CMD_ADDR(irc->mapped_rxbox_ptr);
	irc->rxdmov_cmd.complete_func = irda_rxdmov_callback;
	irc->rxdmov_cmd.cmdptr = DMOV_CMD_ADDR(irc->mapped_rxbox_ptr);
	irc->rxbox->cmd =
		CMD_LC
		| CMD_SRC_CRCI(irc->pfdata->crci_uartdm_rx)
		| CMD_MODE_BOX;
	/* RX DMOV BOX command */
	irc->rxbox->src_row_addr =
		(u32)irc->pfdata->paddr_uartdm + UARTDM_RF;
	irc->rxbox->dst_row_addr = 0x0;
	irc->rxbox->src_dst_len = (UARTDM_BURST_SIZE<<16)|UARTDM_BURST_SIZE;
	irc->rxbox->num_rows =
		(IRUART_SW_RXBUF_SIZE >> 4 << 16) | IRUART_SW_RXBUF_SIZE >> 4;
	irc->rxbox->row_offset = UARTDM_BURST_SIZE;

	/* Enable Command Register Protection */
	msm_uartdm_write(UARTDM_CR, GCMD_CR_PROTECTION_ENABLE);

	/* Reset TX and RX */
	msm_uartdm_write(UARTDM_CR, CCMD_RESET_RECEIVER);
	msm_uartdm_write(UARTDM_CR, CCMD_RESET_TRANSMITTER);
	msm_uartdm_write(UARTDM_CR, CCMD_RESET_ERROR_STATUS);
	msm_uartdm_write(UARTDM_CR, CCMD_RESET_STALE_INTERRUPT);

	/* Setting PM power on (Low) */
	ret = pm8058_gpio_config(irc->pfdata->gpio_pow,
				 irc->pfdata->gpio_pwcfg_low);
	if (ret) {
		printk(KERN_ERR DRV_NAME ": pmic gpio write failed\n");
		goto err_gpio_config;
	}
	/* Wait 200usec */
	udelay(200);

	/* Enable Transmitter and Receiver */
	msm_uartdm_write(UARTDM_CR, BIT_UART_TX_EN);
	msm_uartdm_write(UARTDM_CR, BIT_UART_RX_EN);

	/* Clear UART SW buffer */
	irda_txbuf_clear(&irc->txbuf);
	irda_rxbuf_clear(&irc->rxbuf);

	/* init Overflow flag */
	irc->rx_overflow = IRDA_NORMAL;

	/* Increment refcount */
	irc->refcount++;

	/* (state change)--> IRDA_UART_OPEN */
	irc->state = IRDA_UART_OPEN;
	irc->rx_state = IRDA_RX_IDLE;

	printk(KERN_INFO DRV_NAME
		": succecssfly opened, refcount = %d\n", irc->refcount);

	/* Activate RXLEV IRQ */
	irc->imr_reg |= BIT_RXLEV;
	msm_uartdm_write(UARTDM_IMR, irc->imr_reg);

	up(&irc->sem);
	return 0;

/* Error handling */
err_gpio_config:
	dma_free_coherent(NULL,
			sizeof(u32 *), irc->rxbox_ptr, irc->mapped_txbox_ptr);
err_alloc_rxbox_ptr:
	dma_free_coherent(NULL,
			sizeof(dmov_box), irc->rxbox, irc->mapped_rxbox);
err_alloc_rxbox:
	dma_free_coherent(NULL,
			sizeof(u32 *), irc->txbox_ptr, irc->mapped_txbox_ptr);
err_alloc_txbox_ptr:
	dma_free_coherent(NULL,
			sizeof(dmov_box), irc->txbox, irc->mapped_txbox);
err_alloc_txbox:
	msm_uartdm_write(UARTDM_IRDA, IRUART_IRDA_DISABLE);
	clk_disable(irc->clk_uartdm);
	iounmap(irc->vaddr_uartdm);
err_ioremap_uartdm:
	release_mem_region((u32) irc->pfdata->paddr_uartdm, UARTDM_SIZE);
err_request_mem_region:
	free_irq(irc->pfdata->irq_uartdm, irc);
err_request_irq_uart:
	tasklet_kill(&irc->tasklet_rxfer_s);
	tasklet_kill(&irc->tasklet_rxfer_e);
	tasklet_kill(&irc->tasklet_txfer);
	pm8058_misc_control(irc->nfcdev->pm_chip,
				PM8058_UART_MUX_MASK, PM8058_UART_MUX_NO);
err_set_uart_mux:
error_pm_nfc_request:
	up(&irc->sem);
	return ret;
}

static int irda_uart_release(struct inode *inode, struct file *file)
{
	down(&irc->sem);
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);

	/* Decrement refcount */
	irc->refcount--;

	/* Check refcount */
	if (irc->refcount == 0) {
		/* do discard flush */
		if (irc->rx_state == IRDA_RX_START) {
			irc->rx_state = IRDA_RX_IDLE;
			msm_dmov_stop_cmd(irc->pfdata->chan_uartdm_rx,
						&irc->rxdmov_cmd, 0);
		}

		/* Deactivate RXLEV IRQ */
		irc->imr_reg &= ~BIT_RXLEV;
		msm_uartdm_write(UARTDM_IMR, irc->imr_reg);
		/* (state change)--> IRDA_UART_OPEN */
		irc->state = IRDA_UART_CLOSE;
		/* Disable the IRDA transceiver */
		msm_uartdm_write(UARTDM_IRDA, IRUART_IRDA_DISABLE);
		/* Disable Transmitter and Receiver */
		msm_uartdm_write(UARTDM_CR, BIT_UART_TX_DISABLE);
		msm_uartdm_write(UARTDM_CR, BIT_UART_RX_DISABLE);

		/* Desable TXDM and RXDM mode */
		msm_uartdm_write(UARTDM_DMEN, IRUART_IRDA_DISABLE);

		/* Wait 1usec */
		udelay(1);
		/* Setting PM power off (Hig) */
		pm8058_gpio_config(irc->pfdata->gpio_pow,
				   irc->pfdata->gpio_pwcfg_hig);

		/* Clear UART SW buffer */
		irda_txbuf_clear(&irc->txbuf);
		irda_rxbuf_clear(&irc->rxbuf);
		/* Reset TX and RX*/
		msm_uartdm_write(UARTDM_CR, CCMD_RESET_RECEIVER);
		msm_uartdm_write(UARTDM_CR, CCMD_RESET_TRANSMITTER);
		msm_uartdm_write(UARTDM_CR, CCMD_RESET_ERROR_STATUS);
		msm_uartdm_write(UARTDM_CR, CCMD_RESET_STALE_INTERRUPT);
		/* Release DMOV data */
		dma_free_coherent(NULL, sizeof(dmov_box),
					irc->txbox, irc->mapped_txbox);
		dma_free_coherent(NULL, sizeof(dmov_box),
					irc->rxbox, irc->mapped_rxbox);
		dma_free_coherent(NULL, sizeof(u32 *),
					irc->txbox_ptr, irc->mapped_txbox_ptr);
		dma_free_coherent(NULL, sizeof(u32 *),
					irc->rxbox_ptr, irc->mapped_rxbox_ptr);
		/* UARTDM clock stop */
		clk_disable(irc->clk_uartdm);
		/* UARTDM iounmap */
		if (irc->iores_uartdm)
			release_mem_region((u32) irc->pfdata->paddr_uartdm,
						UARTDM_SIZE);
		iounmap(irc->vaddr_uartdm);
		/* Remove IRQ for UARTDM */
		free_irq(irc->pfdata->irq_uartdm, irc);
		/* Remove tasklets for UARTDM */
		tasklet_kill(&irc->tasklet_rxfer_s);
		tasklet_kill(&irc->tasklet_rxfer_e);
		tasklet_kill(&irc->tasklet_txfer);
		/* PM8058 UART MUX reset */
		pm8058_misc_control(irc->nfcdev->pm_chip,
				PM8058_UART_MUX_MASK, PM8058_UART_MUX_NO);
		{
			u8 misc = 0x0;
			int ret = pm8058_read(irc->nfcdev->pm_chip,
						SSBI_REG_ADDR_MISC, &misc, 1);
			if (ret)
				printk(KERN_ERR DRV_NAME
					": cannot read pm8058 UART MUX reg.\n");
			else
				printk(KERN_INFO DRV_NAME
					": misc register = %x\n", misc);
		}
	}
	printk(KERN_INFO DRV_NAME ": Closed. Refcount = %d\n", irc->refcount);
	up(&irc->sem);

	return 0;
}

static ssize_t irda_uart_read(
	struct file *file, char __user *buf,
	size_t count, loff_t *offset)
{
	int cnt, rest, srest1, srest2;
	unsigned long flags;
	int ret;

	down(&irc->sem);

	if (irc->state != IRDA_UART_OPEN) {
		ret = -EHOSTUNREACH;
		goto err_out;
	}

	spin_lock_irqsave(&irc->lock, flags);
	ret = (irc->rx_overflow == IRDA_NORMAL)
		&& (irc->rxbuf.wp == irc->rxbuf.rp);
	if (ret)
		irc->condition_rx = 0;
	spin_unlock_irqrestore(&irc->lock, flags);

	cnt = 0;
	if (ret) {
		ret = wait_event_interruptible(irc->wait_rx, irc->condition_rx);
		irc->condition_rx = 0;

		spin_lock_irqsave(&irc->lock, flags);
		ret = (irc->rx_overflow == IRDA_NORMAL)
			&& (irc->rxbuf.wp == irc->rxbuf.rp);
		spin_unlock_irqrestore(&irc->lock, flags);

		if (ret)
			goto out;
	}

	spin_lock_irqsave(&irc->lock, flags);
	if (irc->rxbuf.wp > irc->rxbuf.rp) {
		srest1 = irc->rxbuf.wp - irc->rxbuf.rp;
		srest2 = 0;
	} else {
		srest1 = IRUART_SW_RXRINGBUF_SIZE - irc->rxbuf.rp;
		srest2 = irc->rxbuf.wp;
	}
	rest = count - cnt;
	spin_unlock_irqrestore(&irc->lock, flags);

	if (rest > srest1)
		rest = srest1;
	ret = copy_to_user(buf + cnt,
				(irc->rxbuf.ringbuf + irc->rxbuf.rp), rest);
	if (ret) {
		ret = -EFAULT;
		goto err_out;
	}

	spin_lock_irqsave(&irc->lock, flags);
	/* Update RX buf status */
	irc->rxbuf.rp += rest;
	if (irc->rxbuf.rp >= IRUART_SW_RXRINGBUF_SIZE)
		irc->rxbuf.rp -= IRUART_SW_RXRINGBUF_SIZE;
	cnt += rest;
	spin_unlock_irqrestore(&irc->lock, flags);

	rest = count - cnt;
	if (!rest || !srest2)
		goto out;
	if (rest > srest2)
		rest = srest2;
	ret = copy_to_user(buf + cnt,
				(irc->rxbuf.ringbuf + irc->rxbuf.rp), rest);
	if (ret) {
		ret = -EFAULT;
		goto err_out;
	}

	spin_lock_irqsave(&irc->lock, flags);
	/* Update RX buf status */
	irc->rxbuf.rp += rest;
	cnt += rest;
	spin_unlock_irqrestore(&irc->lock, flags);

out:
	irc->rx_overflow = IRDA_NORMAL;

	up(&irc->sem);
	return cnt;

err_out:
	printk(KERN_ERR DRV_NAME ": %s error(%d)\n", __func__, ret);
	up(&irc->sem);
	return ret;
}

static ssize_t irda_uart_write(
	struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	int ret;

	down(&irc->sem);

	if (irc->state != IRDA_UART_OPEN) {
		printk(KERN_ERR DRV_NAME
			": inconsistent state(%d).\n", irc->state);
		ret = -EHOSTUNREACH;
		goto err_out;
	}

	if (count > IRUART_SW_TXBUF_SIZE) {
		printk(KERN_ERR DRV_NAME
			": too many characters for write(%d).\n", count);
		ret = -EINVAL;
		goto err_out;
	}

	irda_txbuf_clear(&irc->txbuf);

	if (copy_from_user(irc->txbuf.buf, buf, count)) {
		ret = -EFAULT;
		goto err_out;
	}

	irc->txbuf.num = count;

	irc->condition_tx = 0;
	msm_uartdm_write(UARTDM_CR, BIT_UART_RX_DISABLE);

	irda_uart_txfer_exec(0);

	if (!irc->condition_tx)
		ret = wait_event_interruptible(irc->wait_tx, irc->condition_tx);
	irc->condition_tx = 0;

	up(&irc->sem);
	return irc->txbuf.num;

err_out:
	up(&irc->sem);
	return ret;
}

static unsigned int irda_uart_poll(
	struct file *file, struct poll_table_struct *wait)
{
	unsigned long flags;
	int           ret;
	unsigned int  mask = 0;

	down(&irc->sem);

	if (irc->state != IRDA_UART_OPEN) {
		mask = POLLERR;
		goto out;
	}

	poll_wait(file, &irc->wait_rx, wait);
	spin_lock_irqsave(&irc->lock, flags);
	ret = (irc->rx_overflow == IRDA_NORMAL)
		&& (irc->rxbuf.wp == irc->rxbuf.rp);
	if (!ret)
		mask = POLLIN | POLLRDNORM;
	mask |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&irc->lock, flags);
out:
	up(&irc->sem);
	return mask;
}

#define IRUART_IOCTRL_BAUDRATE 0x0e000002
#define IRUART_IOCTRL_QRYINFRM 0x0e000005
static int irda_uart_ioctl(
	struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret;

	down(&irc->sem);

	switch (cmd) {
	case IRUART_IOCTRL_BAUDRATE:
		ret = irda_baudrate_set((unsigned int)arg);
		if (ret) {
			printk(KERN_ERR DRV_NAME ": baudrate set failed.\n");
			goto err_out;
		}
		break;
	default:
		printk(KERN_ERR DRV_NAME ": Error. Unsupported IOCTL.\n");
		ret = -ENOTTY;
		goto err_out;
	}

	up(&irc->sem);
	return 0;

err_out:
	up(&irc->sem);
	return ret;
}

static const struct file_operations irc_fops = {
	.owner		= THIS_MODULE,
	.read		= irda_uart_read,
	.write		= irda_uart_write,
	.poll		= irda_uart_poll,
	.ioctl		= irda_uart_ioctl,
	.open		= irda_uart_open,
	.release	= irda_uart_release,
};

static struct miscdevice irda_uart_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "irda_uart",
	.fops = &irc_fops,
};

static int __init irda_probe(struct platform_device *pdev)
{
	int ret;

	irc = kzalloc(sizeof(struct irda_driver_data), GFP_KERNEL);
	if (!irc) {
		printk(KERN_ERR DRV_NAME ": could not allocate memory.\n");
		ret = -ENOMEM;
		goto error_alloc_driver_data;
	}

	irc->pfdata = pdev->dev.platform_data;

	/* GPIO Setting */
	ret = irc->pfdata->gpio_init();
	if (ret && ret != -EBUSY) {
		printk(KERN_ERR DRV_NAME ": gpio initialize failed.\n");
		ret = -EIO;
		goto error_gpio_init;
	}

	/* Platform Clock */
	irc->clk_uartdm = clk_get(irc->pfdata->clk_dev, irc->pfdata->clk_str);
	if (IS_ERR(irc->clk_uartdm)) {
		printk(KERN_ERR DRV_NAME ": clock assignment failed.\n");
		ret = -ENODEV;
		goto error_get_platform_clock;
	}
	/* init Ref count */
	irc->refcount = 0;

	/* Alloc RX ring buffer */
	irc->rxbuf.ringbuf = kzalloc(sizeof(u8)*(
					IRUART_SW_RXRINGBUF_SIZE
					+ IRUART_SW_RXBUF_SIZE),
				GFP_KERNEL);
	if (!irc->rxbuf.ringbuf) {
		printk(KERN_ERR DRV_NAME
			": could not allocate memory for RX ring buffer.\n");
		ret = -ENOMEM;
		goto error_alloc_rx_ringbuf;
	}

	/* Alloc TX SW Buffer */
	irc->txbuf.buf = dma_alloc_coherent(NULL,
					sizeof(u8)*IRUART_SW_TXBUF_SIZE,
					&irc->txbuf.dmabase, GFP_KERNEL);
	if (!irc->txbuf.buf) {
		printk(KERN_ERR DRV_NAME ": no enough mem for TXbuf.\n");
		ret = -ENOMEM;
		goto err_alloc_tx_buffer;
	}

	/* Alloc RX SW Buffer */
	irc->rxbuf.buf = dma_alloc_coherent(NULL,
					sizeof(u8)*IRUART_SW_RXBUF_SIZE,
					&irc->rxbuf.dmabase, GFP_KERNEL);
	if (!irc->rxbuf.buf) {
		printk(KERN_ERR DRV_NAME ": no enough mem for RXbuf.\n");
		ret = -ENOMEM;
		goto err_alloc_rx_buffer;
	}

	irc->rx_overflow = IRDA_NORMAL;

	if (misc_register(&irda_uart_device)) {
		printk(KERN_ERR DRV_NAME ": cannot register /dev/irda.\n");
		ret = -EACCES;
		goto err_register_uart_device;
	}

	sema_init(&irc->sem, 1);
	spin_lock_init(&irc->lock);

	irc->state = IRDA_UART_CLOSE;

	return 0;

/* error handling */
err_register_uart_device:
	dma_free_coherent(NULL, sizeof(u8)*IRUART_SW_RXBUF_SIZE,
				irc->rxbuf.buf, irc->rxbuf.dmabase);
err_alloc_rx_buffer:
	dma_free_coherent(NULL, sizeof(u8)*IRUART_SW_TXBUF_SIZE,
				irc->txbuf.buf, irc->txbuf.dmabase);
err_alloc_tx_buffer:
	kfree(irc->rxbuf.ringbuf);
error_alloc_rx_ringbuf:
error_get_platform_clock:
error_gpio_init:
	kfree(irc);
error_alloc_driver_data:
	return ret;
}

static int __devexit irda_remove(struct platform_device *pdev)
{
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);

	misc_deregister(&irda_uart_device);

	dma_free_coherent(NULL, sizeof(u8)*IRUART_SW_TXBUF_SIZE,
				irc->txbuf.buf, irc->txbuf.dmabase);
	dma_free_coherent(NULL, sizeof(u8)*IRUART_SW_RXBUF_SIZE,
				irc->rxbuf.buf, irc->rxbuf.dmabase);
	kfree(irc);
	return 0;
}

static int irda_suspend(struct platform_device *pdev,
			  pm_message_t message)
{
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);
	return 0;
}

static int irda_resume(struct platform_device *pdev)
{
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);
	return 0;
}

static struct platform_driver semc_msm_irda = {
	.probe		= irda_probe,
	.remove		= irda_remove,
	.suspend	= irda_suspend,
	.resume		= irda_resume,
	.driver		= {
		.name		= "semc-msm-irda",
		.owner		= THIS_MODULE,
	},
};

static int __init irda_init(void)
{
	int ret;
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);
	ret = platform_driver_register(&semc_msm_irda);

	return ret;
}

static void __exit irda_exit(void)
{
	printk(KERN_INFO DRV_NAME ": %s\n", __func__);
	platform_driver_unregister(&semc_msm_irda);
}

module_init(irda_init);
module_exit(irda_exit);

MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("Yoshida Manabu <Manabu.X.Yoshida@sonyericsson.com>");
MODULE_DESCRIPTION("SEMC MSM IrDA Driver");
MODULE_LICENSE("GPL");
