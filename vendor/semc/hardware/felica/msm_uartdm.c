/* vendor/semc/hardware/felica/msm_uartdm.c
 *
 * Copyright (C) 2010 Sony Ericsson Mobile Communications AB.
 *
 * Author: Hiroaki Kuriyama <Hiroaki.Kuriyama@sonyericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <mach/dma.h>
#include "semc_felica_ext.h"
#include "msm_uartdm.h"
#include "msm_uartmux.h"
#include "felica_rxbuf.h"
#include "felica_txbuf.h"

#define PRT_NAME "msm uartdm"
#define DMOV_NON_GRACEFUL	(0)
#define DMOV_GRACEFUL		(1)

struct msm_uartdm_data {
	/* Platform data */
	struct msm_uartdm_pfdata	*pfdata;
	/* Workqueue for callback */
	struct workqueue_struct		*uartdm_wq;
	struct work_struct	rcv_1byte_work;
	struct work_struct	rx_end_work;
	struct work_struct	rx_error_work;
	struct work_struct	tx_end_work;
	struct work_struct	tx_error_work;
	/* IO-remap of UARTDM port */
	char			*vaddr_uartdm;
	struct resource		*iores_uartdm;
	/* UARTDM clock */
	struct clk		*clk_uartdm;
	/* TX DMOV data */
	struct msm_dmov_cmd	txdmov_cmd;
	dmov_box		*txbox;
	u32			*txbox_ptr;
	dma_addr_t		mapped_txbox;
	dma_addr_t		mapped_txbox_ptr;
	/* RX DMOV data */
	struct msm_dmov_cmd	rxdmov_cmd;
	dmov_box		*rxbox;
	u32			*rxbox_ptr;
	dma_addr_t		mapped_rxbox;
	dma_addr_t		mapped_rxbox_ptr;
	/* Record of IMR registger */
	unsigned int		imr_reg;
	/* Record of IMR registger */
	spinlock_t		lock;
	/* Dummy distination for RX discard */
	void			*dummy_dst_buf;
	dma_addr_t		dummy_dst_dmabase;
};

static struct msm_uartdm_data *urdm;

/**
 * @brief  Read function of UARTDM register
 * @param  offset : Offset from UARTDM base address
 * @retval value  : Register value (32bit)
 * @note   If offset is out of range, error message is displayed.
 *         No error returns.
 */
static unsigned int msm_uartdm_read(unsigned int offset)
{
	if (UARTDM_SIZE <= offset) {
		pr_err(PRT_NAME ": Error. msm_uartdm_read invalid offset\n");
		return 0x0;
	}
	return ioread32(urdm->vaddr_uartdm + offset);
}

/**
 * @brief  Write function of UARTDM register
 * @param  offset : Offset from UARTDM base address
 * @param  value  : Register value to write (32bit)
 * @retval N/A
 * @note   If offset is out of range, error message is displayed.
 *         No error returns.
 */
static void msm_uartdm_write(unsigned int offset, unsigned int value)
{
	if (UARTDM_SIZE <= offset) {
		pr_err(PRT_NAME ": Error. msm_uartdm_write invalid offset\n");
		return;
	}
	iowrite32(value, urdm->vaddr_uartdm + offset);
}

/**
 * @brief   RX DMA completiton handling
 * @details This function executes;\n
 *            # Disable RX stale interrupt\n
 *            # UARTDM command (Disable stale event)\n
 *            # UARTDM command (Reset stale event)\n
 *            # [If RX DMA succeeded,] queue work of RX end\n
 *            # [If RX DMA failed,] queue work of RX error\n
 * @param   cmd_ptr : (unused)
 * @param   result  : Result value of the DMA transfer (32bit data)
 * @param   err     : (unused)
 * @retval  N/A
 * @note
 */
static void msm_uartdm_rxdmov_callback(struct msm_dmov_cmd *cmd_ptr,
			unsigned int result, struct msm_dmov_errdata *err)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Disable RX Stale interrupt */
	urdm->imr_reg &= ~BIT_RXSTALE;
	msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);
	/* UARTDM command (Disable stale event) */
	msm_uartdm_write(UART_DM_CR, GCMD_DISABLE_STALE_EVENT);
	/* UARTDM command (Reset stale event) */
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_STALE_INTERRUPT);

	if (result & DMOV_RSLT_ERROR)
		/* Queue work of RX end */
		queue_work(urdm->uartdm_wq, &urdm->rx_error_work);
	else
		/* Queue work of RX error */
		queue_work(urdm->uartdm_wq, &urdm->rx_end_work);
}

