/*
 *  linux/drivers/mmc/host/msm_sdcc.c - Qualcomm MSM 7X00A SDCC Driver
 *
 *  Copyright (C) 2007 Google Inc,
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Based on mmci.c
 *
 * Author: San Mehat (san@android.com)
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/log2.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/clk.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/memory.h>

#include <asm/cacheflush.h>
#include <asm/div64.h>
#include <asm/sizes.h>

#include <asm/mach/mmc.h>
#include <mach/msm_iomap.h>
#include <mach/dma.h>
#include <mach/htc_pwrsink.h>


#include "msm_sdcc.h"

#define DRIVER_NAME "msm-sdcc"

#define DBG(host, fmt, args...)	\
	pr_debug("%s: %s: " fmt, mmc_hostname(host->mmc), __func__ , args)

#define IRQ_DEBUG 0

#if defined(CONFIG_DEBUG_FS)
static void msmsdcc_dbg_createhost(struct msmsdcc_host *);
static void msmsdcc_dbg_deletehost(void);
static struct dentry *debugfs_file = 0;
#endif

static unsigned int msmsdcc_fmin = 144000;
static unsigned int msmsdcc_fmax = 50000000;
static unsigned int msmsdcc_4bit = 1;
static unsigned int msmsdcc_pwrsave = 1;
static unsigned int msmsdcc_sdioirq = 0;
static unsigned int msmsdcc_piopoll = 1;

#define PIO_SPINMAX 30
#define CMD_SPINMAX 20

#define VERBOSE_COMMAND_TIMEOUTS	1

#if IRQ_DEBUG == 1
static char *irq_status_bits[] = { "cmdcrcfail", "datcrcfail", "cmdtimeout",
                                   "dattimeout", "txunderrun", "rxoverrun",
                                   "cmdrespend", "cmdsent", "dataend", NULL,
                                   "datablkend", "cmdactive", "txactive",
                                   "rxactive", "txhalfempty", "rxhalffull",
                                   "txfifofull", "rxfifofull", "txfifoempty",
                                   "rxfifoempty", "txdataavlbl", "rxdataavlbl",
                                   "sdiointr", "progdone", "atacmdcompl",
                                   "sdiointrope", "ccstimeout", NULL, NULL,
                                   NULL, NULL, NULL
                                 };

static void
msmsdcc_print_status(struct msmsdcc_host *host, char *hdr, uint32_t status)
{
	int i;

	printk(KERN_DEBUG "%s-%s ", mmc_hostname(host->mmc), hdr);
	for (i = 0; i < 32; i++) {
		if (status & (1 << i))
			printk("%s ", irq_status_bits[i]);
	}
	printk("\n");
}
#endif

static void
msmsdcc_start_command(struct msmsdcc_host *host, struct mmc_command *cmd,
                      u32 c);

static void
msmsdcc_request_end(struct msmsdcc_host *host, struct mmc_request *mrq)
{
	writel(0, host->base + MMCICOMMAND);

	BUG_ON(host->curr.data);

	host->curr.mrq = NULL;
	host->curr.cmd = NULL;

	if (mrq->data)
		mrq->data->bytes_xfered = host->curr.data_xfered;
	if (mrq->cmd->error == -ETIMEDOUT)
		mdelay(5);

	/*
	 * Need to drop the host lock here; mmc_request_done may call
	 * back into the driver...
	 */
	spin_unlock(&host->lock);
	mmc_request_done(host->mmc, mrq);
	spin_lock(&host->lock);
}

static void
msmsdcc_stop_data(struct msmsdcc_host *host)
{
	writel(0, host->base + MMCIDATACTRL);
	host->curr.data = NULL;
	host->curr.got_dataend = host->curr.got_datablkend = 0;
}

uint32_t msmsdcc_fifo_addr(struct msmsdcc_host *host)
{
	return host->memres->start + MMCIFIFO;
}

static void
msmsdcc_dma_complete_func(struct msm_dmov_cmd *cmd,
                          unsigned int result,
                          struct msm_dmov_errdata *err)
{
	struct msmsdcc_dma_data	*dma_data =
	    container_of(cmd, struct msmsdcc_dma_data, hdr);
	struct msmsdcc_host	*host = dma_data->host;
	unsigned long		flags;
	struct mmc_request	*mrq;

	spin_lock_irqsave(&host->lock, flags);
	mrq = host->curr.mrq;
	BUG_ON(!mrq);

	if (!(result & DMOV_RSLT_VALID)) {
		printk(KERN_ERR "msmsdcc: Invalid DataMover result\n");
		goto out;
	}

	if (result & DMOV_RSLT_DONE) {
		host->curr.data_xfered = host->curr.xfer_size;
	} else {
		/* Error or flush  */
		if (result & DMOV_RSLT_ERROR)
			printk(KERN_ERR "%s: DMA error (0x%.8x)\n",
			       mmc_hostname(host->mmc), result);
		if (result & DMOV_RSLT_FLUSH)
			printk(KERN_ERR "%s: DMA channel flushed (0x%.8x)\n",
			       mmc_hostname(host->mmc), result);
		if (err)
			printk(KERN_ERR
			       "Flush data: %.8x %.8x %.8x %.8x %.8x %.8x\n",
			       err->flush[0], err->flush[1], err->flush[2],
			       err->flush[3], err->flush[4], err->flush[5]);
		if (!mrq->data->error)
			mrq->data->error = -EIO;
	}
	host->dma.busy = 0;
	dma_unmap_sg(mmc_dev(host->mmc), host->dma.sg, host->dma.num_ents,
	             host->dma.dir);

	if (host->curr.user_pages) {
		struct scatterlist *sg = host->dma.sg;
		int i;

		for (i = 0; i < host->dma.num_ents; i++, sg++)
			flush_dcache_page(sg_page(sg));
	}

