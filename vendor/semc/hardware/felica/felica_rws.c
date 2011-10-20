/* vendor/semc/hardware/felica/felica_rws.c
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
#include <linux/uaccess.h>
#include "felica_rws.h"

#define PRT_NAME "felica rws"
#define ONEBYTE  1
#define RWS_LOW  0x0
#define RWS_HIGH 0x1

/* Module parameters for Reader/Writer mode */
unsigned int st_usbcon;
unsigned int st_airplane;
unsigned int ta_rwusb;

module_param(st_usbcon, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(st_usbcon, "USB status for FeliCa");
module_param(st_airplane, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(st_airplane, "Airplane mode status for FeliCa");
module_param(ta_rwusb, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ta_rwusb, "FeliCa RW disabling for USB");

/**
 * @brief   Open file operation of FeliCa RWS controller
 * @param   inode : (unused)
 * @param   file  : (unused)
 * @retval  0     : Success
 * @note
 */
static int felica_rws_open(struct inode *inode, struct file *file)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief   Close file operation of FeliCa RWS controller
 * @param   inode : (unused)
 * @param   file  : (unused)
 * @retval  0     : Success
 * @note
 */
static int felica_rws_release(struct inode *inode, struct file *file)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief   Read file operation of FeliCa RWS controller
 * @details This function executes;\n
 *            # Check R/W module parameters\n
 *            # Determine RWS value\n
 *            # Copy the value to user space
 * @param   file      : (unused)
 * @param   buf       : Destination of the read data
 * @param   count     : Data length must be 1
 * @param   offset    : (unused)
 * @retval  ONEBYTE(1): Success.
 * @retval  Negative  : Failure\n
 *            -EINVAL = Invalid argument\n
 *            -EIO    = Invalid module parameters\n
 *            -EFAULT = Cannot copy data to user space
 * @note
 */
static ssize_t felica_rws_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	char kbuf;

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (ONEBYTE != count  || !buf) {
		pr_err(PRT_NAME ": Error. Invalid arg @RWS read.\n");
		return -EINVAL;
	}

	pr_debug(PRT_NAME ": st_usbcon = 0x%x\n", st_usbcon);
	pr_debug(PRT_NAME ": st_airplane = 0x%x\n", st_airplane);
	pr_debug(PRT_NAME ": ta_rwusb = 0x%x\n", ta_rwusb);

	/* Check R/W module parameters */
	if (PARAM_OFF != st_usbcon && PARAM_ON != st_usbcon) {
		pr_err(PRT_NAME ": Error. Invalid st_usbcon value.\n");
		return -EIO;
	}
	if (PARAM_OFF != st_airplane && PARAM_ON != st_airplane) {
		pr_err(PRT_NAME ": Error. Invalid st_airplane value.\n");
		return -EIO;
	}
	if (PARAM_OFF != ta_rwusb && PARAM_ON != ta_rwusb) {
		pr_err(PRT_NAME ": Error. Invalid ta_rwusb value.\n");
		return -EIO;
	}

	/* Determine RWS value */
	if ((PARAM_ON == st_usbcon || PARAM_ON == st_airplane)
					&& PARAM_OFF == ta_rwusb)
		kbuf = RWS_HIGH;
	else
		kbuf = RWS_LOW;

	/* Copy the value to user space */
	if (copy_to_user(buf, &kbuf, ONEBYTE)) {
		pr_err(PRT_NAME ": Error. copy_to_user failure.\n");
		return -EFAULT;
	}

	/* 1 byte read */
	return ONEBYTE;
}

/***************** RWS FOPS ****************************/
static const struct file_operations felica_rws_fops = {
	.owner		= THIS_MODULE,
	.read		= felica_rws_read,
	.write		= NULL,
	.ioctl		= NULL,
	.open		= felica_rws_open,
	.release	= felica_rws_release,
};

static struct miscdevice felica_rws_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "felica_rws",
	.fops = &felica_rws_fops,
};

/**
 * @brief  Initilialize FeliCa RWS controller
 * @details This function executes;\n
 *            # Initialize R/W module parameters\n
 *            # Create RWS character device (/dev/felica_rws)
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Initialization failed.\n
 *            -ENOMEM = Cannot create char dev
 * @note
 */
int felica_rws_probe_func(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Initialize R/W module parameters */
	st_usbcon = PARAM_OFF;
	st_airplane = PARAM_OFF;
	ta_rwusb = PARAM_OFF;

	/* Create RWS character device (/dev/felica_rws) */
	if (misc_register(&felica_rws_device)) {
		pr_err(PRT_NAME ": Error. Cannot register RWS.\n");
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief   Terminate FeliCa RWS controller
 * @details This function executes;\n
 *            # Deregister RWS character device (/dev/felica_rws)\n
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_rws_remove_func(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	misc_deregister(&felica_rws_device);
}
