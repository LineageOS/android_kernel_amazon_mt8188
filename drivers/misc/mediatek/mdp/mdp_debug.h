/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDP_DEBUG_H__
#define __MDP_DEBUG_H__
#include <linux/types.h>

struct mdp_debug_info {
	bool fs_record;
	int debug[10];
	u32 memset_input_buf;
	bool enable_crc;
	bool rdma_config_debug;
	bool fg_debug;
};

struct mdp_debug_info *mdp_debug_get_info(void);
void mdp_debug_printf_record(const char *print_msg, ...);

void mdp_debug_init(void);
void mdp_debug_exit(void);


#endif /* __MDP_DEBUG_H__ */