/**
 * @brief   TX DMOV completiton handling
 * @details This function executes;\n
 *            # [If TX DMA succeeded,] enable TX ready interrupt.\n
 *            # [If TX DMA failed,] queue work of TX error\n
 * @param   cmd_ptr : (unused)
 * @param   result  : Result value of the DMA transfer (32bit data)
 * @param   err     : (unused)
 * @retval  N/A
 */
static void msm_uartdm_txdmov_callback(struct msm_dmov_cmd *cmd_ptr,
			unsigned int result, struct msm_dmov_errdata *err)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (result & DMOV_RSLT_ERROR) {
		queue_work(urdm->uartdm_wq, &urdm->tx_error_work);
	} else {
		urdm->imr_reg |= BIT_TX_READY;
		msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);
	}
}

/**
 * @brief   RX DMOV reset
 * @details This function executes;\n
 *            # Order RX graceful flush of DMA
 * @param   N/A
 * @retval  N/A
 * @note    If no active RX-DMA command, msm_dmov_flush() does nothing.
 */
static void msm_uartdm_rxdmov_reset(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	msm_dmov_flush(urdm->pfdata->chan_uartdm_rx);
}

/**
 * @brief   TX DMOV reset
 * @details This function executes;\n
 *            # Order TX graceful flush of DMA
 * @param   N/A
 * @retval  N/A
 * @note    If no active TX-DMA command, msm_dmov_flush() does nothing.
 */
static void msm_uartdm_txdmov_reset(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	msm_dmov_flush(urdm->pfdata->chan_uartdm_tx);
}

/**
 * @brief   IRQ handler of MSM UARTDM
 * @details This function executes;\n
 *          [If ISR == RXLEV,]\n
 *            # Disable RXLEV interrupt\n
 *            # Queue work of 1byte reception\n
 *          [If ISR == RX stale,]\n
 *            # Disable RX stale interrupt\n
 *            # Order RX graceful flush of DMA\n
 *          [If ISR == TX ready,]\n
 *            # Disable TX ready interrupt\n
 *            # Reset command of TX ready\n
 *            # Queue work of TX end
 * @param   irq : (unused)
 * @param   dev : (unused)
 * @retval  IRQ_HANDLED : Success. IRQ handled.
 * @retval  IRQ_NONE    : Failure. Unknown IRQ reason.
 * @note
 */
static irqreturn_t msm_uartdm_irq_handler(int irq, void *dev)
{
	unsigned int isr_reg;
	unsigned long flags;

	pr_debug(PRT_NAME ": %s\n", __func__);

	spin_lock_irqsave(&urdm->lock, flags);

	isr_reg = msm_uartdm_read(UART_DM_ISR);

	pr_debug(PRT_NAME ": IMR = 0x%x, ISR =0x%x\n", urdm->imr_reg, isr_reg);

	/* RXLEV (1byte recognition)*/
	if (urdm->imr_reg & isr_reg & BIT_RXLEV) {
		urdm->imr_reg &= ~BIT_RXLEV;
		msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);
		queue_work(urdm->uartdm_wq, &urdm->rcv_1byte_work);
		pr_debug(PRT_NAME ": RX lev -> Rcv 1byte callback.\n");
	}
	/* RX Stale (RXDM timeout)*/
	else if (urdm->imr_reg & isr_reg & BIT_RXSTALE) {
		urdm->imr_reg &= ~BIT_RXSTALE;
		msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);
		msm_dmov_flush(urdm->pfdata->chan_uartdm_rx);
		pr_debug(PRT_NAME ": RX stale -> Graceful flush.\n");
	}
	/* TX Ready (TX completion) */
	else if (urdm->imr_reg & isr_reg & BIT_TX_READY) {
		urdm->imr_reg &= ~BIT_TX_READY;
		msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);
		msm_uartdm_write(UART_DM_CR, GCMD_RESET_TX_READY_INTERRUPT);
		queue_work(urdm->uartdm_wq, &urdm->tx_end_work);
		pr_debug(PRT_NAME ": TX ready.\n");
	}
	/* Unknown IRQ reason */
	else {
		spin_unlock_irqrestore(&urdm->lock, flags);
		return IRQ_NONE;
	}

	spin_unlock_irqrestore(&urdm->lock, flags);

	return IRQ_HANDLED;
}