	host->dma.sg = NULL;

	if ((host->curr.got_dataend && host->curr.got_datablkend)
	        || mrq->data->error) {

		/*
		 * If we've already gotten our DATAEND / DATABLKEND
		 * for this request, then complete it through here.
		 */
		msmsdcc_stop_data(host);

		if (!mrq->data->error)
			host->curr.data_xfered = host->curr.xfer_size;
		if (!mrq->data->stop || mrq->cmd->error) {
			writel(0, host->base + MMCICOMMAND);
			host->curr.mrq = NULL;
			host->curr.cmd = NULL;
			mrq->data->bytes_xfered = host->curr.data_xfered;

			spin_unlock_irqrestore(&host->lock, flags);
			mmc_request_done(host->mmc, mrq);
			return;
		} else
			msmsdcc_start_command(host, mrq->data->stop, 0);
	}

out:
	spin_unlock_irqrestore(&host->lock, flags);
	return;
}

static int validate_dma(struct msmsdcc_host *host, struct mmc_data *data)
{
	if (host->dma.channel == -1)
		return -ENOENT;

	if ((data->blksz * data->blocks) < MCI_FIFOSIZE)
		return -EINVAL;
	if ((data->blksz * data->blocks) % MCI_FIFOSIZE)
		return -EINVAL;
	return 0;
}

static int msmsdcc_config_dma(struct msmsdcc_host *host, struct mmc_data *data)
{
	struct msmsdcc_nc_dmadata *nc;
	dmov_box *box;
	uint32_t rows;
	uint32_t crci;
	unsigned int n;
	int i, rc;
	struct scatterlist *sg = data->sg;

	rc = validate_dma(host, data);
	if (rc)
		return rc;

	host->dma.sg = data->sg;
	host->dma.num_ents = data->sg_len;

	nc = host->dma.nc;

	if (host->pdev_id == 1)
		crci = MSMSDCC_CRCI_SDC1;
	else if (host->pdev_id == 2)
		crci = MSMSDCC_CRCI_SDC2;
	else if (host->pdev_id == 3)
		crci = MSMSDCC_CRCI_SDC3;
	else if (host->pdev_id == 4)
		crci = MSMSDCC_CRCI_SDC4;
	else {
		host->dma.sg = NULL;
		host->dma.num_ents = 0;
		return -ENOENT;
	}

	if (data->flags & MMC_DATA_READ)
		host->dma.dir = DMA_FROM_DEVICE;
	else
		host->dma.dir = DMA_TO_DEVICE;

	/* host->curr.user_pages = (data->flags & MMC_DATA_USERPAGE); */
	host->curr.user_pages = 0;

	n = dma_map_sg(mmc_dev(host->mmc), host->dma.sg,
	               host->dma.num_ents, host->dma.dir);

	if (n != host->dma.num_ents) {
		printk(KERN_ERR "%s: Unable to map in all sg elements\n",
		       mmc_hostname(host->mmc));
		host->dma.sg = NULL;
		host->dma.num_ents = 0;
		return -ENOMEM;
	}

	box = &nc->cmd[0];
	for (i = 0; i < host->dma.num_ents; i++) {
		box->cmd = CMD_MODE_BOX;

		if (i == (host->dma.num_ents - 1))
			box->cmd |= CMD_LC;
		rows = (sg_dma_len(sg) % MCI_FIFOSIZE) ?
		       (sg_dma_len(sg) / MCI_FIFOSIZE) + 1 :
		       (sg_dma_len(sg) / MCI_FIFOSIZE) ;

		if (data->flags & MMC_DATA_READ) {
			box->src_row_addr = msmsdcc_fifo_addr(host);
			box->dst_row_addr = sg_dma_address(sg);

			box->src_dst_len = (MCI_FIFOSIZE << 16) |
			                   (MCI_FIFOSIZE);
			box->row_offset = MCI_FIFOSIZE;

			box->num_rows = rows * ((1 << 16) + 1);
			box->cmd |= CMD_SRC_CRCI(crci);
		} else {
			box->src_row_addr = sg_dma_address(sg);
			box->dst_row_addr = msmsdcc_fifo_addr(host);

			box->src_dst_len = (MCI_FIFOSIZE << 16) |
			                   (MCI_FIFOSIZE);
			box->row_offset = (MCI_FIFOSIZE << 16);

			box->num_rows = rows * ((1 << 16) + 1);
			box->cmd |= CMD_DST_CRCI(crci);
		}
		box++;
		sg++;
	}

	/* location of command block must be 64 bit aligned */
	BUG_ON(host->dma.cmd_busaddr & 0x07);

	nc->cmdptr = (host->dma.cmd_busaddr >> 3) | CMD_PTR_LP;
	host->dma.hdr.cmdptr = DMOV_CMD_PTR_LIST |
	                       DMOV_CMD_ADDR(host->dma.cmdptr_busaddr);
	host->dma.hdr.complete_func = msmsdcc_dma_complete_func;

	return 0;
}

