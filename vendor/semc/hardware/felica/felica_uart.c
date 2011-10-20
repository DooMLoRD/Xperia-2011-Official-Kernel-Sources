/* vendor/semc/hardware/felica/felica_uart.c
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
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mfd/pmic8058.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "semc_felica_ext.h"
#include "felica_uart.h"
#include "msm_uartdm.h"
#include "msm_uartmux.h"
#include "felica_rxbuf.h"
#include "felica_txbuf.h"
#include "felica_statemachine.h"
#include "felica_statemachine_userdata.h"

#define PRT_NAME "felica uart"

struct felica_uart_data {
	struct felica_uart_pfdata	*pfdata;
	/* Reference count of FeliCa UART */
	int			refcnt;
	/* TX reservation during RX */
	enum {no, yes}		tx_reserved;
	/* Waitquese variables for fsync */
	wait_queue_head_t	wait_fsync;
	int			condition_fsync;
	/* Result of fsync */
	enum {success, error}	result_fsync;
	/* Previous available result */
	int			prev_avail;
};

static struct felica_uart_data *fluart;


/**
 * @brief  Action to do nothing
 * @param  nodata : (unused)
 * @retval 0
 * @note
 */
int felica_do_nothing(void *nodata)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief   Execute OPEN sequence of FeliCa UART, after checking UART MUX state
 * @details This function executes;\n
 *            # Check UART MUX\n
 *            # Set UART MUX for FeliCa\n
 *            # Call UARTDM open function\n
 *            # Clear tx_reserved variable\n
 *            # Call UART 1byte-reception enable function\n
 *            # Increment the reference count to 1
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EBUSY = The UART is occupied by another\n
 *            -EIO   = UART MUX access error / UARTDM open error /
 *                     Cannot enable 1byte-reception
 * @note
 */
int felica_do_open_seq(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check UART MUX state*/
	ret = msm_uartmux_get();
	if (0 > ret) {
		pr_err(PRT_NAME ": Error. Cannot get UART MUX.\n");
		ret = -EIO;
		goto err_uartmux_get;
	} else if (ret != fluart->pfdata->uartmux_neutral) {
		pr_err(PRT_NAME ": Error. UART MUX not available.\n");
		ret = -EBUSY;
		goto err_uartmux_get;
	}

	/* Set UART MUX for FeliCa */
	ret = msm_uartmux_set(fluart->pfdata->uartmux_felica);
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot set UART MUX.\n");
		ret = -EIO;
		goto err_uartmux_set;
	}

	/* Call UARTDM open function */
	ret = msm_uartdm_open();
	if (ret) {
		pr_err(PRT_NAME ": Error. UART Open failed.\n");
		ret = -EIO;
		goto err_uartdm_open;
	}

	/* Clear tx_reserved variable */
	fluart->tx_reserved = no;

	/* Call UART 1byte-reception enable function */
	ret = msm_uartdm_rcv_1byte_enable();
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot enable RCV.\n");
		ret = -EIO;
		goto err_uartdm_rcv_1byte_enable;
	}

	/* Increment the reference count to 1 */
	fluart->refcnt = 1;

	return 0;

err_uartdm_rcv_1byte_enable:
	msm_uartdm_close();
err_uartdm_open:
	msm_uartmux_set(fluart->pfdata->uartmux_neutral);
err_uartmux_set:
err_uartmux_get:
	return ret;
}

/**
 * @brief   Execute Open again
 * @details This function executes;\n
 *            # Increment the reference count to 2
 * @param   nodata : (unused)
 * @retval  0      : Success
 * @note
 */
int felica_do_open_again(void *nodata)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	fluart->refcnt = 2;

	return 0;
}

/**
 * @brief   Check if UART can be closed. Issue EXEC_CLOSE_SIG if possible.
 * @details This function executes;\n
 *            # Decrement the reference count\n
 *            # If it becomes 0 or less, request EXEC_CLOSE_SIG event.
 * @param   nodata : (unused)
 * @retval  0      : Success
 * @note
 */