/**
 * @brief   Start MSM UARTDM RX transfer
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # Check UARTDM RX overrun\n
 *            # Direct RX DMA path\n
 *            # Enable RX Stale IRQ\n
 *            # UARTDM command (Enable stale event)\n
 *            # Enqueue RX DMA command\n
 *            # Write DMRX register
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not opened.\n
 *            -EIO    = UARTDM RX overrun
 */
int msm_uartdm_rx_start(void)
{
	unsigned int sr;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	/* Check UARTDM RX overrun */
	sr = msm_uartdm_read(UART_DM_SR);
	if (sr & BIT_UART_OVERRUN) {
		pr_err(PRT_NAME ": Warning. UART RX overran.\n");
		msm_uartdm_write(UART_DM_CR, CCMD_RESET_ERROR_STATUS);
		return -EIO;
	}

	/* Direct RX DMA path */
	urdm->rxbox->dst_row_addr = felica_rxbuf_push_pointer();
	if (urdm->rxbox->dst_row_addr) {
		urdm->rxbox->row_offset = UARTDM_BURST_SIZE;
	} else {
		urdm->rxbox->dst_row_addr = urdm->dummy_dst_dmabase;
		urdm->rxbox->row_offset = 0x0;
	}

	/* Enable RX Stale IRQ */
	urdm->imr_reg |= BIT_RXSTALE;
	msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);

	/* UARTDM command (Enable stale event) */
	msm_uartdm_write(UART_DM_CR, GCMD_ENABLE_STALE_EVENT);

	/* Enqueue RX DMA command */
	msm_dmov_enqueue_cmd(urdm->pfdata->chan_uartdm_rx, &urdm->rxdmov_cmd);

	/* Write DMRX register */
	msm_uartdm_write(UART_DM_DMRX, LOCAL_UARTDM_RX_FIFO_SIZE);

	return 0;
}

/**
 * @brief   Reset MSM UARTDM RX port
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # UARTDM command (Disable UARTDM RX)\n
 *            # UARTDM command (Reset UARTDM RX)\n
 *            # UARTDM command (Reset Error&Stale)\n
 *            # Clear RX buffer\n
 *            # Call RX DMA reset\n
 *            # UARTDM command (Enable UARTDM RX)
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not opened.
 * @note
 */
int msm_uartdm_rx_reset(void)
{

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	msm_uartdm_write(UART_DM_CR, BIT_UART_RX_DISABLE);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_RECEIVER);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_ERROR_STATUS);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_STALE_INTERRUPT);
	felica_rxbuf_clear();
	msm_uartdm_rxdmov_reset();
	msm_uartdm_write(UART_DM_CR, BIT_UART_RX_EN);

	return 0;
}


/**
 * @brief   Start MSM UARTDM TX transfer
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # Count data in TX buffer\n
 *            # Get TX buffer pointer\n
 *            # Update TX buffer information
 *            # Enqueue TX DMA command\n
 *            # Write NO_CHARS_FOR_TX register\n
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure.\n
 *            -ENODEV  = UARTDM not opened / Cannot access TX buf\n
 *            -EFAULT  = Cannot update TX buf\n
 *            -ENODATA = No TX data exists.
 * @note
 */
int msm_uartdm_tx_start(void)
{
	int cnt, ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	/* Count data in TX buffer */
	cnt = felica_txbuf_count();

	if (!cnt) {
		pr_err(PRT_NAME ": Error. No TX data exists.\n");
		return -ENODATA;
	} else if (0 < cnt) {
		/* Get TX buf pointer */
		urdm->txbox->src_row_addr = felica_txbuf_pull_pointer();
		urdm->txbox->num_rows = (((cnt + 15) >> 4) << 16)
						| ((cnt + 15) >> 4);
		/* Update TX buffer information */
		ret = felica_txbuf_pull_status_update(cnt);
		if (ret) {
			pr_err(PRT_NAME ": Error. Cannot update TX buf.\n");
			return -EFAULT;
		}
		/* Enqueue TX DMA command */
		msm_dmov_enqueue_cmd(urdm->pfdata->chan_uartdm_tx,
						&urdm->txdmov_cmd);
		/* Write NO_CHAR_FOR_TX register */
		msm_uartdm_write(UART_DM_NO_CHARS_FOR_TX, cnt);
	} else {
		pr_err(PRT_NAME ": Error. Cannot access TX buf.\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * @brief   Reset MSM UARTDM TX port
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # UARTDM command (Disable UARTDM TX)\n
 *            # UARTDM command (Reset UARTDM TX)\n
 *            # UARTDM command (Reset Error)\n
 *            # Clear TX buffer\n
 *            # Call TX DMA reset\n
 *            # UARTDM command (Enable UARTDM TX)
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not opened.
 */
int msm_uartdm_tx_reset(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	msm_uartdm_write(UART_DM_CR, BIT_UART_TX_DISABLE);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_TRANSMITTER);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_ERROR_STATUS);
	felica_txbuf_clear();
	msm_uartdm_txdmov_reset();
	msm_uartdm_write(UART_DM_CR, BIT_UART_TX_EN);

	return 0;
}

/**
 * @brief   Enable interrupt of 1byte reception (RXLEV)
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # Enable UARTDM RXLEV IRQ
 * @param   N/A
 * @retval  0 : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not opened.
 * @note
 */
int msm_uartdm_rcv_1byte_enable(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	urdm->imr_reg |= BIT_RXLEV;
	msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);

	return 0;
}

