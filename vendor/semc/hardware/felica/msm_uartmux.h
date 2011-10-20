/* vendor/semc/hardware/felica/msm_uartmux.h
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

#ifndef _MSM_UARTMUX_H
#define _MSM_UARTMUX_H

#define	SSBI_REG_ADDR_MISC	0x1CC

#include <linux/mutex.h>

int msm_uartmux_get(void);
int msm_uartmux_set(int);

int msm_uartmux_init(void);
void msm_uartmux_exit(void);

struct pm8058_nfc_device {
	struct mutex		nfc_mutex;
	struct pm8058_chip	*pm_chip;
};

#endif