static void
msmsdcc_start_data(struct msmsdcc_host *host, struct mmc_data *data)
{
	unsigned int datactrl, timeout;
	unsigned long long clks;
	void __iomem *base = host->base;
	unsigned int pio_irqmask = 0;

	host->curr.data = data;
	host->curr.xfer_size = data->blksz * data->blocks;
	host->curr.xfer_remain = host->curr.xfer_size;
	host->curr.data_xfered = 0;
	host->curr.got_dataend = 0;
	host->curr.got_datablkend = 0;

	memset(&host->pio, 0, sizeof(host->pio));

	clks = (unsigned long long)data->timeout_ns * host->clk_rate;
	do_div(clks, 1000000000UL);
	timeout = data->timeout_clks + (unsigned int)clks;
	writel(timeout, base + MMCIDATATIMER);

	writel(host->curr.xfer_size, base + MMCIDATALENGTH);

	datactrl = MCI_DPSM_ENABLE | (data->blksz << 4);

	if (!msmsdcc_config_dma(host, data))
		datactrl |= MCI_DPSM_DMAENABLE;
	else {
		host->pio.sg = data->sg;
		host->pio.sg_len = data->sg_len;
		host->pio.sg_off = 0;

		if (data->flags & MMC_DATA_READ) {
			pio_irqmask = MCI_RXFIFOHALFFULLMASK;
			if (host->curr.xfer_remain < MCI_FIFOSIZE)
				pio_irqmask |= MCI_RXDATAAVLBLMASK;
		} else
			pio_irqmask = MCI_TXFIFOHALFEMPTYMASK;
	}

	if (data->flags & MMC_DATA_READ)
		datactrl |= MCI_DPSM_DIRECTION;

	writel(pio_irqmask, base + MMCIMASK1);
	writel(datactrl, base + MMCIDATACTRL);

	if (datactrl & MCI_DPSM_DMAENABLE) {
		host->dma.busy = 1;
		msm_dmov_enqueue_cmd(host->dma.channel, &host->dma.hdr);
	}
}

static void
msmsdcc_start_command(struct msmsdcc_host *host, struct mmc_command *cmd, u32 c)
{
	void __iomem *base = host->base;

	DBG(host, "op %02x arg %08x flags %08x\n",
	    cmd->opcode, cmd->arg, cmd->flags);

	if (readl(base + MMCICOMMAND) & MCI_CPSM_ENABLE) {
		writel(0, base + MMCICOMMAND);
		udelay(2 + ((5 * 1000000) / host->clk_rate));
	}

	c |= cmd->opcode | MCI_CPSM_ENABLE;

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136)
			c |= MCI_CPSM_LONGRSP;
		c |= MCI_CPSM_RESPONSE;
	}

	if (/*interrupt*/0)
		c |= MCI_CPSM_INTERRUPT;

	if ((((cmd->opcode == 17) || (cmd->opcode == 18))  ||
	        ((cmd->opcode == 24) || (cmd->opcode == 25))) ||
	        (cmd->opcode == 53))
		c |= MCI_CSPM_DATCMD;

	if (cmd == cmd->mrq->stop)
		c |= MCI_CSPM_MCIABORT;

	host->curr.cmd = cmd;

	host->stats.cmds++;

	writel(cmd->arg, base + MMCIARGUMENT);
	writel(c, base + MMCICOMMAND);
}

static void
msmsdcc_data_err(struct msmsdcc_host *host, struct mmc_data *data,
                 unsigned int status)
{
	if (status & MCI_DATACRCFAIL) {
		printk(KERN_ERR "%s: Data CRC error\n",
		       mmc_hostname(host->mmc));
		printk(KERN_ERR "%s: opcode 0x%.8x\n", __func__,
		       data->mrq->cmd->opcode);
		printk(KERN_ERR "%s: blksz %d, blocks %d\n", __func__,
		       data->blksz, data->blocks);
		data->error = -EILSEQ;
	} else if (status & MCI_DATATIMEOUT) {
		printk(KERN_ERR "%s: Data timeout\n", mmc_hostname(host->mmc));
		data->error = -ETIMEDOUT;
	} else if (status & MCI_RXOVERRUN) {
		printk(KERN_ERR "%s: RX overrun\n", mmc_hostname(host->mmc));
		data->error = -EIO;
	} else if (status & MCI_TXUNDERRUN) {
		printk(KERN_ERR "%s: TX underrun\n", mmc_hostname(host->mmc));
		data->error = -EIO;
	} else {
		printk(KERN_ERR "%s: Unknown error (0x%.8x)\n",
		       mmc_hostname(host->mmc), status);
		data->error = -EIO;
	}
}


static int
msmsdcc_pio_read(struct msmsdcc_host *host, char *buffer, unsigned int remain)
{
	void __iomem	*base = host->base;
	uint32_t	*ptr = (uint32_t *) buffer;
	int		count = 0;

	while (readl(base + MMCISTATUS) & MCI_RXDATAAVLBL) {

		*ptr = readl(base + MMCIFIFO + (count % MCI_FIFOSIZE));
		ptr++;
		count += sizeof(uint32_t);

		remain -=  sizeof(uint32_t);
		if (remain == 0)
			break;
	}
	return count;
}

static int
msmsdcc_pio_write(struct msmsdcc_host *host, char *buffer,
                  unsigned int remain, u32 status)
{
	void __iomem *base = host->base;
	char *ptr = buffer;

	do {
		unsigned int count, maxcnt;

		maxcnt = status & MCI_TXFIFOEMPTY ? MCI_FIFOSIZE :
		         MCI_FIFOHALFSIZE;
		count = min(remain, maxcnt);

		writesl(base + MMCIFIFO, ptr, count >> 2);
		ptr += count;
		remain -= count;

		if (remain == 0)
			break;

		status = readl(base + MMCISTATUS);
	} while (status & MCI_TXFIFOHALFEMPTY);

	return ptr - buffer;
}

static int
msmsdcc_spin_on_status(struct msmsdcc_host *host, uint32_t mask, int maxspin)
{
	while (maxspin) {
		if ((readl(host->base + MMCISTATUS) & mask))
			return 0;
		udelay(1);
		--maxspin;
	}
	return -ETIMEDOUT;
}

static irqreturn_t
msmsdcc_pio_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;
	void __iomem		*base = host->base;
	uint32_t		status;

	status = readl(base + MMCISTATUS);
#if IRQ_DEBUG
	msmsdcc_print_status(host, "irq1-r", status);
#endif

	/* SEMC_BEGIN (Crash on irq when data already handled - DMS00718508) */
