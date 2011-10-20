/* vendor/semc/hardware/felica/felica_statemachine.c
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

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/semaphore.h>
#include "felica_statemachine.h"
#include "felica_statemachine_userdata.h"

#define PRT_NAME "flc_statemachine"
#define BEFORE_INIT  (-1)
#define INIT_STATE   (0)
#define NO_EVENT_REQ (-1)

static int state = BEFORE_INIT;
static struct event_struct next_evset;
static struct semaphore sem_transition;

/**
 * @brief  Request next event during action
 * @param  evset : Requested event and its data
 * @retval 0 : Success.
 * @note
 */
int felica_request_nextevent(struct event_struct evset)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	next_evset = evset;

	return 0;
}

/**
 * @brief   Core of State-machine event handler
 * @details This function executes;\n
 *          # Check the event number\n
 *          # Check the state number\n
 *          # Init request of next event\n
 *          # Dispatch event according to state\n
 *          # Execute the corresponding function\n
 *          # [If action succeeded,] go to the next state\n
 *          # [If action succeeded and the next event is requested,]
 *          # call this event handler recursively.\n
 *          # Return result
 * @param   evset : Requested event and its data
 * @retval  .retval : Return value of the last action\n
 *            0/Positive : Action succeeded.\n
 *            Negative   : Action failed.\n
 *              STATEMACHINE_ERROR(<0) = Statemachine handling error\n
 *              Others = Failure during the action is executed.
 * @retval  .ev : Event during the last action
 * @retval  .state : State during the last action
 * @note
 */
static struct result_struct __felica_event_handle(struct event_struct evset)
{
	struct result_struct result;
	struct transition_struct *t;

	pr_debug(PRT_NAME ": %s (ev#%d/st#%d)\n", __func__, evset.ev, state);

	result.ev = evset.ev;
	result.state = state;

	/* Check the event number */
	if ((0 > evset.ev) || (__end_of_event <= evset.ev)) {
		pr_err(PRT_NAME ": Error. Invalid Event number.\n");
		result.retval = STATEMACHINE_ERROR;
		return result;
	}

	/* Check the state number */
	if ((0 > state) || (__end_of_state <= state)) {
		pr_err(PRT_NAME ": Error. Invalid State number.\n");
		result.retval = STATEMACHINE_ERROR;
		return result;
	}

	/* Init next event */
	next_evset.ev = NO_EVENT_REQ;
	next_evset.evdata = NULL;

	/* Dispatch event-state */
	t = &tran[state][evset.ev];
	if ((NULL == t->action) ||
		(0 > t->nextstate) || (__end_of_state <= t->nextstate)) {
		pr_err(PRT_NAME ": Error. Invalid transition defined.\n");
		result.retval = STATEMACHINE_ERROR;
		return result;
	}

	/* Execute action */
	result.retval = (t->action)(evset.evdata);

	if (0 > result.retval)
		return result;

	/* Change state */
	pr_debug(PRT_NAME ": State change #%d -> #%d\n",
						state, t->nextstate);
	state = t->nextstate;

	/* Execute next event recursively, if exists. */
	if (NO_EVENT_REQ != next_evset.ev)
		result = __felica_event_handle(next_evset);

	return result;
}

/**
 * @brief   State-machine event handler
 * @details This function executes;\n
 *          # Down semaphore\n
 *          # Call __felica_event_handle\n
 *          # Up semaphore
 *          # Return result
 * @param   evset : Requested event and its data
 * @retval  .retval : Return value of the last action\n
 * @retval  .ev : Event of the last action
 * @retval  .state : State of the last action
 * @note
 */
struct result_struct felica_event_handle(struct event_struct evset)
{
	struct result_struct result;

	pr_debug(PRT_NAME ": %s (ev#%d)\n", __func__, evset.ev);

	down(&sem_transition);

	result = __felica_event_handle(evset);

	up(&sem_transition);

	return result;
}

/**
 * @brief   Initialization of statemachine
 * @details This function executes;\n
 *          # Init the semaphore\n
 *          # Set the state to Default
 * @param   N/A
 * @retval  0 : Success
 * @note
 */
int felica_statemachine_init(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	/* Init semaphore */
	init_MUTEX(&sem_transition);

	/* Start from the default state */
	state = INIT_STATE;

	return 0;
}

/**
 * @brief   Exit function of statemachine
 * @details This function executes;\n
 *          # Set the state as Statemachine is not ready
 * @param  N/A
 * @retval N/A
 * @note
 */
void felica_statemachine_exit(void)
{
	pr_debug(PRT_NAME ": %s\n", __func__);

	state = BEFORE_INIT;
}
