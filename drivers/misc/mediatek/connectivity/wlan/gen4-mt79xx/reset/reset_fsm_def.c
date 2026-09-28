// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   reset_fsm_def.c
*   \brief  reset fsm def
*
*    This file contains all implementations of reset module
*/


/**********************************************************************
*                         C O M P I L E R   F L A G S
***********************************************************************
*/


/**********************************************************************
*                    E X T E R N A L   R E F E R E N C E S
***********************************************************************
*/
#include "reset.h"

/**********************************************************************
*                                 M A C R O S
***********************************************************************
*/
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

/**********************************************************************
*                              C O N S T A N T S
***********************************************************************
*/


/**********************************************************************
*                             D A T A   T Y P E S
***********************************************************************
*/


/**********************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
***********************************************************************
*/
static void enter_POWER_UP(struct FsmEntity *fsm, struct FsmState *prev);
static void leave_POWER_UP(struct FsmEntity *fsm, struct FsmState *next);
static void enter_PROBED(struct FsmEntity *fsm, struct FsmState *prev);
static void enter_PRE_RESET(struct FsmEntity *fsm, struct FsmState *prev);
static void leave_PRE_RESET(struct FsmEntity *fsm, struct FsmState *next);
static void enter_PRE_POWER_OFF(struct FsmEntity *fsm, struct FsmState *prev);
static void leave_PRE_POWER_OFF(struct FsmEntity *fsm, struct FsmState *next);
static void enter_POWER_OFF(struct FsmEntity *fsm, struct FsmState *prev);
static void leave_POWER_OFF(struct FsmEntity *fsm, struct FsmState *next);

static bool gaurd_isAllModuleReady(struct FsmEntity *fsm);
static bool guard_isResetWorkIdle(struct FsmEntity *fsm);
static bool guard_isPendingPowerOff(struct FsmEntity *fsm);

static void action_RestartWaitprobeTimer(struct FsmEntity *fsm);
#if (RESETKO_WAIT_MODULE_PROBE_TIMEOUT == 0)
static void action_WaitProbeTimeout(struct FsmEntity *fsm);
#endif
static void action_PowerReset(struct FsmEntity *fsm);
static void action_PendingPowerOff(struct FsmEntity *fsm);
static void action_ClearPendingPowerOff(struct FsmEntity *fsm);
static void action_NotifyFunctionOff(struct FsmEntity *fsm);

/**********************************************************************
*                            P U B L I C   D A T A
***********************************************************************
*/


/**********************************************************************
*                           P R I V A T E   D A T A
***********************************************************************
*/
bool fgResetWorkIdle = true;

static struct FsmState RFSM_POWER_UP;
static struct FsmState RFSM_PROBED;
static struct FsmState RFSM_PRE_RESET;
static struct FsmState RFSM_PRE_POWER_OFF;
static struct FsmState RFSM_POWER_OFF;

static struct ResetFsmEventAction RFSM_EVENT_ACTION_LIST_POWER_UP[] = {
	{RFSM_EVENT_PROBED, NULL, NULL, &RFSM_PROBED},
	{RFSM_EVENT_TRIGGER_RESET, NULL, NULL, &RFSM_PRE_RESET},
	{RFSM_EVENT_TRIGGER_POWER_OFF, NULL, NULL, &RFSM_PRE_POWER_OFF},
	{RFSM_EVENT_START_PROBE, NULL, action_RestartWaitprobeTimer, NULL},
#if RESETKO_WAIT_MODULE_PROBE_TIMEOUT
	/* wait probe timeout, trigger reset */
	{RFSM_EVENT_TIMEOUT, NULL, NULL, &RFSM_PRE_RESET}
#else
	/* if timeout, just release wakeupResource */
	{RFSM_EVENT_TIMEOUT, NULL, action_WaitProbeTimeout, NULL}
#endif
};

static struct ResetFsmEventAction RFSM_EVENT_ACTION_LIST_PROBED[] = {
	{RFSM_EVENT_REMOVED, NULL, NULL, &RFSM_POWER_UP},
	{RFSM_EVENT_TRIGGER_RESET, NULL, NULL, &RFSM_PRE_RESET},
	{RFSM_EVENT_TRIGGER_POWER_OFF, NULL, NULL, &RFSM_PRE_POWER_OFF}
};

static struct ResetFsmEventAction RFSM_EVENT_ACTION_LIST_PRE_RESET[] = {
	{RFSM_EVENT_READY, gaurd_isAllModuleReady, action_PowerReset, NULL},
#if RESETKO_WAIT_MODULE_READY_TIMEOUT
	/* timeout force reset */
	{RFSM_EVENT_TIMEOUT, guard_isResetWorkIdle, action_PowerReset, NULL},
#endif
	/* if trigger power off before reset, ignore reset and run power off */
	{RFSM_EVENT_TRIGGER_POWER_OFF, guard_isResetWorkIdle, NULL,
		&RFSM_PRE_POWER_OFF},
	/* if reset action is running, just set pending power off flag */
	{RFSM_EVENT_TRIGGER_POWER_OFF, NULL, action_PendingPowerOff, NULL},
	{RFSM_EVENT_TRIGGER_POWER_ON, NULL, action_ClearPendingPowerOff, NULL},
	{RFSM_EVENT_RESET_DONE, guard_isPendingPowerOff, NULL,
		&RFSM_PRE_POWER_OFF},
	{RFSM_EVENT_RESET_DONE, NULL, NULL, &RFSM_POWER_UP},
	{RFSM_EVENT_RESET_OFF_DONE, guard_isPendingPowerOff, NULL,
		&RFSM_PRE_POWER_OFF},
	{RFSM_EVENT_RESET_OFF_DONE, NULL, NULL, &RFSM_POWER_UP}
};