int felica_do_close_check(void *nodata)
{
	struct event_struct evset = {exec_close_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	fluart->refcnt--;
	if (0 >= fluart->refcnt)
		felica_request_nextevent(evset);

	return 0;
}

/**
 * @brief   Execute Close sequence of FeliCa UART port.
 * @details This function executes;\n
 *            # Call UART 1byte-reception disable function\n
 *            # Call UARTDM close function\n
 *            # Set UART MUX to Neutral
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EIO = Cannot disable 1byte-reception / UARTDM close error /
 *                   UART MUX write error
 * @note
 */
int felica_do_close_seq(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_rcv_1byte_disable();
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot disable RCV.\n");
		ret = -EIO;
		goto err_uartdm_rcv_disable;
	}

	ret = msm_uartdm_close();
	if (ret) {
		pr_err(PRT_NAME ": Error. UARTDM close failed.\n");
		ret = -EIO;
		goto err_uartdm_close;
	}

	ret = msm_uartmux_set(fluart->pfdata->uartmux_neutral);
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot set UART MUX.\n");
		ret = -EIO;
		goto err_uartmux_set;
	}

	return 0;

err_uartdm_rcv_disable:
	msm_uartdm_close();
err_uartdm_close:
	msm_uartmux_set(fluart->pfdata->uartmux_neutral);
err_uartmux_set:
	return ret;
}

/**
 * @brief   Execute Read sequence of FeliCa UART port.
 * @details This function executes;\n
 *            # Check data length in RX buffer\n
 *            # If data exists, get char data from RX buffer
 * @param   data     : Data structure for Read\n
 *           .buf    : Pointer to User space\n
 *           .count  : Length to read
 * @retval  Positive : Success. Length of the data read successfully
 * @retval  0        : Success. No data to read
 * @retval  Negative : Failure\n
 * @retval    -EINVAL = The arguments are invalid.\n
 * @retval    -EFAULT = Data cannot be copied from RX buffer.
 * @note
 */
int felica_do_read_seq(void *data)
{
	int ret;
	struct read_data_struct *dataset = data;

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (!dataset->buf || !dataset->count) {
		pr_err(PRT_NAME ": Error. Invalid arg @read.");
		return -EINVAL;
	}

	ret = felica_rxbuf_count();
	if (0 > ret) {
		pr_err(PRT_NAME ": Error. RXbuf check failed.");
		return -EFAULT;
	}

	if (0 < ret) {
		ret = felica_rxbuf_gets(dataset->buf, dataset->count);
		if (0 > ret) {
			pr_err(PRT_NAME ": Error. RXbuf access failed.");
			ret = -EFAULT;
		}
	}

	return ret;
}

/**
 * @brief   Execute Available sequence of FeliCa UART port.
 * @details This function executes;\n
 *            # Check data length in RX buffer
 * @param   nodata     : (unused)
 * @retval  0/Positive : Success. Length of data stored in RX buffer
 * @retval  Negative   : Failure\n
 *            -EFAULT = RX buffer access failed.
 * @note
 */
int felica_do_available_seq(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = felica_rxbuf_count();
	if (0 > ret) {
		pr_err(PRT_NAME ": Error. RXbuf check failed.");
		return -EFAULT;
	}

	return ret;
}

/**
 * @brief   Execute Write sequence of FeliCa UART port.
 * @details This function executes;\n
 *            # Put char data to TX buffer
 * @param   data     : Data structure for Write\n
 *           .buf    : Pointer to User space\n
 *           .count  : Length to write
 * @retval  Positive : Success. Length of the data written successfully
 * @retval  Negative : Failure\n
 *            -EINVAL = The arguments are invalid.\n
 *            -EFAULT = Data cannot be copied to TX buffer.
 * @note
 */
int felica_do_write_seq(void *data)
{
	int ret;
	struct write_data_struct *dataset = data;

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (!dataset->buf || !dataset->count) {
		pr_err(PRT_NAME ": Error. Invalid arg @write.");
		return -EINVAL;
	}

	ret = felica_txbuf_puts(dataset->buf, dataset->count);
	if (0 > ret) {
		pr_err(PRT_NAME ": Error. TXbuf access failed.");
		return -EFAULT;
	}

	return ret;
}

