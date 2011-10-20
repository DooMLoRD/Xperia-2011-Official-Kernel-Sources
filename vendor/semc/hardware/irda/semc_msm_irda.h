/* drivers/serial/semc_irda/semc_msm_irda.h
 *
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Manabu Yoshida <Manabu.X.Yoshida@sonyericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef _SEMC_MSM_IRDA_H_
#define _SEMC_MSM_IRDA_H_

/* IrDA UART SW buffer settings */
#define IRUART_SW_TXBUF_SIZE UART_XMIT_SIZE
#define IRUART_SW_RXBUF_SIZE 512
#define IRUART_SW_RXRINGBUF_SIZE (IRUART_SW_TXBUF_SIZE*2)

/* SEMC MSM IrDA driver - Platform data */
struct irda_platform_data {
	int gpio_pow;
	struct pm8058_gpio *gpio_pwcfg_low;
	struct pm8058_gpio *gpio_pwcfg_hig;

	u32 paddr_uartdm;
	int irq_uartdm;
	int chan_uartdm_tx;
	int crci_uartdm_tx;
	int chan_uartdm_rx;
	int crci_uartdm_rx;
	char clk_str[12];
	struct device *clk_dev;
	int (*gpio_init)(void);
};

struct irtxbuf_all {
	int ptr;
	int num;
	u8 *buf;
	dma_addr_t dmabase;
};

struct irrxbuf_all {
	/* read buffer */
	int wp;
	int rp;
	u8 *ringbuf;
	/* dma buffer */
	u8 *buf;
	dma_addr_t dmabase;
};

#endif