static struct ResetFsmEventAction RFSM_EVENT_ACTION_LIST_PRE_POWER_OFF[] = {
	{RFSM_EVENT_READY, NULL, NULL, &RFSM_POWER_OFF},
#if RESETKO_WAIT_MODULE_READY_TIMEOUT
	/* timeout force power off */
	{RFSM_EVENT_TIMEOUT, NULL, NULL, &RFSM_POWER_OFF},
#endif
	{RFSM_EVENT_PROBED, NULL, action_NotifyFunctionOff, NULL},
	{RFSM_EVENT_TRIGGER_POWER_ON, NULL, NULL, &RFSM_POWER_UP},
	{RFSM_EVENT_RESET_DONE, NULL, NULL, &RFSM_POWER_OFF}
};

static struct ResetFsmEventAction RFSM_EVENT_ACTION_LIST_POWER_OFF[] = {
	{RFSM_EVENT_TRIGGER_POWER_ON, NULL, NULL, &RFSM_POWER_UP},
	{RFSM_EVENT_START_PROBE, NULL, NULL, &RFSM_PRE_POWER_OFF},
	{RFSM_EVENT_PROBED, NULL, NULL, &RFSM_PRE_POWER_OFF},
};

static struct FsmState RFSM_POWER_UP = {
	.name = "POWER_UP",
	.enter_func = enter_POWER_UP,
	.leave_func = leave_POWER_UP,
	.eventActionListCount = ARRAY_SIZE(RFSM_EVENT_ACTION_LIST_POWER_UP),
	.eventActionList = RFSM_EVENT_ACTION_LIST_POWER_UP
};
static struct FsmState RFSM_PROBED = {
	.name = "PROBED",
	.enter_func = enter_PROBED,
	.leave_func = NULL,
	.eventActionListCount = ARRAY_SIZE(RFSM_EVENT_ACTION_LIST_PROBED),
	.eventActionList = RFSM_EVENT_ACTION_LIST_PROBED
};
static struct FsmState RFSM_PRE_RESET = {
	.name = "PRE_RESET",
	.enter_func = enter_PRE_RESET,
	.leave_func = leave_PRE_RESET,
	.eventActionListCount =
		ARRAY_SIZE(RFSM_EVENT_ACTION_LIST_PRE_RESET),
	.eventActionList = RFSM_EVENT_ACTION_LIST_PRE_RESET
};
static struct FsmState RFSM_PRE_POWER_OFF = {
	.name = "PRE_POWER_OFF",
	.enter_func = enter_PRE_POWER_OFF,
	.leave_func = leave_PRE_POWER_OFF,
	.eventActionListCount =
		ARRAY_SIZE(RFSM_EVENT_ACTION_LIST_PRE_POWER_OFF),
	.eventActionList = RFSM_EVENT_ACTION_LIST_PRE_POWER_OFF
};
static struct FsmState RFSM_POWER_OFF = {
	.name = "POWER_OFF",
	.enter_func = enter_POWER_OFF,
	.leave_func = leave_POWER_OFF,
	.eventActionListCount =
		ARRAY_SIZE(RFSM_EVENT_ACTION_LIST_POWER_OFF),
	.eventActionList = RFSM_EVENT_ACTION_LIST_POWER_OFF
};


/**********************************************************************
*                              F U N C T I O N S
**********************************************************************/
static void enter_POWER_UP(struct FsmEntity *fsm, struct FsmState *prev)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	resetkoStartTimer(fsm, WAIT_PROBE_TIMEOUT);
}

static void leave_POWER_UP(struct FsmEntity *fsm, struct FsmState *next)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	resetkoCancleTimer(fsm);
}

static void enter_PROBED(struct FsmEntity *fsm, struct FsmState *prev)
{
	if (fsm->wakeupCount > 0)
		wakeupSourceRelax(fsm);
}

static void enter_PRE_RESET(struct FsmEntity *fsm, struct FsmState *prev)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	if (prev != &RFSM_PRE_POWER_OFF) {
		fsm->fgReady = false;
		resetkoStartTimer(fsm, WAIT_ALL_MODULE_READY_TIMEOUT);
		wakeupSourceStayAwake(fsm);
		resetkoNotifyEvent(fsm, MODULE_NOTIFY_PRE_POWER_OFF);
	}
}