/**
 * @brief   Disable interrupt of 1byte reception (RXLEV)
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # Disable UARTDM RXLEV IRQ
 * @param   N/A
 * @retval  0 : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not opened.
 * @note
 */
int msm_uartdm_rcv_1byte_disable(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	urdm->imr_reg &= ~BIT_RXLEV;
	msm_uartdm_write(UART_DM_IMR, urdm->imr_reg);

	return 0;
}

/**
 * @brief   Check data length in UARTDM RX FIFO
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # Read UARTDM DMRX register
 * @param   N/A
 * @retval  0/Positive : Success. Length of data in RX FIFO.
 * @retval  Negative   : Failure\n
 *            -ENODEV = UARTDM not opened.
 * @note
 */
int msm_uartdm_rcv_byte_check(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	return (int) msm_uartdm_read(UART_DM_DMRX) & BIT_RX_DM_CRCI_CHARS;
}

/**
 * @brief   Work function of RX 1byte reception
 * @details This function executes;\n
 *            # If registered, call the callback function.
 * @param   work : (unused)
 * @retval  N/A
 * @note
 */
static void msm_uartdm_rcv_1byte_work(struct work_struct *work)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (urdm->pfdata->callback_rcv_1byte)
		urdm->pfdata->callback_rcv_1byte();
}

/**
 * @brief   Work function of UARTDM RX End
 * @details This function executes;\n
 *            # [If UART opened,] check RX_TOTAL_BYTE register.\n
 *            # [If UART opened,] update RX buffer with the data length.\n
 *            # [If registered,]  call the callback function.
 * @param   work : (unused)
 * @retval  N/A
 * @note
 */
static void msm_uartdm_rx_end_work(struct work_struct *work)
{
	unsigned int cnt;

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (urdm->vaddr_uartdm) {
		cnt = msm_uartdm_read(UART_DM_RX_TOTAL_SNAP)
						& BIT_RX_TOTAL_BYTES;
		pr_debug(PRT_NAME ": RX_TOTAL_SNAP = %d\n", cnt);
		felica_rxbuf_push_status_update(cnt);
	} else
		pr_warning(PRT_NAME ": Warning. rx_end_work @ UART closed.\n");

	if (urdm->pfdata->callback_rx_complete)
		urdm->pfdata->callback_rx_complete();
}

/**
 * @brief   Work function of UARTDM RX Error
 * @details This function executes;\n
 *            # If registered, call the callback function.
 * @param   work : (unused)
 * @retval  N/A
 */
static void msm_uartdm_rx_error_work(struct work_struct *work)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (urdm->pfdata->callback_rx_error)
		urdm->pfdata->callback_rx_error();
}

/**
 * @brief   Work function of UARTDM TX End
 * @details This function executes;\n
 *            # If registered, call the callback function.
 * @param   work : (unused)
 * @retval  N/A
 * @note
 */
static void msm_uartdm_tx_end_work(struct work_struct *work)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (urdm->pfdata->callback_tx_complete)
		urdm->pfdata->callback_tx_complete();
}

/**
 * @brief   Work function of UARTDM TX Error
 * @details This function executes;\n
 *            # If registered, call the callback function.
 * @param   work : (unused)
 * @retval  N/A
 * @note
 */
