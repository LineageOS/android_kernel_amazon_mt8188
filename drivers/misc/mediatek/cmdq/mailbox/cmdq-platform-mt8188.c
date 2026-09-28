// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <dt-bindings/gce/mt8188-gce.h>

#include "cmdq-util.h"

#define GCE_M_PA	0x10320000
#define GCE_D_PA	0x10330000

const char *cmdq_thread_module_dispatch(phys_addr_t gce_pa, s32 thread)
{
	if (gce_pa == GCE_M_PA) {
		switch (thread) {
		case 0 ... 7:
			return "DISP";
		case 15:
			return "CMDQ_SEC";
		case 13 ... 14:
		case 16:
		case 21 ... 22:
			return "MDP";
		case 23:
			return "CMDQ_TEST";
		default:
			return "CMDQ";
		}
	} else if (gce_pa == GCE_D_PA) {
		switch (thread) {
		case 0 ... 6:
			return "VDOSYS";
		case 15:
			return "CMDQ_SEC";
		case 16 ... 18:
			return "VCU";
		case 22:
			return "CMDQ_BW";
		case 23:
			return "CMDQ_TEST";
		default:
			return "CMDQ";
		}
	}

	return "CMDQ";
}

const char *cmdq_event_module_dispatch(phys_addr_t gce_pa, const u16 event,
	s32 thread)
{
	if (1) {
		switch (event) {
		case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
		case CMDQ_SYNC_TOKEN_STREAM_EOF:
		case CMDQ_SYNC_TOKEN_ESD_EOF:
		case CMDQ_SYNC_TOKEN_STREAM_BLOCK:
		case CMDQ_SYNC_TOKEN_CABC_EOF:
			return "DISP";
		case CMDQ_SYNC_TOKEN_MSS:
			return "MSS";
		case CMDQ_SYNC_TOKEN_MSF:
			return "MSF";

		case CMDQ_EVENT_TPR_TIMEOUT_0 ... CMDQ_EVENT_TPR_TIMEOUT_15:
			return cmdq_thread_module_dispatch(gce_pa, thread);
		}

		switch (event) {
		case CMDQ_EVENT_IMG_TRAW0_CQ_THR_DONE_0
			... CMDQ_EVENT_IMG_TRAW0_DMA_ERROR_INT:
			return "TRAW";

		case CMDQ_EVENT_IMG_TRAW1_CQ_THR_DONE_0
			... CMDQ_EVENT_IMG_TRAW1_DMA_ERROR_INT:
			return "LTRAW";

		case CMDQ_EVENT_IMG_SOF:
			return "IMGSYS";

		case CMDQ_EVENT_IMG_DIP_CQ_THR_DONE_0
			... CMDQ_EVENT_IMG_DIP_DMA_ERR:
			return "DIP";

		case CMDQ_EVENT_IMG_WPE_EIS_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_EIS_CQ_THR_DONE_9:
			return "WPE_EIS";

		case CMDQ_EVENT_IMG_PQDIP_A_CQ_THR_DONE_0
			... CMDQ_EVENT_IMG_PQDIP_A_DMA_ERR:
			return "PQDIP_A";

		case CMDQ_EVENT_IMG_PQDIP_B_CQ_THR_DONE_0
			... CMDQ_EVENT_IMG_PQDIP_B_DMA_ERR:
			return "PQDIP_B";

		case CMDQ_EVENT_IMG_WPE_TNR_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_TNR_CQ_THR_DONE_9:
			return "WPE_TNR";

		case CMDQ_EVENT_IMG_WPE_LITE_GCE_FRAME_DONE
			... CMDQ_EVENT_IMG_WPE_LITE_CQ_THR_DONE_9:
			return "WPE_LITE";

		case CMDQ_EVENT_IMG_XTRAW_CQ_THR_DONE_0
			... CMDQ_EVENT_IMG_XTRAW_CQ_THR_DONE_9:
			return "XTRAW";

		case CMDQ_EVENT_IMG_IMGSYS_IPE_ME_DONE:
			return "ME";

		case CMDQ_EVENT_VPP0_DISP_COLOR_SOF:
		case CMDQ_EVENT_VPP0_DISP_OVL_NOAFBC_SOF:
		case CMDQ_EVENT_VPP0_DISP_RDMA_SOF:
		case CMDQ_EVENT_VPP0_DISP_WDMA_SOF:
		case CMDQ_EVENT_VPP0_DISP_COLOR_FRAME_DONE:
		case CMDQ_EVENT_VPP0_DISP_OVL_NOAFBC_FRAME_DONE:
		case CMDQ_EVENT_VPP0_DISP_RDMA_FRAME_DONE:
		case CMDQ_EVENT_VPP0_DISP_WDMA_FRAME_DONE:
		case CMDQ_EVENT_VPP0_DISP_MUTEX_STREAM_DONE_0
			... CMDQ_EVENT_VPP0_DISP_MUTEX_STREAM_DONE_15:
		case CMDQ_EVENT_VPP0_DISP_OVL_NOAFBC_FRAME_RESET_DONE_PULSE:
		case CMDQ_EVENT_VPP0_DISP_OVL_NOAFBC_TARGET_MATCH_0
			... CMDQ_EVENT_VPP0_DISP_WDMA_WDMA_TARGET_LINE_EVENT:
		case CMDQ_EVENT_VDO0_DISP_OVL0_SOF
			... CMDQ_EVENT_VDO0_MDP_WROT0_O_SW_RST_DONE:
			return "DISP";

		case CMDQ_EVENT_VPP0_MDP_RDMA_SW_RST_DONE:
		case CMDQ_EVENT_VPP0_MDP_RDMA_PM_VALID_EVENT:
		case CMDQ_EVENT_VPP0_MDP_WROT_SW_RST_DONE:
		case CMDQ_EVENT_VPP1_SVPP1_MDP_TCC_SOF
			... CMDQ_EVENT_VPP1_MUTEX_BUF_UNDERRUN_GCE_EVENT_15:
		case CMDQ_EVENT_VPP1_SVPP3_MDP_WROT_SW_RST_DONE_GCE_EVENT
			... CMDQ_EVENT_VPP1_SVPP1_MDP_OVL_NEW_EVENT_1:
			return "MDP";

		case CMDQ_EVENT_VDEC_SOC_EVENT_0
			... CMDQ_EVENT_VDEC_CORE0_EVENT_15:
			return "VDEC";

		case CMDQ_EVENT_VENC_TOP_VENC_FRAME_DONE
			... CMDQ_EVENT_VENC_TOP_VPS_HEADER_DONE:
			return "VENC";

		default:
			return "CMDQ";
		}
	}
	return "CMDQ";
}
EXPORT_SYMBOL(cmdq_event_module_dispatch);