//	do {
	while (host->pio.sg) {
		/* SEMC_END (Crash on irq when data already handled - DMS00718508) */

		unsigned long flags;
		unsigned int remain, len;
		char *buffer;

		if (!(status & (MCI_TXFIFOHALFEMPTY | MCI_RXDATAAVLBL))) {
			if (host->curr.xfer_remain == 0 || !msmsdcc_piopoll)
				break;

			if (msmsdcc_spin_on_status(host,
			                           (MCI_TXFIFOHALFEMPTY |
			                            MCI_RXDATAAVLBL),
			                           PIO_SPINMAX)) {
				break;
			}
		}

		/* Map the current scatter buffer */
		local_irq_save(flags);
		buffer = kmap_atomic(sg_page(host->pio.sg),
		                     KM_BIO_SRC_IRQ) + host->pio.sg->offset;
		buffer += host->pio.sg_off;
		remain = host->pio.sg->length - host->pio.sg_off;
		len = 0;
		if (status & MCI_RXACTIVE)
			len = msmsdcc_pio_read(host, buffer, remain);
		if (status & MCI_TXACTIVE)
			len = msmsdcc_pio_write(host, buffer, remain, status);

		/* Unmap the buffer */
		kunmap_atomic(buffer, KM_BIO_SRC_IRQ);
		local_irq_restore(flags);

		host->pio.sg_off += len;
		host->curr.xfer_remain -= len;
		host->curr.data_xfered += len;
		remain -= len;

		if (remain == 0) {
			/* This sg page is full - do some housekeeping */
			if (status & MCI_RXACTIVE && host->curr.user_pages)
				flush_dcache_page(sg_page(host->pio.sg));

			if (!--host->pio.sg_len) {
				memset(&host->pio, 0, sizeof(host->pio));
				break;
			}

			/* Advance to next sg */
			host->pio.sg++;
			host->pio.sg_off = 0;
		}

		status = readl(base + MMCISTATUS);
		/* SEMC_BEGIN (Crash on irq when data already handled - DMS00718508) */
//	} while (1);
	}
	/* SEMC_END (Crash on irq when data already handled - DMS00718508) */

	if (status & MCI_RXACTIVE && host->curr.xfer_remain < MCI_FIFOSIZE)
		writel(MCI_RXDATAAVLBLMASK, base + MMCIMASK1);

	if (!host->curr.xfer_remain)
		writel(0, base + MMCIMASK1);

	return IRQ_HANDLED;
}

static void msmsdcc_do_cmdirq(struct msmsdcc_host *host, uint32_t status)
{
	struct mmc_command *cmd = host->curr.cmd;
	void __iomem	   *base = host->base;

	host->curr.cmd = NULL;
	cmd->resp[0] = readl(base + MMCIRESPONSE0);
	cmd->resp[1] = readl(base + MMCIRESPONSE1);
	cmd->resp[2] = readl(base + MMCIRESPONSE2);
	cmd->resp[3] = readl(base + MMCIRESPONSE3);

	del_timer(&host->command_timer);
	if (status & MCI_CMDTIMEOUT) {
#if VERBOSE_COMMAND_TIMEOUTS
		printk(KERN_ERR "%s: Command timeout\n",
		       mmc_hostname(host->mmc));
#endif
		cmd->error = -ETIMEDOUT;
	} else if (status & MCI_CMDCRCFAIL &&
	           cmd->flags & MMC_RSP_CRC) {
		printk(KERN_ERR "%s: Command CRC error\n",
		       mmc_hostname(host->mmc));
		cmd->error = -EILSEQ;
	}

	if (!cmd->data || cmd->error) {
		if (host->curr.data && host->dma.sg)
			msm_dmov_stop_cmd(host->dma.channel,
			                  &host->dma.hdr, 0);
		else if (host->curr.data) { /* Non DMA */
			msmsdcc_stop_data(host);
			msmsdcc_request_end(host, cmd->mrq);
		} else /* host->data == NULL */
			msmsdcc_request_end(host, cmd->mrq);
	} else if (!(cmd->data->flags & MMC_DATA_READ))
		msmsdcc_start_data(host, cmd->data);
}

static irqreturn_t
msmsdcc_irq(int irq, void *dev_id)
{
	struct msmsdcc_host	*host = dev_id;
	void __iomem		*base = host->base;
	u32			status;
	int			ret = 0;
	int			cardint = 0;

	spin_lock(&host->lock);

	do {
		struct mmc_data *data;
		status = readl(base + MMCISTATUS);

#if IRQ_DEBUG
		msmsdcc_print_status(host, "irq0-r", status);
#endif

		status &= (readl(base + MMCIMASK0) |
		           MCI_DATABLOCKENDMASK);
		writel(status, base + MMCICLEAR);
#if IRQ_DEBUG
		msmsdcc_print_status(host, "irq0-p", status);
#endif

		data = host->curr.data;
		if (data) {
			/* Check for data errors */
			if (status & (MCI_DATACRCFAIL|MCI_DATATIMEOUT|
			              MCI_TXUNDERRUN|MCI_RXOVERRUN)) {
				msmsdcc_data_err(host, data, status);
				host->curr.data_xfered = 0;
				if (host->dma.sg)
					msm_dmov_stop_cmd(host->dma.channel,
					                  &host->dma.hdr, 0);
				else {
					msmsdcc_stop_data(host);
					if (!data->stop)
						msmsdcc_request_end(host,
						                    data->mrq);
					else
						msmsdcc_start_command(host,
						                      data->stop,
						                      0);
				}
			}

			/* Check for data done */
			if (!host->curr.got_dataend && (status & MCI_DATAEND))
				host->curr.got_dataend = 1;

			if (!host->curr.got_datablkend &&
			        (status & MCI_DATABLOCKEND)) {
				host->curr.got_datablkend = 1;
			}

			if (host->curr.got_dataend &&
			        host->curr.got_datablkend) {
				/*
				 * If DMA is still in progress, we complete
				 * via the completion handler
				 */
				if (!host->dma.busy) {
					/*
					 * There appears to be an issue in the
					 * controller where if you request a
					 * small block transfer (< fifo size),
					 * you may get your DATAEND/DATABLKEND
					 * irq without the PIO data irq.
					 *
					 * Check to see if theres still data
					 * to be read, and simulate a PIO irq.
					 */
					if (readl(base + MMCISTATUS) &
					        MCI_RXDATAAVLBL)
						msmsdcc_pio_irq(1, host);

					msmsdcc_stop_data(host);
					if (!data->error)
						host->curr.data_xfered =
						    host->curr.xfer_size;

					if (!data->stop)
						msmsdcc_request_end(host,
						                    data->mrq);
					else
						msmsdcc_start_command(host,
						                      data->stop, 0);
				}
			}
		}

		if (status & (MCI_CMDSENT | MCI_CMDRESPEND | MCI_CMDCRCFAIL |
		              MCI_CMDTIMEOUT) && host->curr.cmd) {
			msmsdcc_do_cmdirq(host, status);
		}

		if (status & MCI_SDIOINTOPER) {
			cardint = 1;
			status &= ~MCI_SDIOINTOPER;
		}
		ret = 1;
	} while (status);

	spin_unlock(&host->lock);

	/*
	 * We have to delay handling the card interrupt as it calls
	 * back into the driver.
	 */
	if (cardint) {
		mmc_signal_sdio_irq(host->mmc);
	}

	return IRQ_RETVAL(ret);
}

