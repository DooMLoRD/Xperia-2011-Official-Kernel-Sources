/* vendor/semc/hardware/felica/felica_txbuf.h
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

#ifndef _FELICA_TXBUF_H
#define _FELICA_TXBUF_H

#define TXBUF_SIZE	260

int felica_txbuf_init(void);
void felica_txbuf_exit(void);
int felica_txbuf_count(void);
void felica_txbuf_clear(void);
int felica_txbuf_puts(const char __user *, int);
dma_addr_t felica_txbuf_pull_pointer(void);
int felica_txbuf_pull_status_update(int);

#endif