static void msm_uartdm_tx_error_work(struct work_struct *work)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (urdm->pfdata->callback_tx_error)
		urdm->pfdata->callback_tx_error();
}

/**
 * @brief   Open MSM UARTDM port
 * @details This function executes;\n
 *            # Check UARTDM sub-module data\n
 *            # UARTDM IRQ registration\n
 *            # UARTDM MIMO remap\n
 *            # Start UARTDM clock \n
 *            # UARTDM register setting\n
 *            # UARTDM command (Cmd reg protection)\n
 *            # UARTDM command (Reset RX & TX)\n
 *            # UARTDM command (Reset Error & Stale)\n
 *            # UARTDM command (Enable RX & TX)\n
 *            # Clear TX & RX buffer\n
 *            # Init DMA command boxes (TX)\n
 *            # Init DMA command boxes (RX)\n
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not initialized / Request IRQ error /
 *                      Clock cannot be configured.\n
 *            -ENOMEM = UARTDM MMIO remap error
 */
int msm_uartdm_open(void)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM sub-module data */
	if (!urdm) {
		pr_err(PRT_NAME ": Error. UARTDM not initialized.\n");
		return -ENODEV;
	}

	/* UARTDM IRQ registration */
	ret = request_irq(urdm->pfdata->irq_uartdm, msm_uartdm_irq_handler,
	  IRQF_TRIGGER_HIGH, urdm->pfdata->irq_name, urdm->pfdata->clk_dev);
	if (ret) {
		pr_err(PRT_NAME ": Error. Request IRQ failed.\n");
		ret = -ENODEV;
		goto err_request_irq_uart;
	}
	urdm->imr_reg = 0x0;

	/* UARTDM MMIO remap */
	urdm->iores_uartdm = request_mem_region(
		(resource_size_t) urdm->pfdata->paddr_uartdm, UARTDM_SIZE,
						 urdm->pfdata->iomem_name);
	if (!urdm->iores_uartdm)
		pr_warning(PRT_NAME ": Warning. UARTDM IOMEM conflict.\n");
	urdm->vaddr_uartdm = ioremap_nocache(
		(unsigned long) urdm->pfdata->paddr_uartdm, UARTDM_SIZE);
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM IOremap failed.\n");
		ret = -ENOMEM;
		goto err_ioremap_uartdm;
	}
	pr_debug(PRT_NAME ": IOremap v-add 0x%x\n",
						(int) urdm->vaddr_uartdm);

	/* Start UARTDM clock */
	ret = clk_set_rate(urdm->clk_uartdm, LOCAL_UARTDM_BASE_FREQ);
	ret |= clk_enable(urdm->clk_uartdm);
	if (ret) {
		pr_err(PRT_NAME ": Error. UARTDM clock failed.\n");
		ret = -ENODEV;
		goto err_uartdm_clk_enable;
	}

	/* UARTDM register setting */
	/* + Baudrate setting */
	msm_uartdm_write(UART_DM_CSR, LOCAL_UARTDM_BAUDRATE_CSR);
	/* + Data-stop-parity setting (MR2) */
	msm_uartdm_write(UART_DM_MR2, LOCAL_UARTDM_DATA_STOP_PARITY);
	/* + UARTDM RX-TX buffer split setting */
	msm_uartdm_write(UART_DM_BADR, LOCAL_UARTDM_TX_RX_FIFO_SPLIT);
	/* + RX&TX wartermark setting */
	msm_uartdm_write(UART_DM_TFWR, 0x0);
	msm_uartdm_write(UART_DM_RFWR, 0x0);
	/* + Stale time-out setting */
	msm_uartdm_write(UART_DM_IPR,
		(LOCAL_UARTDM_STALE_TIMEOUT & BIT_STALE_TIMEOUT_LSB) |
		((LOCAL_UARTDM_STALE_TIMEOUT << 2) & BIT_STALE_TIMEOUT_MSB));
	/* + Enable TXDM and RXDM mode */
	msm_uartdm_write(UART_DM_DMEN, BIT_RX_DM_EN | BIT_TX_DM_EN);
	/* + Disable IrDA mode */
	msm_uartdm_write(UART_DM_IRDA, 0x0);

	/* UARTDM command (Cmd reg protection) */
	msm_uartdm_write(UART_DM_CR, GCMD_CR_PROTECTION_ENABLE);

	/* UARTDM command (Reset RX & TX) */
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_RECEIVER);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_TRANSMITTER);

	/* UARTDM command (Reset Error & Stale) */
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_ERROR_STATUS);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_STALE_INTERRUPT);

	/* UARTDM command (Enable RX & TX) */
	msm_uartdm_write(UART_DM_CR, BIT_UART_TX_EN);
	msm_uartdm_write(UART_DM_CR, BIT_UART_RX_EN);

	/* Clear TX & RX buffer */
	felica_txbuf_clear();
	felica_rxbuf_clear();

	/* Init DMA command boxes (TX) */
	/* + TX DMOV mapping */
	*urdm->txbox_ptr = CMD_PTR_LP | DMOV_CMD_ADDR(urdm->mapped_txbox);
	urdm->txdmov_cmd.cmdptr = DMOV_CMD_ADDR(urdm->mapped_txbox_ptr);
	urdm->txdmov_cmd.complete_func = msm_uartdm_txdmov_callback;
	urdm->txdmov_cmd.cmdptr = DMOV_CMD_ADDR(urdm->mapped_txbox_ptr);
	urdm->txbox->cmd = CMD_LC | CMD_DST_CRCI(urdm->pfdata->crci_uartdm_tx)
							| CMD_MODE_BOX;
	/* + TX DMOV BOX command */
	urdm->txbox->src_row_addr = 0x0;
	urdm->txbox->dst_row_addr = (u32) urdm->pfdata->paddr_uartdm
								+ UART_DM_TF;
	urdm->txbox->src_dst_len = (UARTDM_BURST_SIZE << 16)
							| UARTDM_BURST_SIZE;
	urdm->txbox->num_rows = 0x0;
	urdm->txbox->row_offset = (UARTDM_BURST_SIZE << 16);

	/* Init DMA command boxes (RX) */
	/* + RX DMOV mapping */
	*urdm->rxbox_ptr = CMD_PTR_LP | DMOV_CMD_ADDR(urdm->mapped_rxbox);
	urdm->rxdmov_cmd.cmdptr = DMOV_CMD_ADDR(urdm->mapped_rxbox_ptr);
	urdm->rxdmov_cmd.complete_func = msm_uartdm_rxdmov_callback;
	urdm->rxdmov_cmd.cmdptr = DMOV_CMD_ADDR(urdm->mapped_rxbox_ptr);
	urdm->rxbox->cmd = CMD_LC | CMD_SRC_CRCI(urdm->pfdata->crci_uartdm_rx)
							| CMD_MODE_BOX;
	/* + RX DMOV BOX command */
	urdm->rxbox->src_row_addr = (u32) urdm->pfdata->paddr_uartdm
							+ UART_DM_RF;
	urdm->rxbox->dst_row_addr = 0x0;	/* Will be set later */
	urdm->rxbox->src_dst_len = (UARTDM_BURST_SIZE << 16)
						| UARTDM_BURST_SIZE;
	urdm->rxbox->num_rows = ((LOCAL_UARTDM_RX_FIFO_SIZE >> 4) << 16)
					| (LOCAL_UARTDM_RX_FIFO_SIZE >> 4);
	urdm->rxbox->row_offset = 0x0;		/* Will be set later */

	return 0;