static void
msmsdcc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long		flags;

	WARN_ON(host->curr.mrq != NULL);

	WARN_ON(host->pwr == 0);

	spin_lock_irqsave(&host->lock, flags);

	host->stats.reqs++;

	if (host->eject) {
		if (mrq->data && !(mrq->data->flags & MMC_DATA_READ)) {
			mrq->cmd->error = 0;
			mrq->data->bytes_xfered = mrq->data->blksz *
			                          mrq->data->blocks;
		} else
			mrq->cmd->error = -ENOMEDIUM;

		spin_unlock_irqrestore(&host->lock, flags);
		mmc_request_done(mmc, mrq);
		return;
	}

	host->curr.mrq = mrq;

	if (mrq->data && mrq->data->flags & MMC_DATA_READ)
		msmsdcc_start_data(host, mrq->data);

	msmsdcc_start_command(host, mrq->cmd, 0);

	if (host->cmdpoll && !msmsdcc_spin_on_status(host,
	        MCI_CMDRESPEND|MCI_CMDCRCFAIL|MCI_CMDTIMEOUT,
	        CMD_SPINMAX)) {
		uint32_t status = readl(host->base + MMCISTATUS);
		msmsdcc_do_cmdirq(host, status);
		writel(MCI_CMDRESPEND | MCI_CMDCRCFAIL | MCI_CMDTIMEOUT,
		       host->base + MMCICLEAR);
		host->stats.cmdpoll_hits++;
	} else {
		host->stats.cmdpoll_misses++;
		mod_timer(&host->command_timer, jiffies + HZ);
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

static void
msmsdcc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	u32 clk = 0, pwr = 0;
	int rc;

	if (ios->clock) {

		if (!host->clks_on) {
			clk_enable(host->pclk);
			clk_enable(host->clk);
			host->clks_on = 1;
		}
		if (ios->clock != host->clk_rate) {
			rc = clk_set_rate(host->clk, ios->clock);
			if (rc < 0)
				printk(KERN_ERR
				       "Error setting clock rate (%d)\n", rc);
			else
				host->clk_rate = ios->clock;
		}
		clk |= MCI_CLK_ENABLE;
	}

	if (ios->bus_width == MMC_BUS_WIDTH_4)
		clk |= (2 << 10); /* Set WIDEBUS */

	if (ios->clock > 400000 && msmsdcc_pwrsave)
		clk |= (1 << 9); /* PWRSAVE */

	clk |= (1 << 12); /* FLOW_ENA */
	clk |= (1 << 15); /* feedback clock */

	if (host->plat->translate_vdd)
		pwr |= host->plat->translate_vdd(mmc_dev(mmc), ios->vdd);

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		htc_pwrsink_set(PWRSINK_SDCARD, 0);
		break;
	case MMC_POWER_UP:
		pwr |= MCI_PWR_UP;
		break;
	case MMC_POWER_ON:
		htc_pwrsink_set(PWRSINK_SDCARD, 100);
		pwr |= MCI_PWR_ON;
		break;
	}

	if (ios->bus_mode == MMC_BUSMODE_OPENDRAIN)
		pwr |= MCI_OD;

	writel(clk, host->base + MMCICLOCK);

	if (host->pwr != pwr) {
		host->pwr = pwr;
		writel(pwr, host->base + MMCIPOWER);
	}

	if (!(clk & MCI_CLK_ENABLE) && host->clks_on) {
		clk_disable(host->clk);
		clk_disable(host->pclk);
		host->clks_on = 0;
	}
}

static void msmsdcc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msmsdcc_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 status;

	/* printk("%s: %d\n", __FUNCTION__, enable); */
	spin_lock_irqsave(&host->lock, flags);
	if (msmsdcc_sdioirq == 1) {
		status = readl(host->base + MMCIMASK0);
		if (enable)
			status |= MCI_SDIOINTOPERMASK;
		else
			status &= ~MCI_SDIOINTOPERMASK;
		host->saved_irq0mask = status;
		writel(status, host->base + MMCIMASK0);
	}
	spin_unlock_irqrestore(&host->lock, flags);
}

