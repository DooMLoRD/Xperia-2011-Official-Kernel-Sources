/* vendor/semc/hardware/felica/felica_txbuf.c
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
#include <linux/uaccess.h>
#include "felica_txbuf.h"

#define PRT_NAME "txbuf"

/* DEBUG_TXBUF_DUMP : 0 = without dump, 1 = with dump */
#define DEBUG_TXBUF_DUMP 0

struct txbuf_all {
	int ptr;
	int num;
	char *buf;
	dma_addr_t dmabase;
};

static struct txbuf_all txbuf;


/**
 * @brief  Clear TX buffer
 * @param  N/A
 * @retval N/A
 * @note
 */
void felica_txbuf_clear(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	txbuf.num = 0;
	txbuf.ptr = 0;
}

/**
 * @brief  Count the number of the stored data in TX buffer
 * @param  N/A
 * @retval 0/Positive : the number of the stored data
 * @retval Negative   : Failure\n
 *           -ENODEV = TX buf not initialized
 * @note
 */
int felica_txbuf_count(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (!txbuf.buf) {
		pr_err(PRT_NAME ": Error. TX buf not initialized.\n");
		return -ENODEV;
	}

	return txbuf.num - txbuf.ptr;
}

/**
 * @brief  Copy user space data to the TX buffer
 * @param  buf : Pointer to User space buffer
 * @param  len : Date length to copy
 * @retval Positive : Number of the data copied successfully.
 * @retval Negative : Failure\n
 *           -EINVAL = Invalid arguments\n
 *           -ENODEV = TX buf not initialized
 *           -EFBIG  = Data is too big\n
 *           -EFAULT = Data copy error
 * @note
 */
int felica_txbuf_puts(const char __user *buf, int len)
{
	int ret;
	int rest;

#if DEBUG_TXBUF_DUMP
	int i;
#endif

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (0 >= len || !buf) {
		pr_err(PRT_NAME ": Error. Invalid argument.\n");
		return -EINVAL;
	}
	if (!txbuf.buf) {
		pr_err(PRT_NAME ": Error. TX buf not initialized.\n");
		return -ENODEV;
	}

	rest = TXBUF_SIZE - txbuf.num;

	if (len > rest) {
		pr_err(PRT_NAME ": Error. Too many data to write.\n");
		return -EFBIG;
	}

	ret = copy_from_user(txbuf.buf + txbuf.num, buf, len);
	if (ret) {
		pr_err(PRT_NAME ": Error. copy_from_user failure.\n");
		return -EFAULT;
	}

#if DEBUG_TXBUF_DUMP
	printk("TXputs dump: ");
	for (i = 0; i < len; i++)
		printk("%02x ", txbuf.buf[txbuf.num + i]);
	printk("\n");
#endif

	txbuf.num += len;

	return len;
}

/**
 * @brief  Return DMA address pointer to pull data
 * @param  N/A
 * @retval Valid-address : DMA address to pull
 * @retval 0 : No space to pull
 * @note
 */
dma_addr_t felica_txbuf_pull_pointer(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (txbuf.ptr == txbuf.num)
		return 0;

	return txbuf.dmabase + txbuf.ptr;
}

/**
 * @brief  Update TX buffer status according to the pushed data.
 * @param  len      : Data length already pushed
 * @retval 0        : Success
 * @retval Negative : Failure\n
 *           -EINVAL = Invalid argument\n
 *           -ENODEV = TX buf not initialized
 * @note
 */
int felica_txbuf_pull_status_update(int len)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	if (0 >= len || TXBUF_SIZE < len) {
		pr_err(PRT_NAME ": Error. Invalid arg for pull status.\n");
		return -EINVAL;
	}
	if (!txbuf.buf) {
		pr_err(PRT_NAME ": Error. TX buf not initialized.\n");
		return -ENODEV;
	}

	if (txbuf.ptr + len > txbuf.num) {
		pr_err(PRT_NAME ": Error. Invalid arg, ptr overtakes num.\n");
		return -EINVAL;
	}
	txbuf.ptr += len;

	return 0;
}

/**
 * @brief  Initialize & alloc TX buffer
 * @details This function executes;\n
 *          # Alloc TX buffer\n
 *          # Call TX buffer clear
 * @param  N/A
 * @retval 0 : Success
 * @retval -ENOMEM : Error, no enough memory.
 * @note
 */
int felica_txbuf_init(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	txbuf.buf = dma_alloc_coherent(NULL, sizeof(char) * TXBUF_SIZE,
					&txbuf.dmabase, GFP_KERNEL);
	if (!txbuf.buf) {
		pr_err(PRT_NAME ": Error. No enough mem for TXbuf.\n");
		return -ENOMEM;
	}

	felica_txbuf_clear();

	return 0;
}

/**
 * @brief  Release TX buffer
 * @details This function executes;\n
 *          # Release TX buffer
 * @param  N/A
 * @retval N/A
 * @note
 */
void felica_txbuf_exit(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	dma_free_coherent(NULL, sizeof(char) * TXBUF_SIZE,
					txbuf.buf, txbuf.dmabase);
	txbuf.buf = NULL;
}

