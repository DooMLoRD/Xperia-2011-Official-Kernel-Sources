/* vendor/semc/hardware/felica/felica_rxbuf.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/uaccess.h>
#include "felica_rxbuf.h"

#define PRT_NAME "rxbuf"

/* DEBUG_RXBUF_DUMP : 0 = without dump, 1 = with dump */
#define DEBUG_RXBUF_DUMP 0
#if DEBUG_RXBUF_DUMP
#include <linux/string.h>
#endif

struct rxbuf_slot {
	int	head;
	int	tail;
	char	*buf;
	dma_addr_t dmabase;
};

struct rxbuf_all {
	int wp;
	int rp;
	struct rxbuf_slot slot[RXBUF_N];
	int count;
	enum {no, yes} fulfill;
	struct dma_pool *dmapool;
};

static struct rxbuf_all rxbuf;

#if DEBUG_RXBUF_DUMP
static char dump[RXBUF_M * RXBUF_N];
#endif

/**
 * @brief  Clear RX buffer
 * @param  N/A
 * @retval N/A
 * @note
 */
void felica_rxbuf_clear(void)
{
	int i;

	pr_debug(PRT_NAME ": %s\n", __func__);

	rxbuf.wp = 0;
	rxbuf.rp = 0;
	for (i = 0; i < RXBUF_N; i++) {
		rxbuf.slot[i].head = 0;
		rxbuf.slot[i].tail = 0;
	}
	rxbuf.count = 0;
	rxbuf.fulfill = no;
}

/**
 * @brief  Count the number of the stored data in RX buffer
 * @param  N/A
 * @retval 0/Positive : Success. the number of the stored data
 * @retval Negative   : Failure\n
 *           -ENODEV = RX buffer not initialized
 * @note
 */
int felica_rxbuf_count(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (!rxbuf.dmapool) {
		pr_err(PRT_NAME ": Error. RX buf not initialized.\n");
		return -ENODEV;
	}

	return rxbuf.count;
}

/**
 * @brief  Copy the stored data to User space buffer
 * @param  buf : Pointer to User space buffer
 * @param  len : Date length to copy
 * @retval Positive : Number of the data copied successfully.
 * @retval 0 : No data to copy
 * @retval Negative : Failure.\n
 *           -EINVAL = Invalid arguments\n
 *           -ENODEV = RX buffer not initialized\n
 *           -EFAULT = Data copy error
 */
int felica_rxbuf_gets(char __user *buf, int len)
{
	int ret;
	int cnt = 0;
	int rest, srest;
	int cpnum;

#if DEBUG_RXBUF_DUMP
	int i;
#endif

	pr_debug(PRT_NAME ": %s\n", __func__);

	if ((0 >= len) || (NULL == buf)) {
		pr_err(PRT_NAME ": Error. Invalid argument.\n");
		return -EINVAL;
	}
	if (!rxbuf.dmapool) {
		pr_err(PRT_NAME ": Error. RX buf not initialized.\n");
		return -ENODEV;
	}

	if (0 == rxbuf.count)
		return 0;

	while (1) {
		rest = len - cnt;
		if (0 == rest)
			break;
		srest = rxbuf.slot[rxbuf.rp].head - rxbuf.slot[rxbuf.rp].tail;
		cpnum = (rest < srest) ? rest : srest ;
		ret = copy_to_user(buf + cnt, rxbuf.slot[rxbuf.rp].buf
					+ rxbuf.slot[rxbuf.rp].tail, cpnum);
		if (ret) {
			pr_err(PRT_NAME ": Error. copy_to_user failure.\n");
			return -EFAULT;
		}

#if DEBUG_RXBUF_DUMP
		memcpy(dump + cnt, rxbuf.slot[rxbuf.rp].buf
					+ rxbuf.slot[rxbuf.rp].tail, cpnum);
#endif

		rxbuf.slot[rxbuf.rp].tail += cpnum;
		cnt += cpnum;
		if (rxbuf.slot[rxbuf.rp].tail == rxbuf.slot[rxbuf.rp].head) {
			rxbuf.slot[rxbuf.rp].tail = 0;
			rxbuf.slot[rxbuf.rp].head = 0;
			rxbuf.rp++;
			rxbuf.rp %= RXBUF_N;
			rxbuf.fulfill = no;
		}
		if (rxbuf.rp == rxbuf.wp)
			break;
	}
	rxbuf.count -= cnt;

#if DEBUG_RXBUF_DUMP
	printk("RXgets dump: ");
	for (i = 0; i < cnt; i++)
		printk("%02x ", dump[i]);
	printk("\n");
#endif

	return cnt;
}