static const struct mmc_host_ops msmsdcc_ops = {
	.request	= msmsdcc_request,
	.set_ios	= msmsdcc_set_ios,
	.enable_sdio_irq = msmsdcc_enable_sdio_irq,
};

static void
msmsdcc_check_status(unsigned long data)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *)data;
	unsigned int status;

	if (!host->plat->status) {
		mmc_detect_change(host->mmc, 0);
		goto out;
	}

	status = host->plat->status(mmc_dev(host->mmc));
	host->eject = !status;
	if (status ^ host->oldstat) {
		printk(KERN_INFO
		       "%s: Slot status change detected (%d -> %d)\n",
		       mmc_hostname(host->mmc), host->oldstat, status);
		if (status)
			mmc_detect_change(host->mmc, (5 * HZ) / 2);
		else
			mmc_detect_change(host->mmc, 0);
	}

	host->oldstat = status;

out:
	if (host->timer.function)
		mod_timer(&host->timer, jiffies + HZ);
}

static void
msmsdcc_status_notify_cb(int card_present, void *dev_id)
{
	struct msmsdcc_host *host = dev_id;

	printk(KERN_DEBUG "%s: card_present %d\n", mmc_hostname(host->mmc),
	       card_present);
	msmsdcc_check_status((unsigned long) host);
}

/*
 * called when a command expires.
 * Dump some debugging, and then error
 * out the transaction.
 */
static void
msmsdcc_command_expired(unsigned long _data)
{
	struct msmsdcc_host	*host = (struct msmsdcc_host *) _data;
	struct mmc_request	*mrq;
	unsigned long		flags;

	spin_lock_irqsave(&host->lock, flags);
	mrq = host->curr.mrq;

	if (!mrq) {
		printk(KERN_INFO "%s: Command expiry misfire\n",
		       mmc_hostname(host->mmc));
		spin_unlock_irqrestore(&host->lock, flags);
		return;
	}

	printk(KERN_ERR "%s: Command timeout (%p %p %p %p)\n",
	       mmc_hostname(host->mmc), mrq, mrq->cmd,
	       mrq->data, host->dma.sg);

	mrq->cmd->error = -ETIMEDOUT;
	msmsdcc_stop_data(host);

	writel(0, host->base + MMCICOMMAND);

	host->curr.mrq = NULL;
	host->curr.cmd = NULL;

	spin_unlock_irqrestore(&host->lock, flags);
	mmc_request_done(host->mmc, mrq);
}

static int
msmsdcc_init_dma(struct msmsdcc_host *host)
{
	memset(&host->dma, 0, sizeof(struct msmsdcc_dma_data));
	host->dma.host = host;
	host->dma.channel = -1;

	if (!host->dmares)
		return -ENODEV;

	host->dma.nc = dma_alloc_coherent(NULL,
	                                  sizeof(struct msmsdcc_nc_dmadata),
	                                  &host->dma.nc_busaddr,
	                                  GFP_KERNEL);
	if (host->dma.nc == NULL) {
		printk(KERN_ERR "Unable to allocate DMA buffer\n");
		return -ENOMEM;
	}
	memset(host->dma.nc, 0x00, sizeof(struct msmsdcc_nc_dmadata));
	host->dma.cmd_busaddr = host->dma.nc_busaddr;
	host->dma.cmdptr_busaddr = host->dma.nc_busaddr +
	                           offsetof(struct msmsdcc_nc_dmadata, cmdptr);
	host->dma.channel = host->dmares->start;

	return 0;
}

#ifdef CONFIG_MMC_MSM7X00A_RESUME_IN_WQ
static void
do_resume_work(struct work_struct *work)
{
	struct msmsdcc_host *host =
	    container_of(work, struct msmsdcc_host, resume_task);
	struct mmc_host	*mmc = host->mmc;

	if (mmc) {
		mmc_resume_host(mmc);
		if (host->stat_irq)
			enable_irq(host->stat_irq);
	}
}
#endif