err_uartdm_clk_enable:
	iounmap(urdm->vaddr_uartdm);
	urdm->vaddr_uartdm = NULL;
	if (urdm->iores_uartdm)
		release_mem_region((u32) urdm->pfdata->paddr_uartdm,
							UARTDM_SIZE);
err_ioremap_uartdm:
	free_irq(urdm->pfdata->irq_uartdm, urdm->pfdata->clk_dev);
err_request_irq_uart:
	return ret;
}

/**
 * @brief   Close MSM UARTDM port
 * @details This function executes;\n
 *            # Check UARTDM port\n
 *            # Reset RX&TX DMA\n
 *            # Clear TX & RX buffer\n
 *            # UARTDM command (Disable RX & TX)\n
 *            # UARTDM command (Reset Error & Stale)\n
 *            # UARTDM command (Reset RX & TX)\n
 *            # Stop UARTDM clock\n
 *            # Release MIMO resource\n
 *            # Release UARTDM IRQ
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = UARTDM not opened.
 * @note
 */
int msm_uartdm_close(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UARTDM port */
	if (!urdm->vaddr_uartdm) {
		pr_err(PRT_NAME ": Error. UARTDM not opened.\n");
		return -ENODEV;
	}

	/* Reset RX&TX DMA */
	msm_uartdm_rxdmov_reset();
	msm_uartdm_txdmov_reset();

	/* Clear TX & RX buffer */
	felica_txbuf_clear();
	felica_rxbuf_clear();

	/* UARTDM command (Disable RX & TX) */
	msm_uartdm_write(UART_DM_CR, BIT_UART_TX_DISABLE);
	msm_uartdm_write(UART_DM_CR, BIT_UART_RX_DISABLE);

	/* UARTDM command (Reset Error & Stale) */
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_ERROR_STATUS);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_STALE_INTERRUPT);

	/* UARTDM command (Reset RX & TX) */
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_RECEIVER);
	msm_uartdm_write(UART_DM_CR, CCMD_RESET_TRANSMITTER);

	/* Stop UARTDM clock */
	clk_disable(urdm->clk_uartdm);

	/* Release MIMO resource */
	iounmap(urdm->vaddr_uartdm);
	urdm->vaddr_uartdm = NULL;
	if (urdm->iores_uartdm)
		release_mem_region((u32) urdm->pfdata->paddr_uartdm,
							UARTDM_SIZE);

	/* Release UARTDM IRQ */
	free_irq(urdm->pfdata->irq_uartdm, urdm->pfdata->clk_dev);

	return 0;
}

