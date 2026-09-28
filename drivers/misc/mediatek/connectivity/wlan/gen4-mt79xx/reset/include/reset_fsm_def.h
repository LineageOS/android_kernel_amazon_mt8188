/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   reset_fsm_def.h
*   \brief  reset_fsm_def.h
*
*
*/


/**********************************************************************
*                         C O M P I L E R   F L A G S
***********************************************************************
*/
#ifndef _RESET_FSM_DEF_H
#define _RESET_FSM_DEF_H

/**********************************************************************
*                    E X T E R N A L   R E F E R E N C E S
***********************************************************************
*/
#include "reset_fsm.h"

/**********************************************************************
*                                 M A C R O S
***********************************************************************
*/
#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC 1000
#endif

#define WAIT_PROBE_TIMEOUT		(20 * MSEC_PER_SEC)
#define WAIT_ALL_MODULE_READY_TIMEOUT	(5 * MSEC_PER_SEC)

/**********************************************************************
*                              C O N S T A N T S
***********************************************************************
*/


/**********************************************************************
*                             D A T A   T Y P E S
***********************************************************************
*/
enum ResetFsmEvent {
	/* module probe function done and success */
	RFSM_EVENT_PROBED = 0,
	/* module remove function done and success */
	RFSM_EVENT_REMOVED = 1,
	/* internal: timeout event */
	RFSM_EVENT_TIMEOUT = 2,
	/* module ready for chip reset or power off */
	RFSM_EVENT_READY = 3,
	/* module request trigger reset */
	RFSM_EVENT_TRIGGER_RESET = 4,
	/* internal: reset action done */
	RFSM_EVENT_RESET_DONE = 5,
	/* internal: power off flow */
	RFSM_EVENT_TRIGGER_POWER_OFF = 6,
	RFSM_EVENT_POWER_OFF_DONE = 7,
	RFSM_EVENT_TRIGGER_POWER_ON = 8,
	RFSM_EVENT_POWER_ON_DONE = 9,

	/* module start probe function */
	RFSM_EVENT_START_PROBE = 10,
	/*internal: chip rst*/
	RFSM_EVENT_RESET_OFF_DONE = 11,

	RFSM_EVENT_All,
	RFSM_EVENT_MAX = RFSM_EVENT_All,

	/* do not use follow event in new code */
	RFSM_EVENT_PROBE_START		= RFSM_EVENT_START_PROBE,
	RFSM_EVENT_PROBE_FAIL		= RFSM_EVENT_MAX + 1,  // ignore
	RFSM_EVENT_PROBE_SUCCESS	= RFSM_EVENT_PROBED,
	RFSM_EVENT_REMOVE		= RFSM_EVENT_REMOVED,
	RFSM_EVENT_L0_RESET_READY	= RFSM_EVENT_READY,
};

enum ModuleNotifyEvent {
	MODULE_NOTIFY_MESSAGE = 0,

	/* for chip reset, wait module ready */
	MODULE_NOTIFY_PRE_POWER_OFF = 1,
	/* for chip reset, notify module reset done */
	MODULE_NOTIFY_RESET_DONE = 2,
	/* for power off flow */
	MODULE_NOTIFY_PRE_FUNCTION_OFF = 3,
	MODULE_NOTIFY_POWER_OFF_DONE = 4,
	MODULE_NOTIFY_POWER_ON_DONE = 5,

	MODULE_NOTIFY_MAX,

	/* do not use follow event in new code */
	MODULE_NOTIFY_PRE_RESET		= MODULE_NOTIFY_PRE_POWER_OFF,
};

/**********************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
***********************************************************************
*/
/* functions in reset_fsm_def.c */
struct FsmEntity *allocResetFsm(uint32_t dongle_id,
				char *name, enum ModuleType eModuleType);
void freeResetFsm(struct FsmEntity *fsm);

void resetFsmHandlevent(struct FsmEntity *fsm, enum ResetFsmEvent event);

/* functions in reset.c */
bool findBusIdByDongleId(uint32_t dongle_id, uint32_t *bus_id);

void resetkoNotifyEvent(struct FsmEntity *fsm, enum ModuleNotifyEvent event);

void wakeupSourceStayAwake(struct FsmEntity *fsm);
void wakeupSourceRelax(struct FsmEntity *fsm);

void clearAllModuleReady(uint32_t dongle_id);
bool isAllModuleReady(uint32_t dongle_id);
bool isAllModuleInState(uint32_t dongle_id, const struct FsmState *state);

void resetkoStartTimer(struct FsmEntity *fsm, unsigned int ms);
void resetkoCancleTimer(struct FsmEntity *fsm);

void powerOff(uint32_t dongle_id);
void powerOn(uint32_t dongle_id);
void powerReset(uint32_t dongle_id);

/**********************************************************************
*                            P U B L I C   D A T A
***********************************************************************
*/


/**********************************************************************
*                           P R I V A T E   D A T A
***********************************************************************
*/


/**********************************************************************
*                              F U N C T I O N S
**********************************************************************/


#endif  /* _RESET_FSM_DEF_H */

