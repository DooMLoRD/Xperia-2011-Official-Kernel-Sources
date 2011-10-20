/* vendor/semc/hardware/felica/msm_uartdm.h
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

#ifndef _MSM_UARTDM_H
#define _MSM_UARTDM_H

struct msm_uartdm_pfdata;

int msm_uartdm_init(struct msm_uartdm_pfdata *pfdata);
void msm_uartdm_exit(void);
int msm_uartdm_open(void);
int msm_uartdm_close(void);
int msm_uartdm_rx_start(void);
int msm_uartdm_rx_reset(void);
int msm_uartdm_tx_start(void);
int msm_uartdm_tx_reset(void);
int msm_uartdm_rcv_1byte_enable(void);
int msm_uartdm_rcv_1byte_disable(void);
int msm_uartdm_rcv_byte_check(void);

/**************************************/
/*    MSM7x30 UART DM - Constant -    */
/**************************************/
#define	UARTDM_SIZE		0x1000
#define UARTDM_BURST_SIZE	16

/********************************************/
/*    MSM7x30 UART DM - Local constant -    */
/********************************************/
#define LOCAL_UARTDM_BASE_FREQ		(460800 * 16)
#define LOCAL_UARTDM_BAUDRATE_CSR	0xFF		/* 460kbps */
#define LOCAL_UARTDM_DATA_STOP_PARITY	0x34		/* 8N1 */
#define LOCAL_UARTDM_STALE_TIMEOUT	32		/* [char times] */
#define LOCAL_UARTDM_TX_RX_FIFO_SPLIT	0x003C	/* Tx240Byte vs Rx272Byte */
#define LOCAL_UARTDM_TX_FIFO_SIZE	(4 * LOCAL_UARTDM_TX_RX_FIFO_SPLIT)
#define LOCAL_UARTDM_RX_FIFO_SIZE	(512 - LOCAL_UARTDM_TX_FIFO_SIZE)

/*********************************************/
/*    MSM7x30 UART DM - Register offset -    */
/*********************************************/
#define	UART_DM_MR1		0x0000
#define	UART_DM_MR2		0x0004
#define	UART_DM_CSR		0x0008
#define	UART_DM_SR		0x0008
#define	UART_DM_CR		0x0010
#define	UART_DM_MISR		0x0010
#define	UART_DM_IMR		0x0014
#define	UART_DM_ISR		0x0014
#define	UART_DM_IPR		0x0018
#define	UART_DM_TFWR		0x001C
#define	UART_DM_RFWR		0x0020
#define	UART_DM_HCR		0x0024
#define	UART_DM_DMRX		0x0034
#define	UART_DM_IRDA		0x0038
#define	UART_DM_RX_TOTAL_SNAP	0x0038
#define	UART_DM_DMEN		0x003C
#define	UART_DM_NO_CHARS_FOR_TX	0x0040
#define	UART_DM_BADR		0x0044
#define	UART_DM_TESTSL		0x0048
#define	UART_DM_TXFS		0x004c
#define	UART_DM_RXFS		0x0050
#define	UART_DM_MISR_MODE	0x0060
#define	UART_DM_MISR_RESET	0x0064
#define	UART_DM_MISR_EXPORT	0x0068
#define	UART_DM_MISR_VAL	0x006c
#define	UART_DM_TF		0x0070
#define	UART_DM_TF_2		0x0074
#define	UART_DM_TF_3		0x0078
#define	UART_DM_TF_4		0x007c
#define	UART_DM_RF		0x0070
#define	UART_DM_RF_2		0x0074
#define	UART_DM_RF_3		0x0078
#define	UART_DM_RF_4		0x007C
#define	UART_DM_SIM_CFG		0x0080
#define	UART_DM_TEST_WR_ADDR	0x0084
#define	UART_DM_TEST_WR_DATA	0x0088
#define	UART_DM_TEST_RD_ADDR	0x008C
#define	UART_DM_TEST_RD_DATA	0x0090