/**
 * @brief   Initialize MSM UARTDM port
 * @details This function executes;\n
 *            # Alloc and Initialize UARTDM sub-module data\n
 *            # Get UARTDM clock device\n
 *            # Create workqueue of UARTDM\n
 *            # Set UARTDM call back functions\n
 *            # Call RX buffer init\n
 *            # Call TX buffer init\n
 *            # Alloc dummy DMA distination\n
 *            # Alloc DMA command boxes (RX & TX)
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EINVAL = No platform data\n
 *            -ENOMEM = No enough memory / Cannot create workqueue /
 *                      RX/TX buffer initialization error
 *            -ENODEV = Clock device not found
 * @note
 */
int msm_uartdm_init(struct msm_uartdm_pfdata *pfdata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Alloc and Initialize UARTDM sub-module data */
	if (!pfdata) {
		pr_err(PRT_NAME ": Error. No platform data for MSM UARTDM.\n");
		return -EINVAL;
	}
	urdm = kzalloc(sizeof(struct msm_uartdm_data), GFP_KERNEL);
	if (!urdm) {
		pr_err(PRT_NAME ": Error. No enough mem for PON.\n");
		ret = -ENOMEM;
		goto err_alloc_uartdm_data;
	}
	urdm->pfdata = pfdata;

	/* Get UARTDM clock device */
	urdm->clk_uartdm = clk_get(urdm->pfdata->clk_dev,
					urdm->pfdata->clk_str);
	if (IS_ERR(urdm->clk_uartdm)) {
		pr_err(PRT_NAME ": Error. UARTDM clock failed.\n");
		ret = -ENODEV;
		goto err_get_uartdm_clock;
	}

	spin_lock_init(&urdm->lock);

	/* Create workqueue of UARTDM */
	urdm->uartdm_wq = create_workqueue(urdm->pfdata->workqueue_name);
	if (!urdm->uartdm_wq) {
		pr_err(PRT_NAME ": Error. Workqueue failed.\n");
		ret = -ENOMEM;
		goto err_create_workqueue;
	}

	/* Set UARTDM call back functions */
	INIT_WORK(&urdm->rcv_1byte_work, msm_uartdm_rcv_1byte_work);
	INIT_WORK(&urdm->rx_end_work, msm_uartdm_rx_end_work);
	INIT_WORK(&urdm->rx_error_work, msm_uartdm_rx_error_work);
	INIT_WORK(&urdm->tx_end_work, msm_uartdm_tx_end_work);
	INIT_WORK(&urdm->tx_error_work, msm_uartdm_tx_error_work);

	/* Call RX buffer init */
	ret = felica_rxbuf_init();
	if (ret) {
		pr_err(PRT_NAME ": Error. RX buffer init failed.\n");
		ret = -ENOMEM;
		goto err_rx_buf_init;
	}

	/* Call TX buffer init */
	ret = felica_txbuf_init();
	if (ret) {
		pr_err(PRT_NAME ": Error. TX buffer init failed.\n");
		ret = -ENOMEM;
		goto err_tx_buf_init;
	}

	/* Alloc dummy DMA distination */
	urdm->dummy_dst_buf = dma_alloc_coherent(NULL,
				sizeof(char) * UARTDM_BURST_SIZE,
				&urdm->dummy_dst_dmabase, GFP_KERNEL);
	if (!urdm->dummy_dst_buf) {
		pr_err(PRT_NAME ": Error. Alloc dummy distination.\n");
		ret = -ENOMEM;
		goto err_alloc_dummy_dst;
	}

	/* Alloc TX DMOV box */
	urdm->txbox = dma_alloc_coherent(NULL, sizeof(dmov_box),
					&urdm->mapped_txbox, GFP_KERNEL);
	if (!urdm->txbox) {
		pr_err(PRT_NAME ": Error. No enough mem for TXDM.\n");
		ret = -ENOMEM;
		goto err_alloc_txbox;
	}
	urdm->txbox_ptr = dma_alloc_coherent(NULL, sizeof(u32 *),
				&urdm->mapped_txbox_ptr, GFP_KERNEL);
	if (!urdm->txbox_ptr) {
		pr_err(PRT_NAME ": Error. No enough mem for TXDM.\n");
		ret = -ENOMEM;
		goto err_alloc_txbox_ptr;
	}

	/* Alloc RX DMOV box */
	urdm->rxbox = dma_alloc_coherent(NULL, sizeof(dmov_box),
					&urdm->mapped_rxbox, GFP_KERNEL);
	if (!urdm->rxbox) {
		pr_err(PRT_NAME ": Error. No enough mem for RXDM.\n");
		ret = -ENOMEM;
		goto err_alloc_rxbox;
	}
	urdm->rxbox_ptr = dma_alloc_coherent(NULL, sizeof(u32 *),
				&urdm->mapped_rxbox_ptr, GFP_KERNEL);
	if (!urdm->rxbox_ptr) {
		pr_err(PRT_NAME ": Error. No enough mem for RXDM.\n");
		ret = -ENOMEM;
		goto err_alloc_rxbox_ptr;
	}

	return 0;

err_alloc_rxbox_ptr:
	dma_free_coherent(NULL, sizeof(dmov_box), urdm->rxbox,
						urdm->mapped_rxbox);
err_alloc_rxbox:
	dma_free_coherent(NULL, sizeof(u32 *), urdm->txbox_ptr,
						urdm->mapped_txbox_ptr);
err_alloc_txbox_ptr:
	dma_free_coherent(NULL, sizeof(dmov_box), urdm->txbox,
						urdm->mapped_txbox);
err_alloc_txbox:
	dma_free_coherent(NULL, sizeof(char) * UARTDM_BURST_SIZE,
			urdm->dummy_dst_buf, urdm->dummy_dst_dmabase);
err_alloc_dummy_dst:
	felica_txbuf_exit();
err_tx_buf_init:
	felica_rxbuf_exit();
err_rx_buf_init:
	destroy_workqueue(urdm->uartdm_wq);
err_create_workqueue:
err_get_uartdm_clock:
	kfree(urdm);
	urdm = NULL;
err_alloc_uartdm_data:
	return ret;
}

