/* vendor/semc/hardware/felica/felica_uart.h
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

#ifndef _FELICA_UART_H
#define _FELICA_UART_H

#include <linux/param.h>
#include <asm/ioctls.h>

#define FELICA_UART_FSYNC_TIMEOUT	(3 * HZ)
#define FELICA_UART_READ_MSLEEP		1
#define FELICA_UART_AVAILABLE_MSLEEP	1

#define IOCTL_UART_AVAILABLE	FIONREAD

struct felica_uart_pfdata;

struct read_data_struct {
	char __user *buf;
	size_t count;
};

struct write_data_struct {
	const char __user *buf;
	size_t count;
};

int felica_uart_probe_func(struct felica_uart_pfdata *);
void felica_uart_remove_func(void);

#endif
