/* vendor/semc/hardware/felica/felica_master.c
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
#include <linux/platform_device.h>
#include "semc_felica_ext.h"
#include "felica_uart.h"
#include "felica_cen.h"
#include "felica_pon.h"
#include "felica_rfs.h"
#include "felica_int.h"
#include "felica_rws.h"

#define DRV_NAME "felica driver"
#define DRV_VERSION "0.2"
#define PRT_NAME "felica master"

/**
 * @brief   Probe function of FeliCa driver
 * @details This function executes;\n
 *            # Load platform data of FeliCa driver\n
 *            # GPIO setting of MSM & PM\n
 *            # Call initialization functions of each controller
 * @param   pdev     : Pointer to data of FeliCa platform device
 * @retval  0        : Success
 * @retval  Negative : Failure
 * @note
 */
static int felica_probe(struct platform_device *pdev)
{
	int	ret;
	struct felica_platform_data	*flc_pfdata;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Load platform data of FeliCa driver */
	flc_pfdata = pdev->dev.platform_data;
	if (NULL == flc_pfdata) {
		pr_err(PRT_NAME ": Error. No platform data.\n");
		ret = -EINVAL;
		goto err_get_platform_data;
	}

	/* GPIO setting of MSM & PM */
	if (flc_pfdata->gpio_init) {
		ret = flc_pfdata->gpio_init();
		if (ret && -EBUSY != ret) {
			pr_err(PRT_NAME ": Error. GPIO init failed.\n");
			goto error_gpio_init;
		}
	}

	/* Call initialization functions of each controller */
	ret = felica_uart_probe_func(&flc_pfdata->uart_pfdata);
	if (ret) {
		pr_err(PRT_NAME ": Error. UART probe failure.\n");
		goto err_uart_probe;
	}

	ret = felica_cen_probe_func(&flc_pfdata->cen_pfdata);
	if (ret) {
		pr_err(PRT_NAME ": Error. CEN probe failure.\n");
		goto err_cen_probe;
	}

	ret = felica_pon_probe_func(&flc_pfdata->pon_pfdata);
	if (ret) {
		pr_err(PRT_NAME ": Error. PON probe failure.\n");
		goto err_pon_probe;
	}

	ret = felica_rfs_probe_func(&flc_pfdata->rfs_pfdata);
	if (ret) {
		pr_err(PRT_NAME ": Error. RFS probe failure.\n");
		goto err_rfs_probe;
	}

	ret = felica_int_probe_func(&flc_pfdata->int_pfdata);
	if (ret) {
		pr_err(PRT_NAME ": Error. INT probe failure.\n");
		goto err_int_probe;
	}

	ret = felica_rws_probe_func();
	if (ret) {
		pr_err(PRT_NAME ": Error. RWS probe failure.\n");
		goto err_rws_probe;
	}

	return 0;

/* Error handling */
err_rws_probe:
	felica_int_remove_func();
err_int_probe:
	felica_rfs_remove_func();
err_rfs_probe:
	felica_pon_remove_func();
err_pon_probe:
	felica_cen_remove_func();
err_cen_probe:
	felica_uart_remove_func();
err_uart_probe:
error_gpio_init:
err_get_platform_data:
	return ret;
}

/**
 * @brief   Remove function of FeliCa driver
 * @details This function executes;\n
 *            # Call termination functions of each controller
 * @param   pdev : (unused)
 * @retval  0    : Success
 * @note
 */
static int felica_remove(struct platform_device *pdev)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	felica_rws_remove_func();
	felica_int_remove_func();
	felica_rfs_remove_func();
	felica_pon_remove_func();
	felica_cen_remove_func();
	felica_uart_remove_func();

	return 0;
}

/**
 * @brief  Suspend function of FeliCa driver
 * @param  pdev : (unused)
 * @param  message
 * @retval 0
 * @note
 */
static int felica_suspend(struct platform_device *pdev,
					pm_message_t message)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief  Resume function of FeliCa driver
 * @param  pdev : (unused)
 * @retval 0
 * @note
 */
static int felica_resume(struct platform_device *pdev)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	return 0;
}

/**
 * @brief Platform driver data structure of FeliCa driver
 */
static struct platform_driver semc_felica_driver = {
	.probe		= felica_probe,
	.remove		= felica_remove,
	.suspend	= felica_suspend,
	.resume		= felica_resume,
	.driver		= {
		.name		= "semc_felica",
		.owner		= THIS_MODULE,
	},
};

/**
 * @brief   Init function of FeliCa driver
 * @details This function executes;\n
 *            # platform driver registration of FeliCa driver
 * @param   N/A
 * @retval  0        : Success
 * @retval  Negative : Failure
 * @note
 */
static int __init felica_init(void)
{
	int ret;

	pr_info(PRT_NAME ": FeliCa driver ver %s being loaded.\n",
							DRV_VERSION);

	ret = platform_driver_register(&semc_felica_driver);
	return ret;
}

/**
 * @brief   Exit function of FeliCa driver
 * @details This function executes;\n
 *            # platform driver unregistration of FeliCa driver
 * @param   N/A
 * @retval  N/A
 * @note
 */
static void __exit felica_exit(void)
{
	pr_info(PRT_NAME ": %s\n", __func__);

	platform_driver_unregister(&semc_felica_driver);
}

module_init(felica_init);
module_exit(felica_exit);

MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("SEMC");
MODULE_DESCRIPTION("FeliCa driver");
MODULE_LICENSE("GPL");