/*****************************************************/
/*    MSM7x30 UART DM - Register bit definition -    */
/*****************************************************/
#define BIT_32BIT_ALL		0xFFFFFFFF
/*** UART_DM_MR2 (R/W) ***/
#define BIT_RX_ERROR_CHAR_OFF		0x0200
#define BIT_RX_BREAK_ZERO_CHAR_OFF	0x0100
#define BIT_ERROR_MODE			0x0040
#define BIT_BITS_PER_CHAR		0x0030
#define BIT_STOP_BIT_LEN		0x000C
#define BIT_PARITY_MODE			0x0003
/*** UART_DM_CSR (W) ***/
#define BIT_UART_RX_CLK_SEL	0x00F0
#define BIT_UART_TX_CLK_SEL	0x000F
/*** UART_DM_SR (R) ***/
#define BIT_RX_BREAK_START_LAST	0x0100
#define BIT_HUNT_CHAR		0x0080
#define BIT_RX_BREAK		0x0040
#define BIT_PAR_FRAME_ERR	0x0020
#define BIT_UART_OVERRUN	0x0010
#define BIT_TXEMT		0x0008
#define BIT_TXRDY		0x0004
#define BIT_RXFULL		0x0002
#define BIT_RXRDY		0x0001
/*** UART_DM_CR (W) ***/
#define BIT_CHANNEL_COMMAND_MSB	0x0800
#define BIT_GENERAL_COMMAND	0x0700
#define BIT_CHANNEL_COMMAND_LSB	0x00F0
#define BIT_UART_TX_DISABLE	0x0008
#define BIT_UART_TX_EN		0x0004
#define BIT_UART_RX_DISABLE	0x0002
#define BIT_UART_RX_EN		0x0001
/*** UART_DM_MISR (R) ***/
#define BIT_UART_MISR
/*** UART_DM_IMR (W) & UART_DM_ISR (R) ***/
#define BIT_PAR_FRAME_ERR_IRQ	0x1000
#define BIT_RXBREAK_END		0x0800
#define BIT_RXBREAK_START	0x0400
#define BIT_TX_DONE		0x0200
#define BIT_TX_ERROR		0x0100
#define BIT_TX_READY		0x0080
#define BIT_CURRENT_CTS		0x0040
#define BIT_DELTA_CTS		0x0020
#define BIT_RXLEV		0x0010
#define BIT_RXSTALE		0x0008
#define BIT_RXBREAK_CHANGE	0x0004
#define BIT_RXHUNT		0x0002
#define BIT_TXLEV		0x0001
/*** UART_DM_IPR (R/W) ***/
#define BIT_STALE_TIMEOUT_MSB	0xFFFFFF80
#define BIT_SAMPLE_DATA		0x0040
#define BIT_STALE_TIMEOUT_LSB	0x001F
/*** UART_DM_TFWR	(R/W) ***/
#define BIT_TFW			0xFFFFFFFF
/*** UART_DM_RFWR	(R/W) ***/
#define BIT_RFW			0xFFFFFFFF
/*** UART_DM_DMRX (W/R) ***/
#define BIT_RX_DM_CRCI_CHARS	0x01FFFFFF
/*** UART_DM_RX_TOTAL_SNAP (R) ***/
#define BIT_RX_TOTAL_BYTES	0x00FFFFFF
/*** UART_DM_DMEN (R/W) ***/
#define BIT_RX_DM_EN		0x0002
#define BIT_TX_DM_EN		0x0001
/*** UART_DM_NO_CHARS_FOR_TX (R/W) ***/
#define BIT_TX_TOTAL_TRANS_LEN	0x00FFFFFF
/*** UART_DM_BADR (R/W) ***/
#define BIT_RX_BASE_ADDR	0xFFFFFFFC

/*********************************************************/
/*    MSM7x30 UART DM - Command register definition -    */
/*********************************************************/
/*** Channel Command  (Bit11)---(Bit7)(Bit6)(Bit5)(Bit4) ***/
#define CCMD_NULL_COMMAND			0x00
#define CCMD_RESET_RECEIVER			0x10
#define CCMD_RESET_TRANSMITTER			0x20
#define CCMD_RESET_ERROR_STATUS			0x30
#define CCMD_BREAK_CHANGE_INTERRUPT		0x40
#define CCMD_START_BREAK			0x50
#define CCMD_STOP_BREAK				0x60
#define CCMD_RESET_CTS_N			0x70
#define CCMD_RESET_STALE_INTERRUPT		0x80
#define CCMD_PACKET_MODE			0x90
#define CCMD_MODE_RESET				0xC0
#define CCMD_SET_RFR_N				0xD0
#define CCMD_RESET_RFR_N			0xE0
#define CCMD_RESET_TX_ERROR			0x800
#define CCMD_CLEAR_TX_DONE			0x810
#define CCMD_RESET_BREAK_START_INTERRUPT	0x820
#define CCMD_BREAK_END_INTERRUPT		0x830
#define CCMD_RESET_PER_FRAME_ERR_INTERRUPT	0x840

/*** General Command  (Bit10)(Bit9)(Bit8) ***/
#define GCMD_NULL_COMMAND			0x000
#define GCMD_CR_PROTECTION_ENABLE		0x100
#define GCMD_CR_PROTECTION_DISABLE		0x200
#define GCMD_RESET_TX_READY_INTERRUPT		0x300
#define GCMD_ENABLE_STALE_EVENT			0x500
#define GCMD_DISABLE_STALE_EVENT		0x600
#define GCMD_SW_FORCE_STALE			0x400

#endif
