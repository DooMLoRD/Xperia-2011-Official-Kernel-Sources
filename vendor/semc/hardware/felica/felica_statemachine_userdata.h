/* vendor/semc/hardware/felica/felica_statemachine_userdata.h
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

#ifndef _FELICA_STATEMACHINE_USERDATA_H
#define _FELICA_STATEMACHINE_USERDATA_H

#include "felica_statemachine.h"

/* Enumration of State */
enum states {
	uart_closed,
	uart_open_idle,
	uart_open_rx,
	uart_open_tx,
	__end_of_state
};

/* Enumration of Event */
enum events {
	open_syscall,
	close_syscall,
	read_syscall,
	available_syscall,
	write_syscall,
	fsync_syscall,
	exec_close_sig,
	exec_fsync_sig,
	receive_1byte_sig,
	rx_complete_sig,
	rx_to_idle_sig,
	tx_complete_sig,
	fsync_timeout_sig,
	uartdm_error_sig,
	__end_of_event
};

int felica_do_open_seq(void *);
int felica_do_open_again(void *);
int felica_do_close_check(void *);
int felica_do_close_seq(void *);
int felica_do_read_seq(void *);
int felica_do_available_seq(void *);
int felica_do_write_seq(void *);
int felica_do_fsync_check(void *);
int felica_do_fsync_reserve(void *);
int felica_do_fsync_cancel(void *);
int felica_do_rx_start(void *);
int felica_do_rx_finish(void *);
int felica_do_rx_reset(void *);
int felica_do_tx_start(void *);
int felica_do_tx_finish(void *);
int felica_do_tx_reset(void *);
int felica_do_rx_to_idle(void *);
int felica_do_nothing(void *);

static struct transition_struct tran[__end_of_state][__end_of_event] = {
	/* Transitions @ UART CLOSED state*/
	{ {felica_do_open_seq,      uart_open_idle},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed},
	  {felica_do_nothing,       uart_closed} },
	/* Transitions @ UART OPEN IDLE state*/
	{ {felica_do_open_again,    uart_open_idle},
	  {felica_do_close_check,   uart_open_idle},
	  {felica_do_read_seq,      uart_open_idle},
	  {felica_do_available_seq, uart_open_idle},
	  {felica_do_write_seq,     uart_open_idle},
	  {felica_do_fsync_check,   uart_open_idle},
	  {felica_do_close_seq,     uart_closed},
	  {felica_do_tx_start,      uart_open_tx},
	  {felica_do_rx_start,      uart_open_rx},
	  {felica_do_nothing,       uart_open_idle},
	  {felica_do_nothing,       uart_open_idle},
	  {felica_do_nothing,       uart_open_idle},
	  {felica_do_nothing,       uart_open_idle},
	  {felica_do_nothing,       uart_open_idle} },
	/* Transitions @ UART OPEN RX state*/
	{ {felica_do_open_again,    uart_open_rx},
	  {felica_do_close_check,   uart_open_rx},
	  {felica_do_read_seq,      uart_open_rx},
	  {felica_do_available_seq, uart_open_rx},
	  {felica_do_write_seq,     uart_open_rx},
	  {felica_do_fsync_reserve, uart_open_rx},
	  {felica_do_close_seq,     uart_closed},
	  {felica_do_tx_start,      uart_open_tx},
	  {felica_do_rx_start,      uart_open_rx},
	  {felica_do_rx_finish,     uart_open_rx},
	  {felica_do_rx_to_idle,    uart_open_idle},
	  {felica_do_nothing,       uart_open_rx},
	  {felica_do_fsync_cancel,  uart_open_rx},
	  {felica_do_rx_reset,      uart_open_rx} },
	/* Transitions @ UART OPEN TX state*/
	{ {felica_do_open_again,    uart_open_tx},
	  {felica_do_close_check,   uart_open_tx},
	  {felica_do_read_seq,      uart_open_tx},
	  {felica_do_available_seq, uart_open_tx},
	  {felica_do_nothing,       uart_open_tx},
	  {felica_do_nothing,       uart_open_tx},
	  {felica_do_close_seq,     uart_closed},
	  {felica_do_nothing,       uart_open_tx},
	  {felica_do_nothing,       uart_open_tx},
	  {felica_do_nothing,       uart_open_tx},
	  {felica_do_nothing,       uart_open_tx},
	  {felica_do_tx_finish,     uart_open_idle},
	  {felica_do_tx_reset,      uart_open_idle},
	  {felica_do_tx_reset,      uart_open_idle} }
};

#endif
