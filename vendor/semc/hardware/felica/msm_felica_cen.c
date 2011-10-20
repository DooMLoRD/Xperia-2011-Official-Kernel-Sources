/* vendor/semc/hardware/felica/msm_felica_cen.c
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
#include <linux/pmic8058-nfc.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "semc_felica_ext.h"
#include "felica_cen.h"
#include <crypto/hash.h>
#include <linux/slab.h>

#define PRT_NAME "felica cen"
#define MON_LEVEL_IS_2P90 (0x0)
#define MON_LEVEL_IS_2P95 (0x1)
#define MON_LEVEL_IS_3P00 (0x2)
#define MON_LEVEL_IS_3P05 (0x3)
#define CEN_LOW  (0x00)
#define CEN_HIGH (0x01)
#define WRITE_LEN 1025
#define AUTHENTICATION_LEN (WRITE_LEN - 1)
#define AUTH_HASH_LEN 32
#define HASH_ALG "sha256"

struct felica_cen_data {
	struct felica_cen_pfdata	*pfdata;
	struct pm8058_nfc_device	*nfcdev;
};

static struct felica_cen_data *flcen;

/** @brief Data of sha256 hash digest controller */
struct sdesc {
	struct shash_desc shash;
	char ctx[];
};

/** @ Authentication hash value*/
const u8 auth_hash[AUTH_HASH_LEN] = {
	0xbf, 0x19, 0x2e, 0xaf, 0x67, 0x0c, 0x90, 0xc8,
	0x12, 0x5f, 0xbb, 0xbc, 0x9e, 0x17, 0x23, 0x39,
	0x69, 0x4a, 0xb7, 0xec, 0x3a, 0x9f, 0x91, 0x57,
	0x80, 0xec, 0xb3, 0xf5, 0xfe, 0x28, 0x8f, 0x33
};

/**
 * @brief Generation of hash digest in SHA256
 * @details This function executes;\n
 *	# Generate hash algorithm structure\n
 *	# Generate calculate controller\n
 *	# Calculation of hash digest in SHA256
 * @param  src : Source of hash
 * @param  src_len : Source of length
 * @param  out : Hash digest in SHA256
 * @retval 0 : Success
 * @retval -EFAULT : Failure
 * @note
 */
int calc_hash(const u8 *src, int src_len, u8 *out)
{
	struct crypto_shash *shash;
	struct sdesc *desc;
	int size;
	int ret = -EFAULT;

	shash = crypto_alloc_shash(HASH_ALG, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(shash)) {
		pr_err(PRT_NAME ": Error. crypto_alloc_shash.\n");
		goto err_shash;
	}

	size = sizeof(struct shash_desc) + crypto_shash_descsize(shash);
	desc = kmalloc(size, GFP_KERNEL);
	if (!desc) {
		pr_err(PRT_NAME ": Error. No enough mem for Desc.\n");
		goto err_desc;
	}

	desc->shash.tfm = shash;
	desc->shash.flags = 0x00;

	if (crypto_shash_digest(&desc->shash, src, src_len, out)) {
		pr_err(PRT_NAME ": Error. generate hash.\n");
		goto err_generate;
	}
	ret = 0;

err_generate:
	kfree(desc);
err_desc:
	crypto_free_shash(shash);
err_shash:
	return ret;
}

/**
 * @brief   Open file operation of FeliCa CEN controller
 * @details This function executes;\n
 *            # Get PMIC8058 NFC device\n
 *            # Read NFC support register\n
 *            # [If NFC support is not enabled,] write initial value.
 * @param   inode    : (unused)
 * @param   file     : (unused)
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -ENODEV = Cannot find PMIC NFC device\n
 *            -EIO    = Cannot access PMIC NFC device
 * @note
 */