u32 cmdq_get_backup_event_list(phys_addr_t gce_pa, struct cmdq_backup_event_list **list_out)
{
	u32 array_size = 0;
	static struct cmdq_backup_event_list backup_event_list_d = {
		0,
		{ {0, 0} }
	};
	static struct cmdq_backup_event_list backup_event_list_m = {
		0,
		{ {0, 0} }
	};

	if (gce_pa == GCE_D_PA) {
		/* config the sw token need to backup and restore in GCE-D */
		static struct cmdq_backup_event backup_event_d[] = {
			{0, 0}
		};

		backup_event_list_d.size = ARRAY_SIZE(backup_event_d);
		memcpy(backup_event_list_d.backup_event, backup_event_d,
			sizeof(backup_event_d[0])*backup_event_list_d.size);

		*list_out = &backup_event_list_d;
		array_size = backup_event_list_d.size;

		/* no backup event is set in GCE-D currently yet, but ARRAY_SIZE = 1 */
		if ((array_size == 1) &&
			(backup_event_list_d.backup_event[0].event_id == 0) &&
			(backup_event_list_d.backup_event[0].value == 0))
			array_size = 0;
	} else if (gce_pa == GCE_M_PA) {
		/* config the sw token need to backup and restore in GCE-M */
		static struct cmdq_backup_event backup_event_m[] = {
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_1, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_2, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_3, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_4, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_5, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_6, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_7, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_8, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_9, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_10, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_11, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_12, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_13, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_14, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_15, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_16, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_17, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_18, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_19, 0},
			{CMDQ_SYNC_TOKEN_IMGSYS_POOL_20, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_1, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_2, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_3, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_4, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_5, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_6, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_7, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_8, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_9, 0},
			{CMDQ_SYNC_TOKEN_CAMSYS_POOL_10, 0},
		};

		backup_event_list_m.size = ARRAY_SIZE(backup_event_m);
		memcpy(backup_event_list_m.backup_event, backup_event_m,
			sizeof(backup_event_m[0])*backup_event_list_m.size);

		*list_out = &backup_event_list_m;
		array_size = backup_event_list_m.size;
	} else {
		static struct cmdq_backup_event_list backup_event_list_null = {
			0,
			{ {0, 0} }
		};

		*list_out = &backup_event_list_null;
		array_size = 0;

		cmdq_err("unknown pa=0x%llx", gce_pa);
	}

	return array_size;
}

