/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_MERGE_H__
#define __MTK_DISP_MERGE_H__

#define DISP_MERGE_ENABLE	0x000
#define DISP_MERGE_RESET	0x004
#define DISP_MERGE_CFG_0	0x010
#define DISP_MERGE_CFG_1	0x014
#define DISP_MERGE_CFG_4	0x020
#define DISP_MERGE_CFG_5	0x024
#define DISP_MERGE_CFG_8	0x030
#define DISP_MERGE_CFG_9	0x034
#define DISP_MERGE_CFG_10	0x038
	#define CFG_10_NO_SWAP 0
	#define CFG_10_RG_SWAP 1
	#define CFG_10_GB_SWAP 2
	#define CFG_10_BR_SWAP 4
	#define CFG_10_RG_GB_SWAP 8
	#define CFG_10_RB_GB_SWAP 10
#define DISP_MERGE_CFG_11	0x03C
#define DISP_MERGE_CFG_12	0x040
	#define CFG_10_10_1PI_1PO_BUF_MODE 5
	#define CFG_10_10_1PI_2PO_BUF_MODE 6
	#define CFG_10_10_2PI_1PO_BUF_MODE 7
	#define CFG_10_10_2PI_2PO_BUF_MODE 8
	#define CFG_11_10_1PI_2PO_MERGE    18
#define DISP_MERGE_CFG_13	0x044
#define DISP_MERGE_CFG_14	0x048
#define DISP_MERGE_CFG_15	0x04C
#define DISP_MERGE_CFG_17	0x054
#define DISP_MERGE_CFG_18	0x058
#define DISP_MERGE_CFG_19	0x05C
#define DISP_MERGE_CFG_20	0x060
#define DISP_MERGE_CFG_21	0x064
#define DISP_MERGE_CFG_22	0x068
#define DISP_MERGE_CFG_23	0x06C
#define DISP_MERGE_CFG_24	0x070
#define DISP_MERGE_CFG_25	0x074
#define DISP_MERGE_CFG_26	0x078
#define DISP_MERGE_CFG_27	0x07C
#define DISP_MERGE_CFG_28	0x080
#define DISP_MERGE_CFG_29	0x084
#define DISP_MERGE_PATG_0	0x200
#define DISP_MERGE_PATG_5	0x214
#define DISP_MERGE_PATG_9	0x224
#define DISP_MERGE_PATG_13	0x234
#define DISP_MERGE_PATG_17	0x244
#define DISP_MERGE_PATG_33	0x284
#define DISP_MERGE_PATG_37	0x294
#define DISP_MERGE_PATG_41	0x2A4
#define DISP_MERGE_PATG_45	0x2B4
#define DISP_MERGE_MUTE_0	0xF00

void mtk_merge_layer_config(struct mtk_plane_state *state,
					 struct cmdq_pkt *handle);

void mtk_merge_dump(struct mtk_ddp_comp *comp);

#endif


