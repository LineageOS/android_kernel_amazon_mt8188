/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDP_LARB_H__
#define __MDP_LARB_H__
void mdp_larb_init(void);
int mdp_larb_power_on(void);
int mdp_larb_power_off(void);
struct device *mdp_larb_get_dev(unsigned int larb_id);
#endif