/**
 * @brief   Check if fsync should be executed. Issue EXE_FSYNC_SIG if necessary.
 * @details This function executes;\n
 *            # Count TX buffer\n
 *            # [If TX data exists,] request EXEC_FSYNC_SIG.
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODATA = No data for fsync.\n
 *            -EFAULT  = TX buffer check failed.
 * @note
 */
int felica_do_fsync_check(void *nodata)
{
	int ret;
	struct event_struct evset = {exec_fsync_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = felica_txbuf_count();
	if (0 > ret) {
		pr_err(PRT_NAME ": Error. TXbuf check failed.");
		return -EFAULT;
	} else if (0 == ret) {
		pr_debug(PRT_NAME ": No data to do fsync.");
		return -ENODATA;
	}
	felica_request_nextevent(evset);

	return 0;
}

/**
 * @brief   Check if fsync should be executed, and reserve fsync if necessary.
 * @details This function executes;\n
 *            # Count TX buffer\n
 *            # [If TX data exists,] set tx_reserved variable as reserved.
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EFAULT  = TX buffer access failed.\n
 *            -ENODATA = No data for fsync.
 * @note
 */
int felica_do_fsync_reserve(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = felica_txbuf_count();
	if (0 > ret) {
		pr_err(PRT_NAME ": Error. TXbuf check failed.");
		return -EFAULT;
	} else if (0 == ret) {
		pr_debug(PRT_NAME ": No data to do fsync.");
		return -ENODATA;
	}

	fluart->tx_reserved = yes;

	return 0;
}

/**
 * @brief   Cancel reserved fsync.
 * @details This function executes;\n
 *            # Set tx_reserved variable as not-reserved.
 * @param   nodata : (unused)
 * @retval  0      : Success
 * @note
 */
int felica_do_fsync_cancel(void *nodata)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	fluart->tx_reserved = no;

	return 0;
}

/**
 * @brief   Start UARTDM RX transfer
 * @details This function executes;\n
 *            # Call UARTDM RX start function\n
 *            # If RX start failed, call UARTDM RX reset function.\n
 *            # If RX start failed, enable RX 1byte reception interrupt.
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EIO = UART RX start failed
 * @note
 */
int felica_do_rx_start(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_rx_start();
	if (ret) {
		pr_err(PRT_NAME ": Error. UART RX start failed.");
		msm_uartdm_rx_reset();
		msm_uartdm_rcv_1byte_enable();
		return -EIO;
	}

	return 0;
}

/**
 * @brief   Execute finish sequence of UARTDM RX
 * @details This function executes;\n
 *            # Check UARTDM received byte\n
 *            # [If additional data exists,] request RECEIVE_1BYTE_SIG event.\n
 *            # [If no data but TX reserved,] request EXEC_FSYNC_SIG event.\n
 *            # [If else,] request RX_TO_IDLE_SIG event.
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @note
 */
int felica_do_rx_finish(void *nodata)
{
	int ret;
	struct event_struct evset = {receive_1byte_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_rcv_byte_check();
	if (0 == ret) {
		if (yes == fluart->tx_reserved)
			evset.ev = exec_fsync_sig;
		else
			evset.ev = rx_to_idle_sig;
	}
	felica_request_nextevent(evset);

	return 0;
}

/**
 * @brief   Reset UARTDM RX port
 * @details This function executes;\n
 *            # Call UARTDM RX reset function\n
 *            # Request RX_COMPLETE_SIG event.
 * @param   nodata   : (unused)
 * @retval  0        : Success.
 * @retval  Negative : Failure\n
 *            -EIO = UARTDM RX reset failed
 * @note
 */
int felica_do_rx_reset(void *nodata)
{
	int ret;
	struct event_struct evset = {rx_complete_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_rx_reset();
	if (ret) {
		pr_err(PRT_NAME ": Error. UARTDM RX reset failed.");
		return -EIO;
	}

	felica_request_nextevent(evset);

	return 0;
}

/**
 * @brief   Start UARTDM TX transfer
 * @details This function executes;\n
 *            # Disable RX 1byte reception interrupt\n
 *            # Call UARTDM TX start function.
 * @param   nodata   : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EIO = Cannot disable 1byte-reception / UARTDM TX start failed
 * @note
 */
int felica_do_tx_start(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_rcv_1byte_disable();
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot disable RCV.\n");
		return -EIO;
	}

	ret = msm_uartdm_tx_start();
	if (ret) {
		pr_err(PRT_NAME ": Error. UARTDM TX start failed.");
		return -EIO;
	}

	return 0;
}

/**
 * @brief   Execute finish sequence of UARTDM TX
 * @details This function executes;\n
 *            # Clear TX buffer\n
 *            # Enable RX 1byte reception interrupt
 * @param   nodata   : (unused)
 * @retval  0        : Success.
 * @retval  Negative : Failure\n
 *            -EIO   = Cannot enable 1byte-reception
 * @note
 */
int felica_do_tx_finish(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	felica_txbuf_clear();

	ret = msm_uartdm_rcv_1byte_enable();
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot enable RCV.\n");
		return -EIO;
	}

	return 0;
}