static int felica_cen_open(struct inode *inode, struct file *file)
{
	int ret;
	unsigned int st;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Get PM8058 NFC device */
	flcen->nfcdev = pm8058_nfc_request();
	if (NULL == flcen->nfcdev) {
		pr_err(PRT_NAME ": Error. PM-nfc not found.\n");
		return -ENODEV;
	}

	/* Read NFC support register */
	ret = pm8058_nfc_get_status(flcen->nfcdev, PM_NFC_CTRL_REQ, &st);
	if (ret) {
		pr_err(PRT_NAME ": Error. PM-nfc access failed.\n");
		return -EIO;
	}
	pr_debug(PRT_NAME ": PM8058 NFC register = 0x%x\n", st);

	/* [If NFC support is not enabled,] write initial value. */
	if (!(st & PM_NFC_SUPPORT_EN)) {
		ret = pm8058_nfc_config(flcen->nfcdev, PM_NFC_CTRL_REQ,
			PM_NFC_SUPPORT_EN | PM_NFC_LDO_EN | MON_LEVEL_IS_2P95);
		if (ret) {
			pr_err(PRT_NAME ": Cannot enable PM-nfc.\n");
			return -EIO;
		}
		pr_debug(PRT_NAME ": Successfully enabled PM-nfc.\n");
	}

	return 0;
}

/**
 * @brief  Close file operation of FeliCa CEN controller
 * @param  inode : (unused)
 * @param  file  : (unused)
 * @retval 0     : Success
 * @note
 */
static int felica_cen_release(struct inode *inode, struct file *file)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief   Read file operation of FeliCa CEN controller
 * @details This function executes;\n
 *            # Read PMIC8058 NFC support register\n
 *            # Copy PM_NFC_EN value to user space
 * @param   file     : (unused)
 * @param   buf      : Destination of the read data
 * @param   count    : Data length must be 1
 * @param   offset   : (unused)
 * @retval  1        : Success
 * @retval  Negative : Failure\n
 *            -EINVAL = Invalid argment\n
 *            -EIO    = Cannot access PMIC NFC device\n
 *            -EFAULT = Cannot copy data to user space
 * @note
 */
static ssize_t felica_cen_read(struct file *file, char __user *buf,
					size_t count, loff_t *offset)
{
	int ret;
	char kbuf;
	unsigned int st = 0x00000000;

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (1 != count || !buf) {
		pr_err(PRT_NAME ": Error. Invalid arg @CEN read.\n");
		return -EINVAL;
	}

	/* Read PMIC8058 NFC support register */
	ret = pm8058_nfc_get_status(flcen->nfcdev, PM_NFC_CTRL_REQ, &st);
	if (ret) {
		pr_err(PRT_NAME ": Error. PM-nfc access failed.\n");
		return -EIO;
	}
	pr_debug(PRT_NAME ": PM8058 NFC register = 0x%x\n", st);
	kbuf = (st & PM_NFC_EN) ? CEN_HIGH : CEN_LOW;

	/* Copy PM_NFC_EN value to user space */
	ret = copy_to_user(buf, &kbuf, 1);
	if (ret) {
		pr_err(PRT_NAME ": Error. copy_to_user failure.\n");
		return -EFAULT;
	}

	/* 1 byte read */
	return 1;
}

/**
 * @brief   Write file operation of FeliCa CEN controller
 * @details This function executes;\n
 *            # Carry out user authentication\n
 *            # Copy value from user space\n
 *            # Write PMIC8058 NFC support register\n
 *            # usec delay
 * @param   file     : (unused)
 * @param   buf      : Source of the written data
 * @param   count    : Data length must be WRITE_LEN.
 * @param   offset   : (unused)
 * @retval  1        : Success
 * @retval  Negative : Failure\n
 *            -EINVAL = Invalid argument\n
 *            -ENOMEM = No enough memory\n
 *            -EFAULT = Cannot copy data from user space\n
 *            -EIO    = Cannot access PMIC NFC device\n
 *            -EACCES = Permission denied
 */