int msmsdcc_probe(struct platform_device *pdev)
{
	struct mmc_platform_data *plat = pdev->dev.platform_data;
	struct msmsdcc_host *host;
	struct mmc_host *mmc;
	struct resource *irqres = NULL;
	struct resource *memres = NULL;
	struct resource *dmares = NULL;
	int ret;

	/* must have platform data */
	if (!plat) {
		printk(KERN_ERR "%s: Platform data not available\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (pdev->id < 1 || pdev->id > 4)
		return -EINVAL;

	if (pdev->resource == NULL || pdev->num_resources < 2) {
		printk(KERN_ERR "%s: Invalid resource\n", __func__);
		return -ENXIO;
	}

	memres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dmares = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	irqres = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	if (!irqres || !memres) {
		printk(KERN_ERR "%s: Invalid resource\n", __func__);
		return -ENXIO;
	}

	/*
	 * Setup our host structure
	 */

	mmc = mmc_alloc_host(sizeof(struct msmsdcc_host), &pdev->dev);
	if (!mmc) {
		ret = -ENOMEM;
		goto out;
	}

	host = mmc_priv(mmc);
	host->pdev_id = pdev->id;
	host->plat = plat;
	host->mmc = mmc;

	host->cmdpoll = 1;

	host->base = ioremap(memres->start, PAGE_SIZE);
	if (!host->base) {
		ret = -ENOMEM;
		goto out;
	}

	host->irqres = irqres;
	host->memres = memres;
	host->dmares = dmares;
	spin_lock_init(&host->lock);

#ifdef CONFIG_MMC_EMBEDDED_SDIO
	if (plat->embedded_sdio)
		mmc_set_embedded_sdio_data(mmc,
		                           &plat->embedded_sdio->cis,
		                           &plat->embedded_sdio->cccr,
		                           plat->embedded_sdio->funcs,
		                           plat->embedded_sdio->num_funcs);
#endif

#ifdef CONFIG_MMC_MSM7X00A_RESUME_IN_WQ
	INIT_WORK(&host->resume_task, do_resume_work);
#endif

	/*
	 * Setup DMA
	 */
	msmsdcc_init_dma(host);

	/*
	 * Setup main peripheral bus clock
	 */
	host->pclk = clk_get(&pdev->dev, "sdc_pclk");
	if (IS_ERR(host->pclk)) {
		ret = PTR_ERR(host->pclk);
		printk(KERN_ERR "%s: failed to get pclock (%d)\n", __func__, ret);
		goto host_free;
	}

	ret = clk_enable(host->pclk);
	if (ret)
		goto pclk_put;

	host->pclk_rate = clk_get_rate(host->pclk);

	/*
	 * Setup SDC MMC clock
	 */
	host->clk = clk_get(&pdev->dev, "sdc_clk");
	if (IS_ERR(host->clk)) {
		ret = PTR_ERR(host->clk);
		printk(KERN_ERR "%s: failed to get clock (%d)\n", __func__, ret);
		goto pclk_disable;
	}

	ret = clk_enable(host->clk);
	if (ret)
		goto clk_put;

	ret = clk_set_rate(host->clk, msmsdcc_fmin);
	if (ret) {
		printk(KERN_ERR "%s: Clock rate set failed (%d)\n",
		       __func__, ret);
		goto clk_disable;
	}

	host->clk_rate = clk_get_rate(host->clk);

	host->clks_on = 1;

	/*
	 * Setup MMC host structure
	 */
	mmc->ops = &msmsdcc_ops;
	mmc->f_min = msmsdcc_fmin;
	mmc->f_max = msmsdcc_fmax;
	mmc->ocr_avail = plat->ocr_mask;

	if (msmsdcc_4bit)
		mmc->caps |= MMC_CAP_4_BIT_DATA;
	if (msmsdcc_sdioirq)
		mmc->caps |= MMC_CAP_SDIO_IRQ;
	mmc->caps |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;

	mmc->max_phys_segs = NR_SG;
	mmc->max_hw_segs = NR_SG;
	mmc->max_blk_size = 4096;	/* MCI_DATA_CTL BLOCKSIZE up to 4096 */
	mmc->max_blk_count = 65536;

	mmc->max_req_size = 33554432;	/* MCI_DATA_LENGTH is 25 bits */
	mmc->max_seg_size = mmc->max_req_size;

	writel(0, host->base + MMCIMASK0);
	writel(0x5e007ff, host->base + MMCICLEAR); /* Add: 1 << 25 */

	writel(MCI_IRQENABLE, host->base + MMCIMASK0);
	host->saved_irq0mask = MCI_IRQENABLE;

	/*
	 * Setup card detect change
	 */

	memset(&host->timer, 0, sizeof(host->timer));

	if (plat->register_status_notify) {
		plat->register_status_notify(msmsdcc_status_notify_cb, host);
	} else if (!plat->status)
		printk(KERN_ERR "%s: No card detect facilities available\n",
		       mmc_hostname(mmc));
	else {
		init_timer(&host->timer);
		host->timer.data = (unsigned long)host;
		host->timer.function = msmsdcc_check_status;
		host->timer.expires = jiffies + HZ;
		add_timer(&host->timer);
	}

	if (plat->status) {
		host->oldstat = host->plat->status(mmc_dev(host->mmc));
		host->eject = !host->oldstat;
	}

	/*
	 * Setup a command timer. We currently need this due to
	 * some 'strange' timeout / error handling situations.
	 */
	init_timer(&host->command_timer);
	host->command_timer.data = (unsigned long) host;
	host->command_timer.function = msmsdcc_command_expired;

	ret = request_irq(irqres->start, msmsdcc_irq, IRQF_SHARED,
	                  DRIVER_NAME " (cmd)", host);
	if (ret)
		goto stat_irq_free;

	ret = request_irq(irqres->end, msmsdcc_pio_irq, IRQF_SHARED,
	                  DRIVER_NAME " (pio)", host);
	if (ret)
		goto cmd_irq_free;

	mmc_set_drvdata(pdev, mmc);
	mmc_claim_host(mmc);

	printk(KERN_INFO
	       "%s: Qualcomm MSM SDCC at 0x%016llx irq %d,%d dma %d\n",
	       mmc_hostname(mmc), (unsigned long long)memres->start,
	       (unsigned int) irqres->start,
	       (unsigned int) host->stat_irq, host->dma.channel);
	printk(KERN_INFO "%s: 4 bit data mode %s\n", mmc_hostname(mmc),
	       (mmc->caps & MMC_CAP_4_BIT_DATA ? "enabled" : "disabled"));
	printk(KERN_INFO "%s: MMC clock %u -> %u Hz, PCLK %u Hz\n",
	       mmc_hostname(mmc), msmsdcc_fmin, msmsdcc_fmax, host->pclk_rate);
	printk(KERN_INFO "%s: Slot eject status = %d\n", mmc_hostname(mmc),
	       host->eject);
	printk(KERN_INFO "%s: Power save feature enable = %d\n",
	       mmc_hostname(mmc), msmsdcc_pwrsave);

	if (host->dma.channel != -1) {
		printk(KERN_INFO
		       "%s: DM non-cached buffer at %p, dma_addr 0x%.8x\n",
		       mmc_hostname(mmc), host->dma.nc, host->dma.nc_busaddr);
		printk(KERN_INFO
		       "%s: DM cmd busaddr 0x%.8x, cmdptr busaddr 0x%.8x\n",
		       mmc_hostname(mmc), host->dma.cmd_busaddr,
		       host->dma.cmdptr_busaddr);
	} else
		printk(KERN_INFO
		       "%s: PIO transfer enabled\n", mmc_hostname(mmc));
	if (host->timer.function)
		printk(KERN_INFO "%s: Polling status mode enabled\n",
		       mmc_hostname(mmc));

#if defined(CONFIG_DEBUG_FS)
	msmsdcc_dbg_createhost(host);
#endif
	return 0;
cmd_irq_free:
	free_irq(irqres->start, host);
stat_irq_free:
	if (host->stat_irq)
		free_irq(host->stat_irq, host);
clk_disable:
	clk_disable(host->clk);
clk_put:
	clk_put(host->clk);
pclk_disable:
	clk_disable(host->pclk);
pclk_put:
	clk_put(host->pclk);
host_free:
	mmc_free_host(mmc);
out:
	return ret;
}

int msmsdcc_remove(struct platform_device *dev)
{
	struct mmc_host *mmc = mmc_get_drvdata(dev);
	struct msmsdcc_host *host = mmc_priv(mmc);

#if defined(CONFIG_DEBUG_FS)
	msmsdcc_dbg_deletehost();
#endif
	mmc_release_host(mmc);
	free_irq(host->irqres->start, host);
	free_irq(host->irqres->end, host);
	if (host->stat_irq)
		free_irq(host->stat_irq, host);

	spin_lock(&host->lock);

	del_timer(&host->timer);
	del_timer(&host->command_timer);

	clk_disable(host->clk);
	clk_put(host->clk);
	clk_disable(host->pclk);
	clk_put(host->pclk);

	dma_free_coherent(
	    NULL,
	    sizeof(struct msmsdcc_nc_dmadata),
	    host->dma.nc,
	    host->dma.nc_busaddr);

	spin_unlock(&host->lock);

	mmc_free_host(mmc);

	return 0;
}

int msmsdcc_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = mmc_get_drvdata(dev);
	int rc = 0;

	if (mmc) {
		struct msmsdcc_host *host = mmc_priv(mmc);

		if (host->stat_irq)
			disable_irq(host->stat_irq);

		if (mmc->card && mmc->card->type != MMC_TYPE_SDIO)
			rc = mmc_suspend_host(mmc, state);
		if (!rc) {
			writel(0, host->base + MMCIMASK0);

			if (host->clks_on) {
				clk_disable(host->clk);
				clk_disable(host->pclk);
				host->clks_on = 0;
			}
		}
	}
	return rc;
}

int msmsdcc_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = mmc_get_drvdata(dev);
	unsigned long flags;

	if (mmc) {
		struct msmsdcc_host *host = mmc_priv(mmc);

		spin_lock_irqsave(&host->lock, flags);

		if (!host->clks_on) {
			clk_enable(host->pclk);
			clk_enable(host->clk);
			host->clks_on = 1;
		}

		writel(host->saved_irq0mask, host->base + MMCIMASK0);

		spin_unlock_irqrestore(&host->lock, flags);

		if (mmc->card && mmc->card->type != MMC_TYPE_SDIO)
#ifdef CONFIG_MMC_MSM7X00A_RESUME_IN_WQ
			schedule_work(&host->resume_task);
#else
			mmc_resume_host(mmc);
		if (host->stat_irq)
			enable_irq(host->stat_irq);
#endif
		else if (host->stat_irq)
			enable_irq(host->stat_irq);
	}
	return 0;
}