/**
 * @brief  Return DMA address pointer to push data
 * @param  N/A
 * @retval Valid-address : DMA address to push
 * @retval 0 : No space to push
 */
dma_addr_t felica_rxbuf_push_pointer(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (yes == rxbuf.fulfill || !rxbuf.dmapool)
		return 0;

	return rxbuf.slot[rxbuf.wp].dmabase;
}

/**
 * @brief  Return data length possible to push
 * @param  N/A
 * @retval Positive : Data length of the space to push
 * @retval 0        : No space to push
 * @retval Negative : Failure\n
 *           -ENODEV = RX buffer not initialized
 * @note
 */
int felica_rxbuf_push_length(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (!rxbuf.dmapool) {
		pr_err(PRT_NAME ": Error. RX buf not initialized.\n");
		return -ENODEV;
	}

	if (yes == rxbuf.fulfill)
		return 0;

	return RXBUF_M;
}

/**
 * @brief  Update RX buffer status according to the pushed data.
 * @param  len : Data length already pushed
 * @retval 0 : Success
 * @retval Negative : Failure\n
 *           -EIO    = Invalid arguments\n
 *           -ENODEV = RX buffer not initialized.\n
 *           -EFAULT = RX buf is already fulfilled.
 * @note
 */
int felica_rxbuf_push_status_update(int len)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if ((0 >= len) || (RXBUF_M < len)) {
		pr_err(PRT_NAME ": Error. Invalid arg for push status.\n");
		return -EINVAL;
	}
	if (!rxbuf.dmapool) {
		pr_err(PRT_NAME ": Error. RX buf not initialized.\n");
		return -ENODEV;
	}

	if (yes == rxbuf.fulfill) {
		pr_err(PRT_NAME ": Error. RX buf is already fulfilled.\n");
		return -EFAULT;
	}

	rxbuf.slot[rxbuf.wp].head = len;
	rxbuf.wp++;
	rxbuf.wp %= RXBUF_N;
	if (rxbuf.wp == rxbuf.rp)
		rxbuf.fulfill = yes;
	rxbuf.count += len;

	return 0;
}

/**
 * @brief  Initialize & alloc RX buffer
 * @details This function executes;\n
 *          # Create DMA pool\n
 *          # Alloc RX buffer\n
 *          # Call RX buffer clear
 * @param  N/A
 * @retval 0 : Success
 * @retval -ENOMEM : Error, no enough memory.
 * @note
 */
int felica_rxbuf_init(void)
{
	int i;

	pr_debug(PRT_NAME ": %s\n", __func__);

	rxbuf.dmapool = dma_pool_create(DMAPOOL_NAME, NULL, DMAPOOL_SIZE,
					DMAPOOL_ALIGN, DMAPOOL_ALIGN * RXBUF_N);
	if (!rxbuf.dmapool) {
		pr_err(PRT_NAME ": Error. Cannot create DMA pool for RXbuf.");
		return -ENOMEM;
	}
	for (i = 0; i < RXBUF_N; i++) {
		rxbuf.slot[i].buf = dma_pool_alloc(rxbuf.dmapool, GFP_KERNEL,
						&rxbuf.slot[i].dmabase);
		if (!rxbuf.slot[i].buf) {
			pr_err(PRT_NAME
			    ": Error. No enough mem for RXbuf.\n");
			goto err_alloc_rx_buf;
		}
	}

	felica_rxbuf_clear();

	return 0;

err_alloc_rx_buf:
	for (i--; i >= 0; i--) {
		dma_pool_free(rxbuf.dmapool, rxbuf.slot[i].buf,
					rxbuf.slot[i].dmabase);
	}
	dma_pool_destroy(rxbuf.dmapool);
	rxbuf.dmapool = NULL;
	return -ENOMEM;
}

/**
 * @brief  Release RX buffer
 * @details This function executes;\n
 *          # Release RX buffer\n
 *          # Destroy DMA pool
 * @param  N/A
 * @retval N/A
 * @note
 */
void felica_rxbuf_exit(void)
{
	int i;

	pr_debug(PRT_NAME ": %s\n", __func__);

	for (i = 0; i < RXBUF_N; i++) {
		dma_pool_free(rxbuf.dmapool, rxbuf.slot[i].buf,
					rxbuf.slot[i].dmabase);
	}
	dma_pool_destroy(rxbuf.dmapool);
	rxbuf.dmapool = NULL;
}