/**
 * @brief   Reset UARTDM TX port
 * @details This function executes;\n
 *            # Call UARTDM TX reset\n
 *            # Enable RX 1byte reception interrupt
 * @param   nodata   : (unused)
 * @retval  0        : Success.
 * @retval  Negative : Failure\n
 *            -EIO = UARTDM TX reset failed / Cannot enable 1byte-reception
 * @note
 */
int felica_do_tx_reset(void *nodata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_tx_reset();
	if (ret) {
		pr_err(PRT_NAME ": Error. UARTDM TX reset failed.");
		return -EIO;
	}

	ret = msm_uartdm_rcv_1byte_enable();
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot enable RCV.\n");
		return -EIO;
	}

	return 0;
}

/**
 * @brief   Return from RX state to IDLE state
 * @details This function executes;\n
 *            # Enable RX 1byte reception interrupt
 * @param   nodata : (unused)
 * @retval  0      : Success.
 * @retval  Negative : Failure\n
 *            -EIO   = Cannot enable 1byte-reception
 * @note
 * @note
 */
int felica_do_rx_to_idle(void *nodata)
{
	int ret;
	pr_debug(PRT_NAME ": %s\n", __func__);

	ret = msm_uartdm_rcv_1byte_enable();
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot enable RCV.\n");
		return -EIO;
	}

	return 0;
}

/**
 * @brief   Handling RX 1byte reception callback
 * @details This function executes;\n
 *            # Issue RECEIVE_1BYTE_SIG event
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_cb_rcv_1byte(void)
{
	struct event_struct evset = {receive_1byte_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	felica_event_handle(evset);
}

/**
 * @brief   Handling RX complete callback
 * @details This function executes;\n
 *            # Issue RX_COMPLETE_SIG event
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_cb_rx_complete(void)
{
	struct event_struct evset = {rx_complete_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	felica_event_handle(evset);
}

/**
 * @brief   Handling RX error callback
 * @details This function executes;\n
 *            # Issue UARTDM_ERROR_SIG event
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_cb_rx_error(void)
{
	struct event_struct evset = {uartdm_error_sig, NULL};

	pr_debug(PRT_NAME ": %s\n", __func__);

	felica_event_handle(evset);
}

/**
 * @brief   Handling TX complete callback
 * @details This function executes;\n
 *            # Issue TX_COMPLETE_SIG event\n
 *            # Set result_fsync variable as success\n
 *            # Wakeup FSYNC wait
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_cb_tx_complete(void)
{
	struct event_struct evset = {tx_complete_sig, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	result = felica_event_handle(evset);

	fluart->result_fsync = success;

	fluart->condition_fsync = 1;
	wake_up(&fluart->wait_fsync);
}

/**
 * @brief   Handling TX error callback
 * @details This function executes;\n
 *            # Issue UARTDM_ERROR_SIG event\n
 *            # Set result_fsync variable as error\n
 *            # Wakeup FSYNC wait
 * @param   N/A
 * @retval  N/A
 */