MODULE_DESCRIPTION("Qualcomm MSM 7X00A Multimedia Card Interface driver");
MODULE_LICENSE("GPL");
#if defined(CONFIG_DEBUG_FS)

static int
msmsdcc_dbg_state_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t
msmsdcc_dbg_state_read(struct file *file, char __user *ubuf,
                       size_t count, loff_t *ppos)
{
	struct msmsdcc_host *host = (struct msmsdcc_host *) file->private_data;
	char buf[1024];
	int max, i;

	i = 0;
	max = sizeof(buf) - 1;

	if (host->curr.cmd) {
		struct mmc_command *cmd = host->curr.cmd;

		i += scnprintf(buf + i, max - i,
		               "CurrentCmd : opcode %d, arg 0x%.8x, flags 0x%.8x\n",
		               cmd->opcode, cmd->arg, cmd->flags);
	}

	if (host->curr.data) {
		struct mmc_data *data = host->curr.data;
		i += scnprintf(buf + i, max - i,
		               "DAT0: %.8x %.8x %.8x %.8x %.8x %.8x\n",
		               data->timeout_ns, data->timeout_clks,
		               data->blksz, data->blocks, data->error,
		               data->flags);
		i += scnprintf(buf + i, max - i, "DAT1: %.8x %.8x %.8x %p\n",
		               host->curr.xfer_size, host->curr.xfer_remain,
		               host->curr.data_xfered, host->dma.sg);
	}

	i += scnprintf(buf + i, max - i, "Requests : %u\n", host->stats.reqs);
	i += scnprintf(buf + i, max - i, "Commands : %u\n", host->stats.cmds);
	i += scnprintf(buf + i, max - i, "CmdPoll  : %d\n", host->cmdpoll);
	i += scnprintf(buf + i, max - i, "PollHit  : %u\n", host->stats.cmdpoll_hits);
	i += scnprintf(buf + i, max - i, "PollMiss : %u\n", host->stats.cmdpoll_misses);

	return simple_read_from_buffer(ubuf, count, ppos, buf, i);
}

static const struct file_operations msmsdcc_dbg_state_ops = {
	.read	= msmsdcc_dbg_state_read,
	.open	= msmsdcc_dbg_state_open,
};

static void msmsdcc_dbg_createhost(struct msmsdcc_host *host)
{
	debugfs_file = debugfs_create_file(mmc_hostname(host->mmc), 0644, NULL,
	                                   host, &msmsdcc_dbg_state_ops);
}

static void msmsdcc_dbg_deletehost()
{
	if (debugfs_file)
		debugfs_remove(debugfs_file);
}
#endif