u32 cmdq_util_hw_id(u32 pa)
{
	switch (pa) {
	case GCE_M_PA:
		return 0;
	case GCE_D_PA:
		return 1;
	default:
		cmdq_err("unknown addr:%x", pa);
	}

	return 0;
}

u16 cmdq_util_get_event_id(u16 event_id)
{
	switch (event_id) {
	/* MDP start of frame */
	case CMDQ_EVENT_MDP_RDMA0_SOF:
	return CMDQ_EVENT_VPP0_MDP_RDMA_SOF;
	case CMDQ_EVENT_MDP_RDMA1_SOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_RDMA_SOF;
	case CMDQ_EVENT_MDP_RDMA2_SOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_RDMA_SOF;
	case CMDQ_EVENT_MDP_RDMA3_SOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_RDMA_SOF;
	case CMDQ_EVENT_MDP_RSZ0_SOF:
	return CMDQ_EVENT_VPP0_MDP_RSZ_IN_RSZ_SOF;
	case CMDQ_EVENT_MDP_RSZ1_SOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_RSZ_SOF;
	case CMDQ_EVENT_MDP_RSZ2_SOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_RSZ_SOF;
	case CMDQ_EVENT_MDP_RSZ3_SOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_RSZ_SOF;
	case CMDQ_EVENT_MDP_TDSHP_SOF:
	return CMDQ_EVENT_VPP0_MDP_TDSHP_SOF;
	case CMDQ_EVENT_MDP_TDSHP1_SOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_TDSHP_SOF;
	case CMDQ_EVENT_MDP_TDSHP2_SOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_TDSHP_SOF;
	case CMDQ_EVENT_MDP_TDSHP3_SOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_TDSHP_SOF;
	case CMDQ_EVENT_MDP_WROT0_SOF:
	return CMDQ_EVENT_VPP0_MDP_WROT_SOF;
	case CMDQ_EVENT_MDP_WROT1_SOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_WROT_SOF;
	case CMDQ_EVENT_MDP_WROT2_SOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_WROT_SOF;
	case CMDQ_EVENT_MDP_WROT3_SOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_WROT_SOF;

	/* MDP frame done */
	case CMDQ_EVENT_MDP_RDMA0_EOF:
	return CMDQ_EVENT_VPP0_MDP_RDMA_FRAME_DONE;
	case CMDQ_EVENT_MDP_RDMA1_EOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_RDMA_FRAME_DONE;
	case CMDQ_EVENT_MDP_RDMA2_EOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_RDMA_FRAME_DONE;
	case CMDQ_EVENT_MDP_RDMA3_EOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_RDMA_FRAME_DONE;
	case CMDQ_EVENT_MDP_RSZ0_EOF:
	return CMDQ_EVENT_VPP0_MDP_RSZ_FRAME_DONE;
	case CMDQ_EVENT_MDP_RSZ1_EOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_RSZ_FRAME_DONE;
	case CMDQ_EVENT_MDP_RSZ2_EOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_RSZ_FRAME_DONE;
	case CMDQ_EVENT_MDP_RSZ3_EOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_RSZ_FRAME_DONE;
	case CMDQ_EVENT_MDP_TDSHP_EOF:
	return CMDQ_EVENT_VPP0_MDP_TDSHP_FRAME_DONE;
	case CMDQ_EVENT_MDP_WROT0_WRITE_EOF:
	return CMDQ_EVENT_VPP0_MDP_WROT_VIDO_WDONE;
	case CMDQ_EVENT_MDP_WROT1_WRITE_EOF:
	return CMDQ_EVENT_VPP1_SVPP1_MDP_WROT_FRAME_DONE;
	case CMDQ_EVENT_MDP_WROT2_WRITE_EOF:
	return CMDQ_EVENT_VPP1_SVPP2_MDP_WROT_FRAME_DONE;
	case CMDQ_EVENT_MDP_WROT3_WRITE_EOF:
	return CMDQ_EVENT_VPP1_SVPP3_MDP_WROT_FRAME_DONE;

	/* DISP frame done */
	case CMDQ_EVENT_DISP_RDMA0_EOF:
	return CMDQ_EVENT_VDO0_DISP_RDMA0_FRAME_DONE;

	/* WPE frame done */
	case CMDQ_EVENT_WPE_A_EOF:
	return CMDQ_EVENT_WPE_VPP0_WPE_GCE_FRAME_DONE;

	/* GCE handshake event */
	case CMDQ_EVENT_HANDSHAKE:
	return CMDQ_EVENT_HANDSHAKE_0;

	/* GPR timer token (for gpr r0 to r15) */
	case CMDQ_EVENT_GPR_TIMER:
	return CMDQ_EVENT_TPR_TIMEOUT_0;

	/* Keep this at the end of HW events */
	case CMDQ_HW_EVENT_MAX:
	return CMDQ_MAX_HW_EVENT;

	/* ATF PREBUILT sw token */
	case CMDQ_TOKEN_PREBUILT_MDP_WAIT:
	return CMDQ_SYNC_TOKEN_PREBUILT_MDP_WAIT;
	case CMDQ_TOKEN_PREBUILT_MDP_SET:
	return CMDQ_SYNC_TOKEN_PREBUILT_MDP_SET;
	case CMDQ_TOKEN_PREBUILT_MDP_LOCK:
	return CMDQ_SYNC_TOKEN_PREBUILT_MDP_LOCK;
	case CMDQ_TOKEN_PREBUILT_MML_WAIT:
	return CMDQ_SYNC_TOKEN_PREBUILT_MML_WAIT;
	case CMDQ_TOKEN_PREBUILT_MML_SET:
	return CMDQ_SYNC_TOKEN_PREBUILT_MML_SET;
	case CMDQ_TOKEN_PREBUILT_MML_LOCK:
	return CMDQ_SYNC_TOKEN_PREBUILT_MML_LOCK;
	case CMDQ_TOKEN_PREBUILT_VFMT_WAIT:
	return CMDQ_SYNC_TOKEN_PREBUILT_VFMT_WAIT;
	case CMDQ_TOKEN_PREBUILT_VFMT_SET:
	return CMDQ_SYNC_TOKEN_PREBUILT_VFMT_SET;
	case CMDQ_TOKEN_PREBUILT_VFMT_LOCK:
	return CMDQ_SYNC_TOKEN_PREBUILT_VFMT_LOCK;
	case CMDQ_TOKEN_PREBUILT_DISP_WAIT:
	return CMDQ_SYNC_TOKEN_PREBUILT_DISP_WAIT;
	case CMDQ_TOKEN_PREBUILT_DISP_SET:
	return CMDQ_SYNC_TOKEN_PREBUILT_DISP_SET;
	case CMDQ_TOKEN_PREBUILT_DISP_LOCK:
	return CMDQ_SYNC_TOKEN_PREBUILT_DISP_LOCK;

	/* eof for cmdq secure thread */
	case CMDQ_TOKEN_SECURE_THR_EOF:
	return CMDQ_SYNC_TOKEN_SECURE_THR_EOF;

	/* Pass-2 notifies VENC frame is ready to be encoded */
	case CMDQ_TOKEN_VENC_INPUT_READY:
	return CMDQ_SYNC_TOKEN_VENC_INPUT_READY;

	/* VENC notifies Pass-2 encode done so next frame may start */
	case CMDQ_TOKEN_VENC_EOF:
	return CMDQ_SYNC_TOKEN_VENC_EOF;

	/* SW Sync Tokens (User-defined) */
	case CMDQ_TOKEN_USER_0:
	return CMDQ_SYNC_TOKEN_USER_0;
	case CMDQ_TOKEN_USER_1:
	return CMDQ_SYNC_TOKEN_USER_1;

	/* ISP Tokens */
	case CMDQ_TOKEN_MSS:
	return CMDQ_SYNC_TOKEN_MSS;
	case CMDQ_TOKEN_MSF:
	return CMDQ_SYNC_TOKEN_MSF;

	/* GPR access tokens (for hardware register backup)
	 * There are 15 32-bit GPR, 3 GPR form a set
	 * (64-bit for address, 32-bit for value)
	 */
	case CMDQ_TOKEN_GPR_SET_0:
	return CMDQ_SYNC_TOKEN_GPR_SET_0;
	case CMDQ_TOKEN_GPR_SET_1:
	return CMDQ_SYNC_TOKEN_GPR_SET_1;
	case CMDQ_TOKEN_GPR_SET_2:
	return CMDQ_SYNC_TOKEN_GPR_SET_2;
	case CMDQ_TOKEN_GPR_SET_3:
	return CMDQ_SYNC_TOKEN_GPR_SET_3;
	case CMDQ_TOKEN_GPR_SET_4:
	return CMDQ_SYNC_TOKEN_GPR_SET_4;

	/* Resource lock event to control resource in GCE thread */
	case CMDQ_TOKEN_RESOURCE_WROT0:
	return CMDQ_SYNC_RESOURCE_WROT0;
	case CMDQ_TOKEN_RESOURCE_WROT1:
	return CMDQ_SYNC_RESOURCE_WROT1;

	/* SW token for ATF display va */
	case CMDQ_TOKEN_DISP_VA_START:
	return CMDQ_SYNC_TOKEN_DISP_VA_START;
	case CMDQ_TOKEN_DISP_VA_END:
	return CMDQ_SYNC_TOKEN_DISP_VA_END;

	/* event id is 10 bit */
	case CMDQ_TOKEN_MAX:
	return CMDQ_EVENT_MAX;

	default:
		cmdq_log("unknown event num:%d", event_id);
	break;
	}

	return CMDQ_SYNC_TOKEN_INVALID;
}