void felica_cb_tx_error(void)
{
	struct event_struct evset = {uartdm_error_sig, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	result = felica_event_handle(evset);

	fluart->result_fsync = error;

	fluart->condition_fsync = 1;
	wake_up(&fluart->wait_fsync);
}

/**
 * @brief   Handle OPEN system call of FeliCa UART
 * @details This function executes;\n
 *            # Issue OPEN_SYSCALL event
 * @param   inode    : (unused)
 * @param   file     : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EIO = Action failed.
 */
static int felica_uart_open(struct inode *inode, struct file *file)
{
	struct event_struct evset = {open_syscall, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	result = felica_event_handle(evset);

	fluart->prev_avail = 0;

	if (0 > result.retval)
		return -EIO;

	return 0;
}

/**
 * @brief   Handle CLOSE system call of FeliCa UART
 * @details This function executes;\n
 *            # Issue CLOSE_SYSCALL event
 * @param   inode    : (unused)
 * @param   file     : (unused)
 * @retval  Negative : Failure\n
 *            -EIO = Action failed.
 */
static int felica_uart_release(struct inode *inode, struct file *file)
{
	struct event_struct evset = {close_syscall, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	result = felica_event_handle(evset);

	if (0 > result.retval)
		return -EIO;

	return 0;
}

/**
 * @brief   Handle READ system call of FeliCa UART
 * @details This function executes;\n
 *            # Issue READ_SYSCALL event\n
 *            # [Optional][If no data,] sleep
 * @param   file     : (unused)
 * @param   buf      : Destination of the read data
 * @param   count    : Data length to read
 * @param   offset   : (unused)
 * @retval  Positive : Success. Length of possibly read data
 * @retval  0        : Success. No data to read
 * @retval  Negative : Failure\n
 *            -EINVAL = The arguments are invalid.\n
 *            -EIO    = Action to read data failed.
 */
static ssize_t felica_uart_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	struct read_data_struct dataset = {buf, count};
	struct event_struct evset = {read_syscall, (void *) &dataset};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s (%d)\n", __func__, count);

	if (!buf || !count) {
		pr_err(PRT_NAME ": Error. Invalid arg @read.\n");
		return -EINVAL;
	}

	result = felica_event_handle(evset);
	pr_debug(PRT_NAME ": Read result = %d.\n", result.retval);

	/* [Optional][If no data,] sleep */
	if ((0 == result.retval) && (0 < FELICA_UART_READ_MSLEEP)) {
		pr_debug(PRT_NAME ": Read sleep(%dms).\n",
					FELICA_UART_READ_MSLEEP);
		msleep(FELICA_UART_READ_MSLEEP);
	}

	if (0 > result.retval)
		return -EIO;

	return result.retval;
}

/**
 * @brief   Handle WRITE system call of FeliCa UART
 * @details This function executes;\n
 *            # Issue WRITE_SYSCALL event.
 * @param   file     : (unused)
 * @param   buf      : Source of the written data
 * @param   count    : Data length to write
 * @param   offset   : (unused)
 * @retval  Positive : Success. Length of the data written successfully.
 * @retval  Negative : Failure\n
 *            -EINVAL = The arguments are invalid.\n
 *            -EIO    = Action to write data failed.
 */
static ssize_t felica_uart_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset)
{
	struct write_data_struct dataset =  {buf, count};
	struct event_struct evset = {write_syscall, (void *) &dataset};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s (%d)\n", __func__, count);

	if (!buf || !count) {
		pr_err(PRT_NAME ": Error. Invalid arg @available.\n");
		return -EINVAL;
	}

	result = felica_event_handle(evset);

	if (0 > result.retval)
		return -EIO;

	return result.retval;
}

/**
 * @brief   Handle FSYNC system call of FeliCa UART
 * @details This function executes;\n
 *            # Issue FSYNC_SYSCALL event\n
 *            # [If no TX data exists,] finish successfully.\n
 *            # Wait until TX completes.\n
 *            # [If wait timeout,] issue FSYNC_TIMOUT_SIG.\n
 *            # Check TX result.
 * @param   file     : (unused)
 * @param   dentry   : (unused)
 * @param   datasync : (unused)
 * @retval  0        : Success.
 * @retval  Negative : Failure\n
 *            -EFAULT = DMA transfer error occurred.
 *            -EAGAIN = Time out occurs.
 */
static int felica_uart_fsync(struct file *file, struct dentry *dentry,
							int datasync)
{
	int ret;
	struct event_struct evset = {fsync_syscall, NULL};
	struct event_struct evset_to = {fsync_timeout_sig, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	fluart->condition_fsync = 0;

	result = felica_event_handle(evset);

	if (-ENODATA == result.retval) {
		pr_debug(PRT_NAME ": No data to do fsync.\n");
		return 0;
	} else if (0 > result.retval) {
		pr_err(PRT_NAME ": Error. FSYNC error.\n");
		return -EFAULT;
	}

	if (0 == fluart->condition_fsync) {
		pr_debug(PRT_NAME ": FSYNC waiting ...\n");
		/* Wait here until TX completes */
		ret = wait_event_timeout(fluart->wait_fsync,
			fluart->condition_fsync, FELICA_UART_FSYNC_TIMEOUT);
		/* Time out case */
		if (0 == ret) {
			pr_err(PRT_NAME ": Error. FSYNC timeout.\n");
			result = felica_event_handle(evset_to);
			return -EAGAIN;
		}
		/* Check fsync result */
		if (error == fluart->result_fsync) {
			pr_err(PRT_NAME ": Error. FSYNC error.\n");
			return -EFAULT;
		}
	}

	pr_debug(PRT_NAME ": FSYNC succeeded.\n");

	return 0;
}

/**
 * @brief   Handle IOCTL system call of FeliCa UART
 * @details This function executes;\n
 *            # Check IO control number is Available (FIONREAD).\n
 *            # Issue AVAIABLE_SYSCALL event.\n
 *            # Copy result to User space\n
 *            # [Optional][If data not increase,] sleep
 * @param   inode    : (unused)
 * @param   file     : (unused)
 * @param   cmd      : IOCTL command number
 * @param   arg      : IOCTL arguments
 * @retval  0        : Success.
 * @retval  Negative : Failure.\n
 *            -EINVAL = The arguments are invalid for Available.\n
 *            -EFAULT = RX buffer/Data copy error to execute Available.\n
 *            -ENOTTY = Unsupported IOCTL number
 */
static int felica_uart_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int ret;
	struct event_struct evset = {available_syscall, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (IOCTL_UART_AVAILABLE == cmd) {
		pr_debug(PRT_NAME ": cmd = AVAILABLE\n");
		if (!arg) {
			pr_err(PRT_NAME
				": Error. Invalid arg @available.\n");
			return -EINVAL;
		}

		result = felica_event_handle(evset);
		if (0 > result.retval) {
			pr_err(PRT_NAME ": Error. RX buffer check failed.\n");
			return -EFAULT;
		}

		ret = copy_to_user((int __user *) arg,
					&result.retval, sizeof(int));
		if (ret) {
			pr_err(PRT_NAME
				": Error. copy_to_user failed.\n");
			return -EFAULT;
		}
		pr_debug(PRT_NAME ": Availble result = %d.\n", result.retval);

		/* [Optional][If data not increase,] sleep */
		if ((result.retval == fluart->prev_avail)
				&& (0 < FELICA_UART_AVAILABLE_MSLEEP)) {
			pr_debug(PRT_NAME ": Available sleep(%dms).\n",
						FELICA_UART_AVAILABLE_MSLEEP);
			msleep(FELICA_UART_AVAILABLE_MSLEEP);
		}
		fluart->prev_avail = result.retval;

		return 0;
	} else {
		pr_err(PRT_NAME ": Error. Unsupported IOCTL.\n");
		return -ENOTTY;
	}
}

/***************** UART FOPS ****************************/
static const struct file_operations felica_uart_fops = {
	.owner		= THIS_MODULE,
	.read		= felica_uart_read,
	.write		= felica_uart_write,
	.ioctl		= felica_uart_ioctl,
	.open		= felica_uart_open,
	.release	= felica_uart_release,
	.fsync		= felica_uart_fsync,
};

static struct miscdevice felica_uart_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "felica",
	.fops = &felica_uart_fops,
};

/**
 * @brief   Initialize FeliCa UART and register its device
 * @details This function executes;\n
 *            # Check UART platform data\n
 *            # Alloc and Initialize UART controller's data\n
 *            # Call UARTDM module init function\n
 *            # Call UARTMUX module init function\n
 *            # Init UART ref count\n
 *            # Create UART character device (/dev/felica)\n
 *            # State-machine initialization
 * @param   pfdata   : Pointer to UART platform data
 * @retval  0        : Success
 * @retval  Negative : Initialization failed.\n
 *            -EINVAL = No platform data\n
 *            -ENOMEM = No enough memory / Cannot create char dev.\n
 *            -EIO    = UARTDM init error.\n
 *            -ENODEV = Transition definition not exists.
 * @note
 */
int felica_uart_probe_func(struct felica_uart_pfdata *pfdata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check platform data argment */
	if (!pfdata) {
		pr_err(PRT_NAME ": Error. No platform data for FeliCa UART.\n");
		ret = -EINVAL;
		goto err_check_platform_data;
	}

	/* Alloc driver data area */
	fluart = kzalloc(sizeof(struct felica_uart_data), GFP_KERNEL);
	if (!fluart) {
		pr_err(PRT_NAME ": Error. No enough mem for UART.\n");
		ret = -ENOMEM;
		goto err_alloc_uart_data;
	}
	/* Import UART platform data */
	fluart->pfdata = pfdata;
	/* Init wait queue */
	init_waitqueue_head(&fluart->wait_fsync);
	/* Set UARTDM callbacks functions */
	pfdata->uartdm_pfdata.callback_rcv_1byte = felica_cb_rcv_1byte;
	pfdata->uartdm_pfdata.callback_rx_complete = felica_cb_rx_complete;
	pfdata->uartdm_pfdata.callback_rx_error = felica_cb_rx_error;
	pfdata->uartdm_pfdata.callback_tx_complete = felica_cb_tx_complete;
	pfdata->uartdm_pfdata.callback_tx_error = felica_cb_tx_error;

	/* UARTDM module init */
	ret = msm_uartdm_init(&pfdata->uartdm_pfdata);
	if (ret) {
		pr_err(PRT_NAME ": Error. UARTDM init failed.\n");
		ret = -EIO;
		goto err_msm_uartdm_init;
	}

	/* UARTMUX module init */
	msm_uartmux_init();

	/* Init UART ref count*/
	fluart->refcnt = 0;

	/* Crate UART character device */
	ret = misc_register(&felica_uart_device);
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot register UART.\n");
		ret = -ENOMEM;
		goto err_create_uart_dev;
	}

	/* State machine initialization */
	if (NULL == tran[0][0].action) {  /* Must refer to tran for build */
		ret = -ENODEV;
		goto err_tran_definition;
	}
	felica_statemachine_init();

	return 0;

err_tran_definition:
	misc_deregister(&felica_uart_device);
err_create_uart_dev:
	msm_uartmux_exit();
	msm_uartdm_exit();
err_msm_uartdm_init:
	kfree(fluart);
err_alloc_uart_data:
err_check_platform_data:
	return ret;
}

/**
 * @brief   Terminate FeliCa UART controller
 * @details This function executes;\n
 *            # Issue EXEC_CLOSE_SIG for the case UART is opened\n
 *            # State-machine termination\n
 *            # Dedegister UART character device (/dev/felica)\n
 *            # Call UARTMUX module exit function\n
 *            # Call UARTDM module exit function\n
 *            # Release UART controller's data
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_uart_remove_func(void)
{
	struct event_struct evset = {exec_close_sig, NULL};
	struct result_struct result;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Forced exec_close_sig */
	result = felica_event_handle(evset);

	felica_statemachine_exit();
	misc_deregister(&felica_uart_device);
	msm_uartmux_exit();
	msm_uartdm_exit();
	kfree(fluart);
}