static void leave_PRE_RESET(struct FsmEntity *fsm, struct FsmState *next)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	if (next != &RFSM_PRE_POWER_OFF) {
		resetkoCancleTimer(fsm);
		fsm->fgReady = false;
	}
	fgResetWorkIdle = true;
}

static void enter_PRE_POWER_OFF(struct FsmEntity *fsm, struct FsmState *prev)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	if (prev == &RFSM_POWER_OFF || fsm->fgPowerOff) {
		/* driver abnormal probe in POWER_OFF state
		 * or user request to power off while reset is doing
		 */
		resetkoStartTimer(fsm, WAIT_ALL_MODULE_READY_TIMEOUT);
		wakeupSourceStayAwake(fsm);
		resetkoNotifyEvent(fsm, MODULE_NOTIFY_PRE_FUNCTION_OFF);
	}
	fsm->fgReady = false;
	fsm->fgPowerOff = true;
}
static void leave_PRE_POWER_OFF(struct FsmEntity *fsm, struct FsmState *next)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	if (next != &RFSM_PRE_RESET) {
		resetkoCancleTimer(fsm);
		fsm->fgReady = false;
	}
}

static void enter_POWER_OFF(struct FsmEntity *fsm, struct FsmState *prev)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	wakeupSourceRelax(fsm);
	if (isAllModuleInState(fsm->dongle_id, &RFSM_POWER_OFF)) {
		MR_Info("[%s_%d] all module in POWER_OFF state, hw power off\n",
			fsm->name, fsm->dongle_id);
		powerOff(fsm->dongle_id);
	}
}

static void leave_POWER_OFF(struct FsmEntity *fsm, struct FsmState *next)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	wakeupSourceStayAwake(fsm);
	if (isAllModuleInState(fsm->dongle_id, &RFSM_POWER_OFF)) {
		MR_Info("[%s_%d] all module in POWER_OFF state, hw power on\n",
			fsm->name, fsm->dongle_id);
		powerOn(fsm->dongle_id);
	}
	fsm->fgPowerOff = false;
}

static bool gaurd_isAllModuleReady(struct FsmEntity *fsm)
{
	bool ret = guard_isResetWorkIdle(fsm) &&
		isAllModuleReady(fsm->dongle_id);

	MR_Info("[%s_%d] %s %s\n",
		fsm->name, fsm->dongle_id, __func__, ret ? "pass" : "fail");

	return ret;
}

static bool guard_isResetWorkIdle(struct FsmEntity *fsm)
{
	MR_Info("[%s] %s %s\n", fsm->name, __func__,
		fgResetWorkIdle ? "pass" : "fail");
	return fgResetWorkIdle;
}

static bool guard_isPendingPowerOff(struct FsmEntity *fsm)
{
	if (fsm->fgPowerOff)
		MR_Info("[%s] %s %s\n", fsm->name, __func__,
			fsm->fgPowerOff ? "pass" : "fail");
	return fsm->fgPowerOff;
}

static void action_RestartWaitprobeTimer(struct FsmEntity *fsm)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	resetkoStartTimer(fsm, WAIT_PROBE_TIMEOUT);
}

#if (RESETKO_WAIT_MODULE_PROBE_TIMEOUT == 0)
static void action_WaitProbeTimeout(struct FsmEntity *fsm)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	wakeupSourceRelax(fsm);
}
#endif

static void action_PowerReset(struct FsmEntity *fsm)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	fgResetWorkIdle = false;
	/* clear module ready flag when reset action is triggered */
	fsm->fgReady = false;
	powerReset(fsm->dongle_id);
}

static void action_PendingPowerOff(struct FsmEntity *fsm)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	fsm->fgPowerOff = true;
}

static void action_ClearPendingPowerOff(struct FsmEntity *fsm)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	fsm->fgPowerOff = false;
}

static void action_NotifyFunctionOff(struct FsmEntity *fsm)
{
	MR_Info("[%s_%d] %s\n", fsm->name, fsm->dongle_id, __func__);
	resetkoNotifyEvent(fsm, MODULE_NOTIFY_PRE_FUNCTION_OFF);
}

struct FsmEntity *allocResetFsm(uint32_t dongle_id,
				char *name, enum ModuleType eModuleType)
{
	struct FsmEntity *fsm = allocFsmEntity(dongle_id, name, eModuleType);

	if (!fsm)
		return NULL;
#if CFG_RESETKO_CONN_DYNAMIC_POWER_CTRL
	/* driver fsm init stat is POWER_OFF.
	 * Driver will do wlanProbe after insmod wifi ko.
	 */
	fsm->fsmState = &RFSM_POWER_OFF;
#else
	fsm->fsmState = &RFSM_POWER_UP;
#endif

	return fsm;
}

void freeResetFsm(struct FsmEntity *fsm)
{
	if (fsm)
		freeFsmEntity(fsm);
}

void resetFsmHandlevent(struct FsmEntity *fsm, enum ResetFsmEvent event)
{
	if ((fsm != NULL) && ((unsigned int)event < RFSM_EVENT_MAX))
		RFSM_handle_event(fsm, (unsigned int)event);
}


