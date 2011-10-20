/* vendor/semc/hardware/felica/felica_int.c
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
#include <linux/mfd/pmic8058.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include "semc_felica_ext.h"
#include "felica_int.h"

#define PRT_NAME "felica int"
#define INT_LOW  0x0
#define INT_HIGH 0x1
#define DEFAULT_INT_STATE INT_HIGH

struct felica_int_data {
	struct felica_int_pfdata	*pfdata;
	struct switch_dev		swdev;
	struct work_struct		work_int;
	spinlock_t			lock_int;
};

static struct felica_int_data *flint;

/**
 * @brief   Interrupt handler of FeliCa INT controller
 * @details This function executes;\n
 *            # Disable INT interrupt\n
 *            # Schedule work for FeliCa push (keventd_wq)
 * @param   irq         : (unused)
 * @param   dev         : (unused)
 * @retval  IRQ_HANDLED : Success
 * @note
 */
static irqreturn_t felica_int_irq_handler(int irq, void *dev)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Disable INT interrupt */
	disable_irq_nosync(flint->pfdata->irq_int);
	/* Schedule work for FeliCa push (keventd_wq) */
	schedule_work(&flint->work_int);

	return IRQ_HANDLED;
}

/**
 * @brief   Interrupt work description of FeliCa INT controller
 * @details This function executes;\n
 *            # Read INT GPIO\n
 *            # Update value of the switch device\n
 *            # Enable INT interrupt\n
 * @param   work : (unused)
 * @retval  N/A
 * @note
 */
static void felica_int_exec(struct work_struct *work)
{
	int state;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Read INT GPIO */
	state = gpio_get_value_cansleep(flint->pfdata->gpio_int);
	if (INT_LOW == state || INT_HIGH == state) {
		/* Update value of the switch device */
		switch_set_state(&flint->swdev, state);
		pr_debug(PRT_NAME ":  INT state = %d\n", state);
		/* Enable INT interrupt */
		enable_irq(flint->pfdata->irq_int);
	} else
		pr_err(PRT_NAME ": Error. Cannot read INT GPIO.\n");
}

/**
 * @brief   Initialize FeliCa INT controller
 * @details This function executes;\n
 *            # Check INT platform data\n
 *            # Alloc and init INT controller's data\n
 *            # Init work of FeliCa push\n
 *            # Request INT GPIO\n
 *            # Request IRQ for INT GPIO\n
 *            # Enable IRQ wake for INT GPIO\n
 *            # Create INT switch device (felica_push)\n
 *            # Set default state of the device
 * @param   pfdata   : Pointer to INT platform data
 * @retval  0        : Success
 * @retval  Negative : Failure\n
 *            -EINVAL = No platform data\n
 *            -ENOMEM = No enough memory / Cannot create switch dev\n
 *            -ENODEV = GPIO request failed\n
 *            -EIO    = IRQ request failed / Enabling IRQ wake failed
 */
int felica_int_probe_func(struct felica_int_pfdata *pfdata)
{
	int ret;

	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Check INT platform data */
	if (!pfdata) {
		pr_err(PRT_NAME ": Error. No platform data for INT.\n");
		return -EINVAL;
	}

	/* Alloc and init INT controller's data */
	flint = kzalloc(sizeof(struct felica_int_data), GFP_KERNEL);
	if (!flint) {
		pr_err(PRT_NAME ": Error. No enough mem for INT.\n");
		ret = -ENOMEM;
		goto err_alloc_int_data;
	}
	flint->pfdata = pfdata;

	/* Init work of FeliCa push */
	INIT_WORK(&flint->work_int, felica_int_exec);

	/* Request INT GPIO */
	ret = gpio_request(flint->pfdata->gpio_int, "felica_int");
	if (ret) {
		pr_err(PRT_NAME ": Error. INT GPIO request failed.\n");
		ret = -ENODEV;
		goto err_request_int_gpio;
	}
	ret = gpio_direction_input(flint->pfdata->gpio_int);
	if (ret) {
		pr_err(PRT_NAME ": Error. INT GPIO direction failed.\n");
		ret = -ENODEV;
		goto err_direction_int_gpio;
	}

	/* Request IRQ for INT GPIO */
	ret = request_irq(flint->pfdata->irq_int, felica_int_irq_handler,
	 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "felica_irq", NULL);
	if (ret) {
		pr_err(PRT_NAME ": Error. Request IRQ failed.\n");
		ret = -EIO;
		goto err_request_int_irq;
	}

	/* Enable IRQ wake */
	ret = enable_irq_wake(flint->pfdata->irq_int);
	if (ret) {
		pr_err(PRT_NAME ": Error. Enabling IRQ wake failed.\n");
		ret = -EIO;
		goto err_enable_irq_wake;
	}

	/* Create INT switch device (felica_push) */
	flint->swdev.name = "felica_push";
	if (switch_dev_register(&flint->swdev)) {
		pr_err(PRT_NAME ": Error. Cannot create switch dev\n");
		ret = -ENOMEM;
		goto err_create_switch_dev;
	}

	/* Set dafault state of the device */
	switch_set_state(&flint->swdev, DEFAULT_INT_STATE);

	return 0;

err_create_switch_dev:
	disable_irq_wake(flint->pfdata->irq_int);
err_enable_irq_wake:
	free_irq(flint->pfdata->irq_int, NULL);
err_request_int_irq:
err_direction_int_gpio:
	gpio_free(flint->pfdata->gpio_int);
err_request_int_gpio:
	kfree(flint);
err_alloc_int_data:
	return ret;
}

/**
 * @brief   Terminate FeliCa INT controller
 * @details This function executes;\n
 *            # Unregister switch device (felica_push)\n
 *            # Release IRQ for INT GPIO\n
 *            # Release INT GPIO resource\n
 *            # Release INT controller's data
 * @param   N/A
 * @retval  N/A
 * @note
 */
void felica_int_remove_func(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	switch_dev_unregister(&flint->swdev);
	disable_irq_wake(flint->pfdata->irq_int);
	free_irq(flint->pfdata->irq_int, NULL);
	gpio_free(flint->pfdata->gpio_int);
	kfree(flint);
}