/**
 * @brief   Unload MSM UARTDM data and release RX&TX buffers
 * @details This function executes;\n
 *            # Release DMA command boxes (RX & TX)\n
 *            # Release dummy DMA distination\n
 *            # Release TX buffer\n
 *            # Release RX buffer\n
 *            # Destroy workqueue of UARTDM\n
 *            # Release UARTDM sub-module data
 * @param   N/A
 * @retval  N/A
 * @note
 */
void msm_uartdm_exit(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	dma_free_coherent(NULL, sizeof(u32 *), urdm->rxbox_ptr,
						urdm->mapped_rxbox_ptr);
	dma_free_coherent(NULL, sizeof(dmov_box), urdm->rxbox,
						urdm->mapped_rxbox);
	dma_free_coherent(NULL, sizeof(u32 *), urdm->txbox_ptr,
						urdm->mapped_txbox_ptr);
	dma_free_coherent(NULL, sizeof(dmov_box), urdm->txbox,
						urdm->mapped_txbox);
	dma_free_coherent(NULL, sizeof(char) * UARTDM_BURST_SIZE,
			urdm->dummy_dst_buf, urdm->dummy_dst_dmabase);
	felica_txbuf_exit();
	felica_rxbuf_exit();
	destroy_workqueue(urdm->uartdm_wq);
	kfree(urdm);
	urdm = NULL;
}