static ssize_t felica_cen_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset)
{
	int ret;
	char kbuf;
	unsigned int val;
	u8 *src;
	u8 hash[AUTH_HASH_LEN];

	pr_debug(PRT_NAME ": %s\n", __func__);

	if (WRITE_LEN != count  || !buf) {
		pr_err(PRT_NAME ": Error. Invalid arg @CEN write.\n");
		return -EINVAL;
	}

	/* Carry out user authentication */
	src = kmalloc(AUTHENTICATION_LEN, GFP_KERNEL);
	if (!src) {
		pr_err(PRT_NAME ": Error. No enough mem for Auth.\n");
		return -ENOMEM;
	}
	ret = copy_from_user(src, buf, AUTHENTICATION_LEN);
	if (ret) {
		pr_err(PRT_NAME ": Error. copy_from_user failure.\n");
		kfree(src);
		return -EFAULT;
	}

	if (calc_hash(src, AUTHENTICATION_LEN, hash)) {
		pr_err(PRT_NAME ": Error. calc hash digest failure.\n");
		kfree(src);
		return -EACCES;
	}

	if (memcmp(auth_hash, hash, AUTH_HASH_LEN)) {
		pr_err(PRT_NAME ": Error. invalid authentication.\n");
		kfree(src);
		return -EACCES;
	}
	kfree(src);

	/* Copy value from user space */
	ret = copy_from_user(&kbuf, &buf[AUTHENTICATION_LEN], 1);
	if (ret) {
		pr_err(PRT_NAME ": Error. copy_from_user failure.\n");
		return -EFAULT;
	}

	/* Write PMIC8058 NFC support register */
	if (CEN_LOW == kbuf)
		val = 0x0;
	else if (CEN_HIGH == kbuf)
		val = PM_NFC_EN;
	else {
		pr_err(PRT_NAME ": Error. Invalid val @CEN write.\n");
		return -EINVAL;
	}
	ret = pm8058_nfc_config(flcen->nfcdev, PM_NFC_EN, val);
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot write PM-nfc.\n");
		return -EIO;
	}

	/* usec delay*/
	udelay(UDELAY_CEN_WRITE);

	/* 1 byte write */
	return 1;
}

/***************** CEN FOPS ****************************/
static const struct file_operations felica_cen_fops = {
	.owner		= THIS_MODULE,
	.read		= felica_cen_read,
	.write		= felica_cen_write,
	.ioctl		= NULL,
	.open		= felica_cen_open,
	.release	= felica_cen_release,
	.fsync		= NULL,
};

static struct miscdevice felica_cen_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "felica_cen",
	.fops = &felica_cen_fops,
};

/**
 * @brief   Initialize FeliCa CEN controller
 * @details This function executes;\n
 *            # Check CEN platform data\n
 *            # Alloc and Initialize CEN controller's data\n
 *            # Create CEN character device (/dev/felica_cen)
 * @param   pfdata   : Pointer to CEN platform data
 * @retval  0        : Success
 * @retval  Negative : Initialization failed.\n
 *            -EINVAL = No platform data\n
 *            -ENOMEM = No enough memory / Cannot create char dev
 * @note
 */
int felica_cen_probe_func(struct felica_cen_pfdata *pfdata)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check CEN platform data */
	if (!pfdata) {
		pr_err(PRT_NAME ": Error. No platform data for CEN.\n");
		return -EINVAL;
	}

	/* Alloc and Initialize CEN controller's data */
	flcen = kzalloc(sizeof(struct felica_cen_data), GFP_KERNEL);
	if (!flcen) {
		pr_err(PRT_NAME ": Error. No enough mem for CEN.\n");
		return -ENOMEM;
	}
	flcen->pfdata = pfdata;

	/* Create CEN character device (/dev/felica_cen) */
	if (misc_register(&felica_cen_device)) {
		pr_err(PRT_NAME ": Error. Cannot register CEN.\n");
		kfree(flcen);
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief   Terminate FeliCa cen controller
 * @details This function executes;\n
 *            # Deregister CEN character device (/dev/felica_cen)\n
 *            # Release CEN controller's data
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_cen_remove_func(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	misc_deregister(&felica_cen_device);
	kfree(flcen);
}
