/* vendor/semc/hardware/felica/felica_statemachine.h
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

#ifndef _FELICA_STATEMACHINE_H
#define _FELICA_STATEMACHINE_H

#define STATEMACHINE_ERROR	(-100)

struct transition_struct {
	int (*action)(void *data);
	int nextstate;
};

struct event_struct {
	int ev;
	void *evdata;
};

struct result_struct {
	int retval;
	int ev;
	int state;
};

struct result_struct felica_event_handle(struct event_struct);
int felica_request_nextevent(struct event_struct);

int felica_statemachine_init(void);
void felica_statemachine_exit(void);

#endif
