/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDP_IOCTL_EX_H__
#define __MDP_IOCTL_EX_H__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include "cmdq_helper_ext.h"
extern int gMdpRegMSBSupport;

#define MAX_HANDLE_NUM 20

struct mdp_job_mapping {
	u64 id;
	struct cmdqRecStruct *job;
	struct list_head list_entry;

	struct dma_buf *dma_bufs[MAX_HANDLE_NUM];
	struct dma_buf_attachment *attaches[MAX_HANDLE_NUM];
	struct sg_table *sgts[MAX_HANDLE_NUM];

	int fds[MAX_HANDLE_NUM];
	unsigned long mvas[MAX_HANDLE_NUM];
	uint16_t engines[MAX_HANDLE_NUM];
	u32 handle_count;
	void *node;
};

s32 mdp_ioctl_async_exec(struct file *pf, unsigned long param);
s32 mdp_ioctl_async_wait(unsigned long param);
s32 mdp_ioctl_alloc_readback_slots(void *fp, unsigned long param);
s32 mdp_ioctl_free_readback_slots(void *fp, unsigned long param);
s32 mdp_ioctl_read_readback_slots(unsigned long param);
s32 mdp_ioctl_simulate(unsigned long param);
s32 mdp_ioctl_convert_sec_fd(void *fp, unsigned long param);
s32 mdp_ioctl_check_valid_fd(void *fp, unsigned long param);
void mdp_ioctl_free_job_by_node(void *node);
void mdp_ioctl_free_readback_slots_by_node(void *fp);
int mdp_limit_dev_create(struct platform_device *device);
void mdpsyscon_init(void);
void mdpsyscon_deinit(void);
u32 cmdq_mdp_convert_fd_to_sec_handle(int sec_fd);
int cmdq_mdp_check_fd_valid(int fd);


#endif				/* __MDP_IOCTL_EX_H__ */
