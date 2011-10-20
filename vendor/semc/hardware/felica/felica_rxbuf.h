/* vendor/semc/hardware/felica/felica_rxbuf.h
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

#ifndef _FELICA_RXBUF_H
#define _FELICA_RXBUF_H

#define RXBUF_M	272
#define RXBUF_N	128

#define DMAPOOL_NAME "felica_rxbuf"
#define DMAPOOL_SIZE  RXBUF_M
/* DMAPOOL_ALIGN must be ">=DMAPOOL_SIZE" and pow of 2 */
#define DMAPOOL_ALIGN 512

int felica_rxbuf_init(void);
void felica_rxbuf_exit(void);
void felica_rxbuf_clear(void);
int felica_rxbuf_count(void);
int felica_rxbuf_gets(char __user *, int);
dma_addr_t felica_rxbuf_push_pointer(void);
int felica_rxbuf_push_length(void);
int felica_rxbuf_push_status_update(int);

#endif
