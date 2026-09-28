/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/* \file   "reset_hif.h"
 * \brief  This file contains the declairation of reset module
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#ifndef _RESET_HIF_H
#define _RESET_HIF_H

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "reset.h"

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#ifndef CFG_CHIP_RESET_USE_MSTAR_GPIO_API
#define CFG_CHIP_RESET_USE_MSTAR_GPIO_API 0
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct ResetGpioInfo {
	bool flag_inited;
	unsigned int gpio_num;
	unsigned int default_level;
	unsigned int action_level;
	unsigned int invert_time;
};

struct PowerGpioInfo {
	bool flag_inited;
	unsigned int gpio_num;
	unsigned int switch_on_level;
	unsigned int switch_off_level;
};

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void resetHif_Init(uint32_t dongle_id, struct device_node *node);
void resetHif_Uninit(uint32_t dongle_id);

enum ReturnStatus resetHif_UpdateSdioHost(uint32_t dongle_id, void *info);
void resetHif_SdioRemoveHost(uint32_t dongle_id);
void resetHif_SdioAddHost(uint32_t dongle_id);
bool resetHif_isSdioAdded(uint32_t dongle_id);
enum ReturnStatus resetHif_UpdatePcieHost(uint32_t dongle_id, void *info);
void resetHif_PcieRemoveHost(uint32_t dongle_id);
void resetHif_PcieAddHost(uint32_t dongle_id);
bool resetHif_isPcieAdded(uint32_t dongle_id);
void resetHif_ResetGpioPull(uint32_t dongle_id);
void resetHif_ResetGpioRelease(uint32_t dongle_id);
bool resetHif_isResetGpioReleased(uint32_t dongle_id);

void resetHif_PowerGpioSwitchOn(uint32_t dongle_id);
void resetHif_PowerGpioSwitchOff(uint32_t dongle_id);
bool resetHif_isPowerSwitchOn(uint32_t dongle_id);

bool resetHif_isReuseSomeHifInfo(uint32_t id_1, uint32_t id_2);

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _RESET_HIF_H */