u32 cmdq_test_get_subsys_list(struct cmdq_util_subsys **subsys_out)
{
	/*
	 * The member in each cmdq_util_subsys:
	 * 1st member should be base addr which can map to a subsys id.
	 * 2nd member should be 32bit writable HW register offset for cmdq-test.
	 * If the subsys addr can not be test due to any reason,
	 * just set offset=0x0000 to skip it.
	 */
	static struct cmdq_util_subsys subsys_list[] = {
		/* VPPSYS0 */
		{ 0x14000000, 0x9028 },
		{ 0x14010000, 0x0000 },
		{ 0x14020000, 0x0000 },

		/* VDOSYS0 */
		{ 0x1c000000, 0x0028 },
		{ 0x1c010000, 0x0000 },
		{ 0x1c020000, 0x0000 },

		/* VDOSYS1 */
		{ 0x1c100000, 0x0130 },
		{ 0x1c110000, 0x0000 },
		{ 0x1c120000, 0x0000 },

		/* VPPSYS1 */
		{ 0x14f00000, 0x0000 },
		{ 0x14f10000, 0x0000 },
		{ 0x14f20000, 0x0000 },

		/* VDEC_SOC */
		{ 0x18000000, 0x0000 },
		{ 0x18010000, 0x0000 },

		/* VDEC_CORE0 */
		{ 0x18020000, 0x0000 },

		/* VDEC_CORE1 */
		{ 0x18030000, 0x0000 },

		/* GCE0 */
		{ 0x10320000, 0x1108 }, //V

		/* GCE1 */
		{ 0x10330000, 0x1108 }, //V

		/* CAMSYS */
		{ 0x16000000, 0x0000 },
		{ 0x16010000, 0x0000 },

		/* WPESYS */
		{ 0x14e00000, 0x0000 },

		/* EARC_RX */
		{ 0x1c200000, 0x0000 },

		/* HDMI_TX */
		{ 0x1c300000, 0x0230 }, //V

		/* HDMI_RX */
		{ 0x1c400000, 0x0140 }, //V

		/* eDP_TX */
		{ 0x1c500000, 0x0000 },

		/* DP_TX */
		{ 0x1c600000, 0x0000 },
	};

	*subsys_out = subsys_list;
	return ARRAY_SIZE(subsys_list);
}

