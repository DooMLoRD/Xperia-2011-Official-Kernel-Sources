/* vendor/semc/hardware/felica/msm_uartmux.c
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
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-nfc.h>
#include "msm_uartmux.h"

#define PRT_NAME "msm uartmux"

/**
 * @brief  Get pmic device information
 * @param  N/A
 * @retval Valid address : Pointer to PMIC device
 * @retval NULL : Cannot find PM8058 device
 */
static struct pm8058_chip *msm_uartmux_pmic_info(void)
{
	struct pm8058_nfc_device *nfcdev;

	pr_debug(PRT_NAME ": %s\n", __func__);

	nfcdev = pm8058_nfc_request();
	if (!nfcdev)
		return NULL;
	if (!nfcdev->pm_chip)
		return NULL;

	return nfcdev->pm_chip;
}

/**
 * @brief  Get uartmux state
 * @param  N/A
 * @retval 0/Positive : Success, value of UART MUX.
 * @retval Negative : Failure\n
 *           -ENODEV = Cannot find PM8058 device\n
 *           -EIO    = Cannot read value from PM8058
 */
int msm_uartmux_get(void)
{
	static struct pm8058_chip *pmdev;
	int ret;
	char misc = 0x00;

	pr_debug(PRT_NAME ": %s\n", __func__);

	pmdev = msm_uartmux_pmic_info();
	if (!pmdev) {
		pr_err(PRT_NAME ": Error. Dev not found.\n");
		return -ENODEV;
	}

	ret = pm8058_read(pmdev, SSBI_REG_ADDR_MISC, &misc, 1);
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot read UART MUX.\n");
		return -EIO;
	}

	misc &= PM8058_UART_MUX_MASK;	/* Needed to delete unused bits */
	pr_debug(PRT_NAME ": UART MUX = %d.\n", (int) misc);

	return (int) misc;
}

/**
 * @brief  Set uartmux state
 * @param  value : Value to write.
 * @retval 0 : Success
 * @retval Negative : Failure\n
 *           -ENODEV = Cannot find PM8058 device\n
 *           -EIO    = Cannot write value to PM8058
 */
int msm_uartmux_set(int value)
{
	struct pm8058_chip *pmdev;
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	pmdev = msm_uartmux_pmic_info();
	if (!pmdev) {
		pr_err(PRT_NAME ": Error. Dev not found.\n");
		return -ENODEV;
	}

	ret = pm8058_misc_control(pmdev, PM8058_UART_MUX_MASK, value);
	if (ret) {
		pr_err(PRT_NAME ": Error. Cannot write UART MUX.");
		return -EIO;
	}

	return 0;
}

/**
 * @brief   Init funtion for MSM UARTMUX
 * @param   N/A
 * @retval  0 : Success
 * @note    No initialization is needed for UART MUX.
 */
int msm_uartmux_init(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief   Exit function for MSM UARTMUX
 * @param   N/A
 * @retval  N/A
 * @note
 */
void msm_uartmux_exit(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);
}