const char *cmdq_util_hw_name(void *chan)
{
	u32 hw_id = cmdq_util_hw_id((u32)cmdq_mbox_get_base_pa(chan));

	if (hw_id == 0)
		return "GCE-M";

	if (hw_id == 1)
		return "GCE-D";

	return "CMDQ";
}

bool cmdq_thread_ddr_module(const s32 thread)
{
	switch (thread) {
	case 0 ... 6:
	case 8 ... 9:
	case 15:
		return false;
	default:
		return true;
	}
}

struct cmdq_util_platform_fp platform_fp = {
	.thread_module_dispatch = cmdq_thread_module_dispatch,
	.event_module_dispatch = cmdq_event_module_dispatch,
	.util_hw_id = cmdq_util_hw_id,
	.util_get_event_id = cmdq_util_get_event_id,
	.test_get_subsys_list = cmdq_test_get_subsys_list,
	.get_backup_event_list = cmdq_get_backup_event_list,
	.util_hw_name = cmdq_util_hw_name,
	.thread_ddr_module = cmdq_thread_ddr_module,
};

static int __init cmdq_platform_init(void)
{
	cmdq_util_set_fp(&platform_fp);
	return 0;
}
module_init(cmdq_platform_init);

MODULE_LICENSE("GPL v2");


